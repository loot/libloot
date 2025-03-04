mod conditions;
mod error;

use std::{collections::HashMap, path::Path};

use conditions::{evaluate_all_conditions, filter_map_on_condition};

use crate::{
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
            log::error!("The condition cache's lock is poisoned, assigning a new cache");
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
        overwrite: bool,
    ) -> Result<(), WriteMetadataError> {
        validate_write_path(output_path, overwrite)?;

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
        overwrite: bool,
    ) -> Result<(), WriteMetadataError> {
        validate_write_path(output_path, overwrite)?;

        let mut doc = MetadataDocument::default();

        for plugin in self.masterlist.plugins() {
            let mut minimal_plugin = PluginMetadata::new(plugin.name())
                .expect("Regex plugin name from existing PluginMetadata object is valid");
            minimal_plugin.set_tags(plugin.tags().to_vec());
            minimal_plugin.set_dirty_info(plugin.dirty_info().to_vec());

            doc.set_plugin_metadata(minimal_plugin);
        }

        doc.save(output_path)
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

fn validate_write_path(output_path: &Path, overwrite: bool) -> Result<(), WriteMetadataError> {
    if !output_path.parent().map(|p| p.exists()).unwrap_or(false) {
        Err(WriteMetadataError::new(
            output_path.into(),
            WriteMetadataErrorReason::ParentDirectoryNotFound,
        ))
    } else if !overwrite && output_path.exists() {
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
                    new_group = new_group.with_description(description.to_string());
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
