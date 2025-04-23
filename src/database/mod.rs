mod conditions;
mod error;

use std::{collections::HashMap, path::Path};

use conditions::{evaluate_all_conditions, evaluate_condition, filter_map_on_condition};

use crate::{
    logging,
    metadata::{
        Group, Message, PluginMetadata,
        error::{LoadMetadataError, WriteMetadataError, WriteMetadataErrorReason},
        metadata_document::MetadataDocument,
    },
    sorting::{
        error::GroupsPathError,
        groups::{build_groups_graph, find_path},
        vertex::Vertex,
    },
};
pub use error::{ConditionEvaluationError, MetadataRetrievalError};

/// Control behaviour when writing to files.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum WriteMode {
    /// Create the file if it does not exist, otherwise error.
    Create,
    /// Create the file if it does not exist, otherwise replace its contents.
    CreateOrTruncate,
}

/// The interface through which metadata can be accessed.
#[derive(Debug)]
pub struct Database {
    masterlist: MetadataDocument,
    userlist: MetadataDocument,
    condition_evaluator_state: loot_condition_interpreter::State,
}

impl Database {
    #[must_use]
    pub(crate) fn new(condition_evaluator_state: loot_condition_interpreter::State) -> Self {
        Self {
            masterlist: MetadataDocument::default(),
            userlist: MetadataDocument::default(),
            condition_evaluator_state,
        }
    }

    pub(crate) fn condition_evaluator_state_mut(
        &mut self,
    ) -> &mut loot_condition_interpreter::State {
        &mut self.condition_evaluator_state
    }

    pub(crate) fn clear_condition_cache(&mut self) {
        if let Err(e) = self.condition_evaluator_state.clear_condition_cache() {
            logging::error!("The condition cache's lock is poisoned, assigning a new cache");
            *e.into_inner() = HashMap::new();
        }
    }

    /// Loads the masterlist from the given path.
    ///
    /// Replaces any existing data that was previously loaded from a masterlist.
    pub fn load_masterlist(&mut self, path: &Path) -> Result<(), LoadMetadataError> {
        self.masterlist.load(path)
    }

    /// Loads the masterlist from the given path, using the prelude at the given
    /// path.
    ///
    /// Replaces any existing data that was previously loaded from a masterlist.
    pub fn load_masterlist_with_prelude(
        &mut self,
        masterlist_path: &Path,
        prelude_path: &Path,
    ) -> Result<(), LoadMetadataError> {
        self.masterlist
            .load_with_prelude(masterlist_path, prelude_path)
    }

    /// Loads the userlist from the given path.
    ///
    /// Replaces any existing data that was previously loaded from a userlist.
    pub fn load_userlist(&mut self, path: &Path) -> Result<(), LoadMetadataError> {
        self.userlist.load(path)
    }

    /// Writes a metadata file containing all loaded user-added metadata.
    ///
    /// If `output_path` already exists, it will be written if `overwrite` is
    /// `true`, otherwise no data will be written.
    pub fn write_user_metadata(
        &self,
        output_path: &Path,
        mode: WriteMode,
    ) -> Result<(), WriteMetadataError> {
        validate_write_path(output_path, mode)?;

        self.userlist.save(output_path)
    }

    /// Writes a metadata file that only contains plugin Bash Tag suggestions
    /// and dirty info.
    ///
    /// If `output_path` already exists, it will be written if `overwrite` is
    /// `true`, otherwise no data will be written.
    pub fn write_minimal_list(
        &self,
        output_path: &Path,
        mode: WriteMode,
    ) -> Result<(), WriteMetadataError> {
        validate_write_path(output_path, mode)?;

        let mut doc = MetadataDocument::default();

        for plugin in self.masterlist.plugins_iter() {
            let Ok(mut minimal_plugin) = PluginMetadata::new(plugin.name()) else {
                // This should never happen because the regex plugin name from
                // an existing PluginMetadata object should be valid.
                logging::error!(
                    "Unexpectedly encountered a regex error trying to create a PluginMetadata object with the name {}",
                    plugin.name()
                );
                continue;
            };
            minimal_plugin.set_tags(plugin.tags().to_vec());
            minimal_plugin.set_dirty_info(plugin.dirty_info().to_vec());

            doc.set_plugin_metadata(minimal_plugin);
        }

        doc.save(output_path)
    }

    /// Evaluate the given condition string.
    pub fn evaluate(&self, condition: &str) -> Result<bool, ConditionEvaluationError> {
        evaluate_condition(condition, &self.condition_evaluator_state).map_err(Into::into)
    }

    /// Gets the Bash Tags that are listed in the loaded metadata lists.
    ///
    /// Bash Tag suggestions can include Bash Tags not in this list.
    pub fn known_bash_tags(&self) -> Vec<String> {
        let mut tags = self.masterlist.bash_tags().to_vec();
        tags.extend_from_slice(self.userlist.bash_tags());

        tags
    }

    /// Get all general messages listed in the loaded metadata lists.
    ///
    /// If `evaluate_conditions` is `true`, any metadata conditions are
    /// evaluated before the metadata is returned, otherwise unevaluated
    /// metadata is returned. Evaluating general message conditions also clears
    /// the condition cache before evaluating conditions.
    pub fn general_messages(
        &mut self,
        evaluate_conditions: bool,
    ) -> Result<Vec<Message>, ConditionEvaluationError> {
        if evaluate_conditions {
            self.clear_condition_cache();
        }

        let messages_iter = self
            .masterlist
            .messages()
            .iter()
            .chain(self.userlist.messages());

        if evaluate_conditions {
            let messages = messages_iter
                .filter_map(|m| {
                    filter_map_on_condition(m, m.condition(), &self.condition_evaluator_state)
                })
                .collect::<Result<Vec<_>, _>>()?;

            Ok(messages)
        } else {
            Ok(messages_iter.cloned().collect())
        }
    }

    /// Gets the groups that are defined in the loaded metadata lists.
    ///
    /// If `include_user_metadata` is `true`, any group metadata present in the
    /// userlist is included in the returned metadata, otherwise the metadata
    /// returned only includes metadata from the masterlist.
    pub fn groups(&self, include_user_metadata: bool) -> Vec<Group> {
        if include_user_metadata {
            merge_groups(self.masterlist.groups(), self.userlist.groups())
        } else {
            self.masterlist.groups().to_vec()
        }
    }

    /// Gets the groups that are defined or extended in the loaded userlist.
    pub fn user_groups(&self) -> &[Group] {
        self.userlist.groups()
    }

    /// Sets the group definitions to store in the userlist, replacing any
    /// definitions already loaded from the userlist.
    pub fn set_user_groups(&mut self, groups: Vec<Group>) {
        self.userlist.set_groups(groups);
    }

    /// Get the "shortest" path between the two given groups according to their
    /// "load after" metadata.
    ///
    /// The "shortest" path is defined as the path that maximises the amount of
    /// user metadata involved while minimising the amount of masterlist
    /// metadata involved. It's not the path involving the fewest groups.
    ///
    /// If there is no path between the two groups, the returned [Vec] will be
    /// empty.
    pub fn groups_path(
        &self,
        from_group_name: &str,
        to_group_name: &str,
    ) -> Result<Vec<Vertex>, GroupsPathError> {
        let graph = build_groups_graph(self.masterlist.groups(), self.userlist.groups())?;

        let path = find_path(&graph, from_group_name, to_group_name)?;

        Ok(path)
    }

    /// Get all of a plugin's loaded metadata.
    ///
    /// If `include_user_metadata` is `true`, any user metadata the plugin has
    /// is included in the returned metadata, otherwise the metadata returned
    /// only includes metadata from the masterlist.
    ///
    /// If `evaluateConditions` is `true`, any metadata conditions are evaluated
    /// before the metadata otherwise unevaluated metadata is returned.
    /// Evaluating plugin metadata conditions does **not** clear the condition
    /// cache.
    pub fn plugin_metadata(
        &self,
        plugin_name: &str,
        include_user_metadata: bool,
        evaluate_conditions: bool,
    ) -> Result<Option<PluginMetadata>, MetadataRetrievalError> {
        let mut metadata = self.masterlist.find_plugin(plugin_name)?;

        if include_user_metadata {
            if let Some(mut user_metadata) = self.userlist.find_plugin(plugin_name)? {
                if let Some(metadata) = metadata {
                    user_metadata.merge_metadata(&metadata);
                }
                metadata = Some(user_metadata);
            }
        }

        if evaluate_conditions {
            if let Some(metadata) = metadata {
                return evaluate_all_conditions(metadata, &self.condition_evaluator_state)
                    .map_err(Into::into);
            }
        }

        Ok(metadata)
    }

    /// Get a plugin's metadata loaded from the given userlist.
    ///
    /// If `evaluateConditions` is `true`, any metadata conditions are evaluated
    /// before the metadata otherwise unevaluated metadata is returned.
    /// Evaluating plugin metadata conditions does **not** clear the condition
    /// cache.
    pub fn plugin_user_metadata(
        &self,
        plugin_name: &str,
        evaluate_conditions: bool,
    ) -> Result<Option<PluginMetadata>, MetadataRetrievalError> {
        let metadata = self.userlist.find_plugin(plugin_name)?;

        if evaluate_conditions {
            if let Some(metadata) = metadata {
                return evaluate_all_conditions(metadata, &self.condition_evaluator_state)
                    .map_err(Into::into);
            }
        }

        Ok(metadata)
    }

    /// Sets a plugin's user metadata, replacing any loaded user metadata for
    /// that plugin.
    pub fn set_plugin_user_metadata(&mut self, plugin_metadata: PluginMetadata) {
        self.userlist.set_plugin_metadata(plugin_metadata);
    }

    /// Discards all loaded user metadata for the plugin with the given
    /// filename.
    pub fn discard_plugin_user_metadata(&mut self, plugin: &str) {
        self.userlist.remove_plugin_metadata(plugin);
    }

    /// Discards all loaded user metadata for all groups, plugins, and any
    /// user-added general messages and known bash tags.
    pub fn discard_all_user_metadata(&mut self) {
        self.userlist.clear();
    }
}

fn validate_write_path(output_path: &Path, mode: WriteMode) -> Result<(), WriteMetadataError> {
    if !output_path.parent().is_some_and(Path::exists) {
        Err(WriteMetadataError::new(
            output_path.into(),
            WriteMetadataErrorReason::ParentDirectoryNotFound,
        ))
    } else if mode == WriteMode::Create && output_path.exists() {
        Err(WriteMetadataError::new(
            output_path.into(),
            WriteMetadataErrorReason::PathAlreadyExists,
        ))
    } else {
        Ok(())
    }
}

fn merge_groups(lhs: &[Group], rhs: &[Group]) -> Vec<Group> {
    let mut groups = lhs.to_vec();

    let mut new_groups = Vec::new();

    for rhs_group in rhs {
        if let Some(group) = groups.iter_mut().find(|g| g.name() == rhs_group.name()) {
            if rhs_group.description().is_some() || !rhs_group.after_groups().is_empty() {
                let mut new_group = group.clone();

                if let Some(description) = rhs_group.description() {
                    new_group = new_group.with_description(description.to_owned());
                }

                if !rhs_group.after_groups().is_empty() {
                    let mut after_groups = new_group.after_groups().to_vec();
                    after_groups.extend_from_slice(rhs_group.after_groups());

                    new_group = new_group.with_after_groups(after_groups);
                }

                *group = new_group;
            }
        } else {
            new_groups.push(rhs_group.clone());
        }
    }

    groups.extend(new_groups);

    groups
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use crate::{
        EdgeType, GameType,
        metadata::{File, MessageType},
        tests::{BLANK_DIFFERENT_ESM, BLANK_ESM, BLANK_MASTER_DEPENDENT_ESM},
    };

    use super::*;

    struct Fixture {
        inner: crate::tests::Fixture,
        prelude_path: PathBuf,
        metadata_path: PathBuf,
    }

    impl Fixture {
        fn new(game_type: GameType) -> Self {
            let inner = crate::tests::Fixture::new(game_type);

            let prelude = "- &preludeBashTag Actors.ACBS";
            let prelude_path = inner.local_path.join("prelude.yaml");
            std::fs::write(&prelude_path, prelude).unwrap();

            let metadata = "
prelude:
  - &preludeBashTag C.Climate
bash_tags:
  - *preludeBashTag
globals:
  - type: say
    content: 'A general message'
    condition: 'file(\"missing.esp\")'
groups:
  - name: group1
  - name: group2
    after:
      - group1
plugins:
  - name: Blank.esm
    after:
      - Oblivion.esm
    msg:
      - type: say
        content: 'A note message'
        condition: 'file(\"missing.esp\")'
    tag:
      - Actors.ACBS
      - Actors.AIData
      - '-C.Water'
  - name: Blank - Different.esm
    after:
      - Blank - Master Dependent.esm
    msg:
      - type: warn
        content: 'A warning message'
    dirty:
      - crc: 0x7d22f9df
        util: TES4Edit
        udr: 4
  - name: Blank - Different.esp
    after:
      - Blank - Plugin Dependent.esp
    msg:
      - type: error
        content: 'An error message'
  - name: Blank.esp
    after:
      - Blank - Different Master Dependent.esp
  - name: Blank - Different Master Dependent.esp
    after:
      - Blank - Master Dependent.esp
    msg:
      - type: say
        content: 'A note message'
      - type: warn
        content: 'A warning message'
      - type: error
        content: 'An error message'";
            let metadata_path = inner.local_path.join("metadata.yaml");
            std::fs::write(&metadata_path, metadata).unwrap();

            Self {
                inner,
                prelude_path,
                metadata_path,
            }
        }

        fn database(&self) -> Database {
            Database::new(loot_condition_interpreter::State::new(
                self.inner.game_type.into(),
                self.inner.data_path(),
            ))
        }
    }

    #[test]
    fn load_masterlist_should_succeed_if_given_a_valid_path() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_masterlist(&fixture.metadata_path).unwrap();

        assert_eq!(&["C.Climate"], database.known_bash_tags().as_slice());
    }

    #[test]
    fn load_masterlist_with_prelude_should_succeed_if_given_valid_paths() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database
            .load_masterlist_with_prelude(&fixture.metadata_path, &fixture.prelude_path)
            .unwrap();

        assert_eq!(&["Actors.ACBS"], database.known_bash_tags().as_slice());
    }

    #[test]
    fn load_userlist_should_succeed_if_given_a_valid_path() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_userlist(&fixture.metadata_path).unwrap();

        assert_eq!(&["C.Climate"], database.known_bash_tags().as_slice());
        assert_eq!(
            &[
                Group::default(),
                Group::new("group1".into()),
                Group::new("group2".into()).with_after_groups(vec!["group1".into()])
            ],
            database.user_groups()
        );
    }

    mod write_user_metadata {
        use super::*;

        #[test]
        fn should_write_only_user_metadata() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            database.set_user_groups(vec![Group::new("group3".into())]);

            let output_path = fixture.inner.local_path.join("userlist.yaml");
            database
                .write_user_metadata(&output_path, WriteMode::Create)
                .unwrap();

            let content = std::fs::read_to_string(output_path).unwrap();

            assert_eq!("groups:\n  - name: 'default'\n  - name: 'group3'", content);
        }

        #[test]
        fn should_succeed_if_the_path_does_not_exist() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("userlist.yaml");

            assert!(
                database
                    .write_user_metadata(&output_path, WriteMode::Create)
                    .is_ok()
            );
        }

        #[test]
        fn should_succeed_if_the_path_does_not_exist_and_truncation_is_allowed() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("userlist.yaml");

            assert!(
                database
                    .write_user_metadata(&output_path, WriteMode::CreateOrTruncate)
                    .is_ok()
            );
        }

        #[test]
        fn should_succeed_if_the_path_exists_and_truncation_is_allowed() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("userlist.yaml");

            std::fs::File::create(&output_path).unwrap();

            assert!(
                database
                    .write_user_metadata(&output_path, WriteMode::CreateOrTruncate)
                    .is_ok()
            );
        }

        #[test]
        fn should_error_if_the_parent_path_does_not_exist() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("missing/userlist.yaml");

            assert!(
                database
                    .write_user_metadata(&output_path, WriteMode::Create)
                    .is_err()
            );
        }

        #[test]
        fn should_error_if_the_path_is_read_only() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("userlist.yaml");

            std::fs::File::create(&output_path).unwrap();

            let mut permissions = output_path.metadata().unwrap().permissions();
            permissions.set_readonly(true);
            std::fs::set_permissions(&output_path, permissions).unwrap();

            assert!(
                database
                    .write_user_metadata(&output_path, WriteMode::CreateOrTruncate)
                    .is_err()
            );
        }

        #[test]
        fn should_error_if_the_path_exists_and_truncation_is_not_allowed() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("userlist.yaml");

            std::fs::File::create(&output_path).unwrap();

            assert!(
                database
                    .write_user_metadata(&output_path, WriteMode::Create)
                    .is_err()
            );
        }
    }

    mod write_minimal_list {
        use super::*;

        #[test]
        fn should_only_write_plugin_bash_tags_and_dirty_info() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();
            let output_path = fixture.inner.local_path.join("minimal.yaml");

            database.load_masterlist(&fixture.metadata_path).unwrap();

            assert!(
                database
                    .write_minimal_list(&output_path, WriteMode::Create)
                    .is_ok()
            );

            let content = std::fs::read_to_string(output_path).unwrap();

            // Plugin entries are unordered.
            let expected_content = if content.find(BLANK_DIFFERENT_ESM) < content.find(BLANK_ESM) {
                "plugins:
  - name: 'Blank - Different.esm'
    dirty:
      - crc: 0x7D22F9DF
        util: 'TES4Edit'
        udr: 4
  - name: 'Blank.esm'
    tag:
      - Actors.ACBS
      - Actors.AIData
      - -C.Water"
            } else {
                "plugins:
  - name: 'Blank.esm'
    tag:
      - Actors.ACBS
      - Actors.AIData
      - -C.Water
  - name: 'Blank - Different.esm'
    dirty:
      - crc: 0x7D22F9DF
        util: 'TES4Edit'
        udr: 4"
            };

            assert_eq!(expected_content, content);
        }

        #[test]
        fn should_succeed_if_the_path_does_not_exist() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("minimal.yaml");

            assert!(
                database
                    .write_minimal_list(&output_path, WriteMode::Create)
                    .is_ok()
            );
        }

        #[test]
        fn should_succeed_if_the_path_does_not_exist_and_truncation_is_allowed() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("minimal.yaml");

            assert!(
                database
                    .write_minimal_list(&output_path, WriteMode::CreateOrTruncate)
                    .is_ok()
            );
        }

        #[test]
        fn should_succeed_if_the_path_exists_and_truncation_is_allowed() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("minimal.yaml");

            std::fs::File::create(&output_path).unwrap();

            assert!(
                database
                    .write_minimal_list(&output_path, WriteMode::CreateOrTruncate)
                    .is_ok()
            );
        }

        #[test]
        fn should_error_if_the_parent_path_does_not_exist() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("missing/minimal.yaml");

            assert!(
                database
                    .write_minimal_list(&output_path, WriteMode::Create)
                    .is_err()
            );
        }

        #[test]
        fn should_error_if_the_path_is_read_only() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("minimal.yaml");

            std::fs::File::create(&output_path).unwrap();

            let mut permissions = output_path.metadata().unwrap().permissions();
            permissions.set_readonly(true);
            std::fs::set_permissions(&output_path, permissions).unwrap();

            assert!(
                database
                    .write_minimal_list(&output_path, WriteMode::CreateOrTruncate)
                    .is_err()
            );
        }

        #[test]
        fn should_error_if_the_path_exists_and_truncation_is_not_allowed() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();
            let output_path = fixture.inner.local_path.join("minimal.yaml");

            std::fs::File::create(&output_path).unwrap();

            assert!(
                database
                    .write_minimal_list(&output_path, WriteMode::Create)
                    .is_err()
            );
        }
    }

    #[test]
    fn known_bash_tags_should_append_userlist_tags_to_masterlist_tags() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_masterlist(&fixture.metadata_path).unwrap();

        let userlist_path = fixture.inner.local_path.join("userlist.yaml");
        std::fs::write(&userlist_path, "bash_tags: [Relev, Delev]").unwrap();

        database.load_userlist(&userlist_path).unwrap();

        assert_eq!(
            vec!["C.Climate", "Relev", "Delev"],
            database.known_bash_tags()
        );
    }

    mod general_messages {
        use super::*;

        #[test]
        fn should_append_userlist_messages_to_masterlist_messages() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let userlist_path = fixture.inner.local_path.join("userlist.yaml");
            std::fs::write(
                &userlist_path,
                "globals: [{type: say, content: 'A user message'}]",
            )
            .unwrap();

            database.load_userlist(&userlist_path).unwrap();

            assert_eq!(
                &[
                    Message::new(MessageType::Say, "A general message".into())
                        .with_condition("file(\"missing.esp\")".into()),
                    Message::new(MessageType::Say, "A user message".into())
                ],
                database.general_messages(false).unwrap().as_slice()
            );
        }

        #[test]
        fn should_filter_out_messages_with_false_conditions_when_evaluating_conditions() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let userlist_path = fixture.inner.local_path.join("userlist.yaml");
            std::fs::write(
                &userlist_path,
                "globals: [{type: say, content: 'A user message'}]",
            )
            .unwrap();

            database.load_userlist(&userlist_path).unwrap();

            assert_eq!(
                &[Message::new(MessageType::Say, "A user message".into())],
                database.general_messages(true).unwrap().as_slice()
            );
        }
    }

    mod evaluate {
        use super::*;

        #[test]
        fn should_return_true_if_the_condition_is_true() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();

            assert!(database.evaluate("file(\"Blank.esp\")").unwrap());
        }

        #[test]
        fn should_return_false_if_the_condition_is_false() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();

            assert!(!database.evaluate("file(\"missing.esp\")").unwrap());
        }
    }

    mod groups {
        use super::*;

        #[test]
        fn should_return_default_group_before_metadata_has_been_loaded() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();

            assert_eq!(&[Group::default(),], database.groups(true).as_slice());
        }

        #[test]
        fn should_not_include_user_groups_if_param_is_false() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let userlist_path = fixture.inner.local_path.join("userlist.yaml");
            std::fs::write(
                &userlist_path,
                "groups: [{name: group2, after: [default]}, {name: group3, after: [group1]}]",
            )
            .unwrap();

            database.load_userlist(&userlist_path).unwrap();

            assert_eq!(
                &[
                    Group::default(),
                    Group::new("group1".into()),
                    Group::new("group2".into()).with_after_groups(vec!["group1".into()])
                ],
                database.groups(false).as_slice()
            );
        }

        #[test]
        fn should_merge_masterlist_and_userlist_groups() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let userlist_path = fixture.inner.local_path.join("userlist.yaml");
            std::fs::write(
                &userlist_path,
                "groups: [{name: group2, after: [default]}, {name: group3, after: [group1]}]",
            )
            .unwrap();

            database.load_userlist(&userlist_path).unwrap();

            assert_eq!(
                &[
                    Group::default(),
                    Group::new("group1".into()),
                    Group::new("group2".into())
                        .with_after_groups(vec!["group1".into(), "default".into()]),
                    Group::new("group3".into()).with_after_groups(vec!["group1".into()])
                ],
                database.groups(true).as_slice()
            );
        }
    }

    #[test]
    fn user_groups_should_not_include_masterlist_groups() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_masterlist(&fixture.metadata_path).unwrap();

        assert_eq!(&[Group::default(),], database.user_groups());
    }

    #[test]
    fn set_user_groups_should_replace_existing_user_groups() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_masterlist(&fixture.metadata_path).unwrap();

        let userlist_path = fixture.inner.local_path.join("userlist.yaml");
        std::fs::write(
            &userlist_path,
            "groups: [{name: group2, after: [default]}, {name: group3, after: [group1]}]",
        )
        .unwrap();

        database.load_userlist(&userlist_path).unwrap();

        database.set_user_groups(vec![Group::new("group4".into())]);

        assert_eq!(
            &[
                Group::default(),
                Group::new("group1".into()),
                Group::new("group2".into()).with_after_groups(vec!["group1".into()])
            ],
            database.groups(false).as_slice()
        );

        assert_eq!(
            &[Group::default(), Group::new("group4".into())],
            database.user_groups()
        );
    }

    #[test]
    fn groups_path_should_find_path_using_masterlist_and_user_metadata() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_masterlist(&fixture.metadata_path).unwrap();

        database.set_user_groups(vec![
            Group::new("group3".into()).with_after_groups(vec!["group2".into()]),
        ]);

        let path = database.groups_path("group1", "group3").unwrap();

        assert_eq!(
            vec![
                Vertex::new("group1".into()).with_out_edge_type(EdgeType::MasterlistLoadAfter),
                Vertex::new("group2".into()).with_out_edge_type(EdgeType::UserLoadAfter),
                Vertex::new("group3".into()),
            ],
            path
        );
    }

    mod plugin_metadata {
        use super::*;

        #[test]
        fn should_return_none_if_plugin_has_no_metadata_set() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();

            assert!(
                database
                    .plugin_metadata(BLANK_ESM, true, false)
                    .unwrap()
                    .is_none()
            );
        }

        #[test]
        fn should_return_none_if_plugin_metadata_has_only_name() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.set_plugin_user_metadata(PluginMetadata::new(BLANK_ESM).unwrap());

            assert!(
                database
                    .plugin_metadata(BLANK_ESM, true, false)
                    .unwrap()
                    .is_none()
            );
        }

        #[test]
        fn should_prefer_user_metadata_when_merging_metadata() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            database.set_plugin_user_metadata(plugin);

            assert_eq!(
                &[
                    File::new(BLANK_DIFFERENT_ESM.into()),
                    File::new("Oblivion.esm".into())
                ],
                database
                    .plugin_metadata(BLANK_ESM, true, false)
                    .unwrap()
                    .unwrap()
                    .load_after_files()
            );
        }

        #[test]
        fn should_return_only_masterlist_metadata_if_include_user_metadata_is_false() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            database.set_plugin_user_metadata(plugin);

            assert_eq!(
                &[File::new("Oblivion.esm".into())],
                database
                    .plugin_metadata(BLANK_ESM, false, false)
                    .unwrap()
                    .unwrap()
                    .load_after_files()
            );
        }

        #[test]
        fn should_filter_out_metadata_with_false_conditions_when_evaluating_conditions() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_messages(vec![
                Message::new(MessageType::Say, "content".into())
                    .with_condition("file(\"missing.esp\")".into()),
            ]);

            database.set_plugin_user_metadata(plugin);

            assert!(
                database
                    .plugin_metadata(BLANK_ESM, true, true)
                    .unwrap()
                    .unwrap()
                    .messages()
                    .is_empty()
            );
        }
    }

    mod plugin_user_metadata {
        use super::*;

        #[test]
        fn should_return_none_if_plugin_has_no_user_metadata_set() {
            let fixture = Fixture::new(GameType::Oblivion);
            let database = fixture.database();

            assert!(
                database
                    .plugin_user_metadata(BLANK_ESM, false)
                    .unwrap()
                    .is_none()
            );
        }

        #[test]
        fn should_return_none_if_plugin_user_metadata_has_only_name() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.set_plugin_user_metadata(PluginMetadata::new(BLANK_ESM).unwrap());

            assert!(
                database
                    .plugin_user_metadata(BLANK_ESM, false)
                    .unwrap()
                    .is_none()
            );
        }

        #[test]
        fn should_return_only_user_metadata() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            database.set_plugin_user_metadata(plugin);

            assert_eq!(
                &[File::new(BLANK_DIFFERENT_ESM.into())],
                database
                    .plugin_user_metadata(BLANK_ESM, false)
                    .unwrap()
                    .unwrap()
                    .load_after_files()
            );
        }

        #[test]
        fn should_filter_out_metadata_with_false_conditions_when_evaluating_conditions() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_load_after_files(vec![
                File::new(BLANK_DIFFERENT_ESM.into())
                    .with_condition("file(\"missing.esp\")".into()),
            ]);

            database.set_plugin_user_metadata(plugin);

            assert!(
                database
                    .plugin_user_metadata(BLANK_ESM, true)
                    .unwrap()
                    .is_none()
            );
        }
    }

    mod set_plugin_user_metadata {
        use super::*;

        #[test]
        fn should_replace_existing_user_metadata_for_the_plugin() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            database.set_plugin_user_metadata(plugin.clone());

            plugin.set_load_after_files(vec![File::new(BLANK_MASTER_DEPENDENT_ESM.into())]);

            database.set_plugin_user_metadata(plugin);

            assert_eq!(
                &[File::new(BLANK_MASTER_DEPENDENT_ESM.into())],
                database
                    .plugin_user_metadata(BLANK_ESM, false)
                    .unwrap()
                    .unwrap()
                    .load_after_files()
            );
        }

        #[test]
        fn should_not_modify_masterlist_metadata_for_the_plugin() {
            let fixture = Fixture::new(GameType::Oblivion);
            let mut database = fixture.database();

            database.load_masterlist(&fixture.metadata_path).unwrap();

            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            database.set_plugin_user_metadata(plugin);

            assert_eq!(
                &[
                    File::new(BLANK_DIFFERENT_ESM.into()),
                    File::new("Oblivion.esm".into()),
                ],
                database
                    .plugin_metadata(BLANK_ESM, true, false)
                    .unwrap()
                    .unwrap()
                    .load_after_files()
            );
        }
    }

    #[test]
    fn discard_plugin_user_metadata_should_discard_only_user_metadata_for_only_the_given_plugin() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_masterlist(&fixture.metadata_path).unwrap();

        let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
        plugin1.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

        let mut plugin2 = PluginMetadata::new(BLANK_DIFFERENT_ESM).unwrap();
        plugin2.set_load_after_files(vec![File::new(BLANK_ESM.into())]);

        database.set_plugin_user_metadata(plugin1);
        database.set_plugin_user_metadata(plugin2);

        database.discard_plugin_user_metadata(BLANK_ESM);

        assert_eq!(
            &[File::new("Oblivion.esm".into())],
            database
                .plugin_metadata(BLANK_ESM, true, false)
                .unwrap()
                .unwrap()
                .load_after_files()
        );
        assert_eq!(
            &[
                File::new(BLANK_ESM.into()),
                File::new(BLANK_MASTER_DEPENDENT_ESM.into()),
            ],
            database
                .plugin_metadata(BLANK_DIFFERENT_ESM, true, false)
                .unwrap()
                .unwrap()
                .load_after_files()
        );
    }

    #[test]
    fn discard_all_user_metadata_should_not_remove_masterlist_metadata() {
        let fixture = Fixture::new(GameType::Oblivion);
        let mut database = fixture.database();

        database.load_masterlist(&fixture.metadata_path).unwrap();

        let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
        plugin1.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

        let mut plugin2 = PluginMetadata::new(BLANK_DIFFERENT_ESM).unwrap();
        plugin2.set_load_after_files(vec![File::new(BLANK_ESM.into())]);

        database.set_user_groups(vec![Group::new("group4".into())]);
        database.set_plugin_user_metadata(plugin1);
        database.set_plugin_user_metadata(plugin2);

        database.discard_all_user_metadata();

        assert_eq!(
            &[
                Group::default(),
                Group::new("group1".into()),
                Group::new("group2".into()).with_after_groups(vec!["group1".into()])
            ],
            database.groups(true).as_slice()
        );
        assert_eq!(
            &[File::new("Oblivion.esm".into())],
            database
                .plugin_metadata(BLANK_ESM, true, false)
                .unwrap()
                .unwrap()
                .load_after_files()
        );
        assert_eq!(
            &[File::new(BLANK_MASTER_DEPENDENT_ESM.into()),],
            database
                .plugin_metadata(BLANK_DIFFERENT_ESM, true, false)
                .unwrap()
                .unwrap()
                .load_after_files()
        );
    }
}
