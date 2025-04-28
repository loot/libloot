use std::{
    collections::{HashMap, HashSet},
    path::Path,
};

use saphyr::{LoadableYamlNode, MarkedYaml, YamlData};

use crate::{escape_ascii, logging};

use super::{
    error::{
        ExpectedType, LoadMetadataError, MetadataDocumentParsingError, ParseMetadataError,
        RegexError, WriteMetadataError,
    },
    file::Filename,
    group::Group,
    message::Message,
    plugin_metadata::PluginMetadata,
    yaml::{EmitYaml, TryFromYaml, YamlEmitter, YamlObjectType, get_as_slice, process_merge_keys},
};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MetadataDocument {
    bash_tags: Vec<String>,
    groups: Vec<Group>,
    messages: Vec<Message>,
    plugins: HashMap<Filename, PluginMetadata>,
    regex_plugins: Vec<PluginMetadata>,
}

impl MetadataDocument {
    pub fn load(&mut self, file_path: &Path) -> Result<(), LoadMetadataError> {
        if !file_path.exists() {
            return Err(LoadMetadataError::new(
                file_path.into(),
                MetadataDocumentParsingError::PathNotFound,
            ));
        }

        logging::trace!("Loading file at \"{}\"", escape_ascii(file_path));

        let content = std::fs::read_to_string(file_path)
            .map_err(|e| LoadMetadataError::from_io_error(file_path.into(), e))?;

        self.load_from_str(&content)
            .map_err(|e| LoadMetadataError::new(file_path.into(), e))?;

        logging::trace!(
            "Successfully loaded metadata from file at \"{}\".",
            escape_ascii(file_path)
        );

        Ok(())
    }

    pub fn load_with_prelude(
        &mut self,
        masterlist_path: &Path,
        prelude_path: &Path,
    ) -> Result<(), LoadMetadataError> {
        if !masterlist_path.exists() {
            return Err(LoadMetadataError::new(
                masterlist_path.into(),
                MetadataDocumentParsingError::PathNotFound,
            ));
        }

        if !prelude_path.exists() {
            return Err(LoadMetadataError::new(
                prelude_path.into(),
                MetadataDocumentParsingError::PathNotFound,
            ));
        }

        let masterlist = std::fs::read_to_string(masterlist_path)
            .map_err(|e| LoadMetadataError::from_io_error(masterlist_path.into(), e))?;

        let prelude = std::fs::read_to_string(prelude_path)
            .map_err(|e| LoadMetadataError::from_io_error(masterlist_path.into(), e))?;

        let masterlist = replace_prelude(masterlist, &prelude);

        self.load_from_str(&masterlist)
            .map_err(|e| LoadMetadataError::new(masterlist_path.into(), e))?;

        logging::trace!(
            "Successfully loaded metadata from file at \"{}\".",
            escape_ascii(masterlist_path)
        );

        Ok(())
    }

    fn load_from_str(&mut self, string: &str) -> Result<(), MetadataDocumentParsingError> {
        let mut docs = MarkedYaml::load_from_str(string)?;

        let doc = docs
            .pop()
            .ok_or_else(|| MetadataDocumentParsingError::NoDocuments)?;
        if !docs.is_empty() {
            return Err(MetadataDocumentParsingError::MoreThanOneDocument(
                docs.len() + 1,
            ));
        }
        let doc = process_merge_keys(doc)?;

        let YamlData::Mapping(doc) = doc.data else {
            return Err(ParseMetadataError::unexpected_type(
                doc.span.start,
                YamlObjectType::MetadataDocument,
                ExpectedType::Map,
            )
            .into());
        };

        let mut plugins: HashMap<Filename, PluginMetadata> = HashMap::new();
        let mut regex_plugins: Vec<PluginMetadata> = Vec::new();
        for plugin_yaml in get_as_slice(&doc, "plugins", YamlObjectType::MetadataDocument)? {
            let plugin = PluginMetadata::try_from_yaml(plugin_yaml)?;
            if plugin.is_regex_plugin() {
                regex_plugins.push(plugin);
            } else {
                let filename = Filename::new(plugin.name().to_owned());
                if let Some(old) = plugins.insert(filename, plugin) {
                    return Err(ParseMetadataError::duplicate_entry(
                        plugin_yaml.span.start,
                        old.name().to_owned(),
                        YamlObjectType::PluginMetadata,
                    )
                    .into());
                }
            }
        }

        let messages = get_as_slice(&doc, "globals", YamlObjectType::MetadataDocument)?
            .iter()
            .map(Message::try_from_yaml)
            .collect::<Result<Vec<_>, _>>()?;

        let mut bash_tags = Vec::new();
        let mut str_set = HashSet::new();
        for bash_tag_yaml in get_as_slice(&doc, "bash_tags", YamlObjectType::MetadataDocument)? {
            let bash_tag: &str = match bash_tag_yaml.data.as_str() {
                Some(b) => b,
                None => {
                    return Err(ParseMetadataError::unexpected_type(
                        bash_tag_yaml.span.start,
                        YamlObjectType::BashTagsElement,
                        ExpectedType::String,
                    )
                    .into());
                }
            };

            if str_set.contains(bash_tag) {
                return Err(ParseMetadataError::duplicate_entry(
                    bash_tag_yaml.span.start,
                    bash_tag.to_owned(),
                    YamlObjectType::BashTagsElement,
                )
                .into());
            }

            bash_tags.push(bash_tag.to_owned());
            str_set.insert(bash_tag);
        }

        let mut group_names = HashSet::new();
        let mut groups = Vec::new();
        for group_yaml in get_as_slice(&doc, "groups", YamlObjectType::MetadataDocument)? {
            let group = Group::try_from_yaml(group_yaml)?;

            let name = group.name().to_owned();
            if group_names.contains(&name) {
                return Err(ParseMetadataError::duplicate_entry(
                    group_yaml.span.start,
                    group.name().to_owned(),
                    YamlObjectType::Group,
                )
                .into());
            }

            groups.push(group);
            group_names.insert(name);
        }

        if !group_names.contains(Group::DEFAULT_NAME) {
            groups.insert(0, Group::default());
        }

        self.plugins = plugins;
        self.regex_plugins = regex_plugins;
        self.messages = messages;
        self.bash_tags = bash_tags;
        self.groups = groups;

        Ok(())
    }

    pub fn save(&self, file_path: &Path) -> Result<(), WriteMetadataError> {
        logging::trace!("Saving metadata list to: \"{}\"", escape_ascii(file_path));

        let mut emitter = YamlEmitter::new();

        if !self.bash_tags.is_empty() {
            emitter.map_key("bash_tags");

            emitter.begin_array();

            for tag in &self.bash_tags {
                emitter.unquoted_str(tag);
            }

            emitter.end_array();
        }

        if self.groups.len() > 1 {
            emitter.map_key("groups");
            self.groups.emit_yaml(&mut emitter);
        }

        if !self.messages.is_empty() {
            emitter.map_key("globals");
            self.messages.emit_yaml(&mut emitter);
        }

        if !self.plugins.is_empty() || !self.regex_plugins.is_empty() {
            emitter.map_key("plugins");

            emitter.begin_array();

            for plugin in self.plugins_iter() {
                if !plugin.has_name_only() {
                    plugin.emit_yaml(&mut emitter);
                }
            }

            emitter.end_array();
        }

        let mut contents = emitter.into_string();
        if contents.is_empty() {
            contents = "{}".into();
        }

        std::fs::write(file_path, contents)
            .map_err(|e| WriteMetadataError::new(file_path.into(), e.into()))?;

        Ok(())
    }

    pub fn bash_tags(&self) -> &[String] {
        &self.bash_tags
    }

    pub fn groups(&self) -> &[Group] {
        &self.groups
    }

    pub fn messages(&self) -> &[Message] {
        &self.messages
    }

    pub fn plugins_iter(&self) -> impl Iterator<Item = &PluginMetadata> {
        self.plugins.values().chain(self.regex_plugins.iter())
    }

    pub fn find_plugin(&self, plugin_name: &str) -> Result<Option<PluginMetadata>, RegexError> {
        let mut metadata = match self.plugins.get(&Filename::new(plugin_name.to_owned())) {
            Some(m) => m.clone(),
            None => PluginMetadata::new(plugin_name)?,
        };

        // Now we want to also match possibly multiple regex entries.
        for regex_plugin in &self.regex_plugins {
            if regex_plugin.name_matches(plugin_name) {
                metadata.merge_metadata(regex_plugin);
            }
        }

        if metadata.has_name_only() {
            Ok(None)
        } else {
            Ok(Some(metadata))
        }
    }

    pub fn set_groups(&mut self, groups: Vec<Group>) {
        // Ensure that the default group is present.
        let default_group_exists = groups.iter().any(|g| g.name() == Group::DEFAULT_NAME);

        if default_group_exists {
            self.groups = groups;
        } else {
            self.groups.clear();
            self.groups.push(Group::default());
            self.groups.extend(groups);
        }
    }

    pub fn set_plugin_metadata(&mut self, plugin_metadata: PluginMetadata) {
        if plugin_metadata.is_regex_plugin() {
            self.regex_plugins.push(plugin_metadata);
        } else {
            self.plugins.insert(
                Filename::new(plugin_metadata.name().to_owned()),
                plugin_metadata,
            );
        }
    }

    pub fn remove_plugin_metadata(&mut self, plugin_name: &str) {
        self.plugins.remove(&Filename::new(plugin_name.to_owned()));
    }

    pub fn clear(&mut self) {
        self.bash_tags.clear();
        self.groups.clear();
        self.messages.clear();
        self.plugins.clear();
        self.regex_plugins.clear();
    }
}

impl std::default::Default for MetadataDocument {
    fn default() -> Self {
        Self {
            bash_tags: Vec::default(),
            groups: vec![Group::default()],
            messages: Vec::default(),
            plugins: HashMap::default(),
            regex_plugins: Vec::default(),
        }
    }
}

fn replace_prelude(masterlist: String, prelude: &str) -> String {
    if let Some((start, end)) = split_on_prelude(&masterlist) {
        let prelude = indent_prelude(prelude);

        format!("{start}{prelude}{end}")
    } else {
        masterlist
    }
}

fn split_on_prelude(masterlist: &str) -> Option<(&str, &str)> {
    let (prefix, remainder) = split_on_prelude_start(masterlist)?;

    let mut iter = remainder.bytes().enumerate().peekable();
    while let Some((index, byte)) = iter.next() {
        if byte != b'\n' {
            continue;
        }

        if let Some((_, next_byte)) = iter.peek() {
            if !matches!(next_byte, b' ' | b'#' | b'\n' | b'\r') {
                // Slicing at index should never fail, but we can't prove that,
                // and we don't want to risk panicking.
                if let Some(suffix) = remainder.get(index..) {
                    return Some((prefix, suffix));
                }
            }
        }
    }

    Some((prefix, ""))
}

fn split_on_prelude_start(masterlist: &str) -> Option<(&str, &str)> {
    let prelude_on_first_line = "prelude:";
    let prelude_on_new_line = "\nprelude:";

    if let Some(remainder) = masterlist.strip_prefix(prelude_on_first_line) {
        Some((prelude_on_first_line, remainder))
    } else {
        if let Some(pos) = masterlist.find(prelude_on_new_line) {
            let index = pos + prelude_on_new_line.len();
            // A checked split shouldn't be necessary, but there's no
            // split_inclusive_once() method, so we need to find and split in
            // two steps and there's always the risk of a bug being introduced
            // in the middle.
            if let Some((prefix, remainder)) = masterlist.split_at_checked(index) {
                return Some((prefix, remainder));
            }
        }
        None
    }
}

fn indent_prelude(prelude: &str) -> String {
    let prelude = ("\n  ".to_owned() + &prelude.replace('\n', "\n  "))
        .replace("  \r\n", "\r\n")
        .replace("  \n", "\n");

    if prelude.ends_with("\n  ") {
        prelude.trim_end_matches(' ').to_owned()
    } else {
        prelude
    }
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

    use crate::metadata::File;

    use super::*;

    mod metadata_document {
        use crate::metadata::MessageType;

        use super::*;

        const METADATA_LIST_YAML: &str = r#"bash_tags:
  - 'C.Climate'
  - 'Relev'

groups:
  - name: group1
    after:
      - group2
  - name: group2
    after:
      - default

globals:
  - type: say
    content: 'A global message.'

plugins:
  - name: 'Blank.esm'
    priority: -100
    msg:
      - type: warn
        content: 'This is a warning.'
      - type: say
        content: 'This message should be removed when evaluating conditions.'
        condition: 'active("Blank - Different.esm")'

  - name: 'Blank.+\.esp'
    after:
      - 'Blank.esm'

  - name: 'Blank.+(Different)?.*\.esp'
    inc:
      - 'Blank.esp'

  - name: 'Blank.esp'
    group: group2
    dirty:
      - crc: 0xDEADBEEF
        util: utility
    "#;

        #[test]
        fn load_from_str_should_resolve_aliases() {
            let yaml = "
        prelude:
          - &anchor
            type: say
            content: test message

        globals:
          - *anchor
        ";

            let mut metadata_list = MetadataDocument::default();
            metadata_list.load_from_str(yaml).unwrap();
        }

        #[test]
        fn load_from_str_should_resolve_merge_keys() {
            let yaml = r#"
        prelude:
          - &anchor
            type: say
            content: test message

        globals:
          - <<: *anchor
            condition: file("test.esp")
        "#;

            let mut metadata_list = MetadataDocument::default();
            metadata_list.load_from_str(yaml).unwrap();
        }

        #[test]
        fn load_from_str_should_error_if_a_plugin_has_two_exact_entries() {
            let yaml = "
plugins:
  - name: 'Blank.esm'
    msg:
      - type: warn
        content: 'This is a warning.'

  - name: 'Blank.esm'
    msg:
      - type: error
        content: 'This plugin entry will cause a failure, as it is not the first exact entry.'
        ";

            let mut metadata_list = MetadataDocument::default();
            assert!(metadata_list.load_from_str(yaml).is_err());
        }

        #[test]
        fn load_should_deserialise_masterlist() {
            let tmp_dir = tempdir().unwrap();

            let path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&path, METADATA_LIST_YAML).unwrap();

            let mut metadata_list = MetadataDocument::default();
            metadata_list.load(&path).unwrap();

            let plugin_names: Vec<_> = metadata_list
                .plugins_iter()
                .map(PluginMetadata::name)
                .collect();
            assert!(plugin_names.contains(&"Blank.esm"));
            assert!(plugin_names.contains(&"Blank.esp"));
            assert!(plugin_names.contains(&"Blank.+\\.esp"));
            assert!(plugin_names.contains(&"Blank.+(Different)?.*\\.esp"));

            assert_eq!(&["C.Climate", "Relev"], metadata_list.bash_tags());

            let groups = metadata_list.groups();
            assert_eq!(3, groups.len());

            assert_eq!("default", groups[0].name());
            assert!(groups[0].after_groups().is_empty());

            assert_eq!("group1", groups[1].name());
            assert_eq!(&["group2"], groups[1].after_groups());

            assert_eq!("group2", groups[2].name());
            assert_eq!(&["default"], groups[2].after_groups());
        }

        #[test]
        fn load_should_error_if_an_invalid_metadata_file_is_given() {
            let tmp_dir = tempdir().unwrap();
            let path = tmp_dir.path().join("masterlist.yaml");
            let yaml = r"
  - 'C.Climate'
  - 'Relev'

globals:
  - type: say
    content: 'A global message.'

plugins:
  - name: 'Blank.+\.esp'
    after:
      - 'Blank.esm'
        ";

            std::fs::write(&path, yaml).unwrap();

            let mut metadata_list = MetadataDocument::default();
            assert!(metadata_list.load(&path).is_err());
        }

        #[test]
        fn load_should_error_if_the_given_path_does_not_exist() {
            let mut metadata_list = MetadataDocument::default();
            assert!(metadata_list.load(Path::new("missing")).is_err());
        }

        #[test]
        fn load_with_prelude_should_merge_docs_with_crlf_line_endings() {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "prelude:\r\n  - &ref\r\n    type: say\r\n    content: Loaded from same file\r\n\r\n  - &otherRef\r\n    type: error\r\n    content: Error from same file\r\nglobals:\r\n  - *ref\r\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\r\n  - &ref\r\n    type: say\r\n    content: Loaded from prelude\r\n\r\n  - &otherRef\r\n    type: error\r\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap();

            assert_eq!(
                [
                    Message::new(MessageType::Say, "Loaded from prelude".to_owned()),
                    Message::new(MessageType::Error, "An error message".to_owned()),
                ],
                metadata_list.messages()
            );
        }

        #[test]
        fn load_with_prelude_should_merge_docs_with_lf_line_endings() {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "prelude:\n  - &ref\n    type: say\n    content: Loaded from same file\n\n  - &otherRef\n    type: error\n    content: Error from same file\nglobals:\n  - *ref\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    type: say\n    content: Loaded from prelude\n\n  - &otherRef\n    type: error\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap();

            assert_eq!(
                [
                    Message::new(MessageType::Say, "Loaded from prelude".to_owned()),
                    Message::new(MessageType::Error, "An error message".to_owned()),
                ],
                metadata_list.messages()
            );
        }

        #[test]
        fn load_with_prelude_should_error_if_the_given_masterlist_path_does_not_exist() {
            let tmp_dir = tempdir().unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    type: say\n    content: Loaded from prelude\n",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            assert!(
                metadata_list
                    .load_with_prelude(Path::new("missing"), &prelude_path)
                    .is_err()
            );
        }

        #[test]
        fn load_with_prelude_should_error_if_the_given_prelude_path_does_not_exist() {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "prelude:\n  - &ref\n    type: say\n    content: Loaded from same file\nglobals:\n  - *ref\n").unwrap();

            let mut metadata_list = MetadataDocument::default();
            assert!(
                metadata_list
                    .load_with_prelude(&masterlist_path, Path::new("missing"))
                    .is_err()
            );
        }

        #[test]
        fn save_should_write_the_loaded_metadata() {
            let tmp_dir = tempdir().unwrap();

            let path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&path, METADATA_LIST_YAML).unwrap();

            let mut metadata = MetadataDocument::default();
            metadata.load(&path).unwrap();

            let other_path = tmp_dir.path().join("other.yaml");
            metadata.save(&other_path).unwrap();

            let mut other_metadata = MetadataDocument::default();
            other_metadata.load(&other_path).unwrap();

            assert_eq!(metadata, other_metadata);
        }

        #[test]
        fn clear_should_clear_all_loaded_data() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            assert!(!metadata.messages().is_empty());
            assert!(metadata.plugins_iter().next().is_some());
            assert!(!metadata.bash_tags().is_empty());

            metadata.clear();

            assert!(metadata.messages().is_empty());
            assert!(metadata.plugins_iter().next().is_none());
            assert!(metadata.bash_tags().is_empty());
        }

        #[test]
        fn set_groups_should_replace_existing_groups() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            metadata.set_groups(vec![Group::new("group4".into())]);

            let groups = metadata.groups();

            assert_eq!("default", groups[0].name());
            assert!(groups[0].after_groups().is_empty());

            assert_eq!("group4", groups[1].name());
            assert!(groups[1].after_groups().is_empty());
        }

        #[test]
        fn find_plugin_should_return_none_if_the_given_plugin_has_no_metadata() {
            let metadata = MetadataDocument::default();
            assert!(metadata.find_plugin("Blank.esp").unwrap().is_none());
        }

        #[test]
        fn find_plugin_should_return_the_metadata_object_if_one_exists() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let name = "Blank - Different.esp";
            let plugin = metadata.find_plugin(name).unwrap().unwrap();

            assert_eq!(name, plugin.name());
            assert_eq!(&[File::new("Blank.esm".into())], plugin.load_after_files());
            assert_eq!(&[File::new("Blank.esp".into())], plugin.incompatibilities());
        }

        #[test]
        fn add_plugin_should_store_specific_plugin_metadata() {
            let mut metadata = MetadataDocument::default();

            let name = "Blank.esp";
            let mut plugin = PluginMetadata::new(name).unwrap();
            plugin.set_group("group1".into());
            metadata.set_plugin_metadata(plugin);

            let plugin = metadata.find_plugin(name).unwrap().unwrap();

            assert_eq!(name, plugin.name());
            assert_eq!("group1", plugin.group().unwrap());
        }

        #[test]
        fn add_plugin_should_store_given_regex_plugin_metadata() {
            let mut metadata = MetadataDocument::default();

            let mut plugin = PluginMetadata::new(".+Dependent\\.esp").unwrap();
            plugin.set_group("group1".into());
            metadata.set_plugin_metadata(plugin);

            let name = "Blank - Plugin Dependent.esp";
            let plugin = metadata.find_plugin(name).unwrap().unwrap();

            assert_eq!(name, plugin.name());
            assert_eq!("group1", plugin.group().unwrap());
        }

        #[test]
        fn remove_plugin_metadata_should_remove_the_given_plugin_specific_metadata() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let name = "Blank.esp";
            assert!(metadata.find_plugin(name).unwrap().is_some());

            metadata.remove_plugin_metadata(name);

            assert!(metadata.find_plugin(name).unwrap().is_none());
        }

        #[test]
        fn remove_plugin_metadata_should_not_remove_matching_regex_plugin_metadata() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let name = "Blank.+\\.esp";
            assert!(metadata.find_plugin(name).unwrap().is_some());

            metadata.remove_plugin_metadata(name);

            assert!(metadata.find_plugin(name).unwrap().is_some());

            metadata.remove_plugin_metadata("Blank - Different.esp");

            assert!(metadata.find_plugin(name).unwrap().is_some());
        }
    }

    mod replace_prelude {
        use super::*;

        #[test]
        fn should_return_an_empty_string_if_given_empty_strings() {
            let result = replace_prelude(String::new(), "");

            assert!(result.is_empty());
        }

        #[test]
        fn should_not_change_a_masterlist_with_no_prelude() {
            let prelude = "globals:
  - type: note
    content: A message.
";
            let masterlist = "plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude);

            assert_eq!(masterlist, result);
        }

        #[test]
        fn should_not_change_a_flow_style_masterlist() {
            let prelude = "globals: [{type: note, content: A message.}]";
            let masterlist = "{prelude: {}, plugins: [{name: a.esp}]}";

            let result = replace_prelude(masterlist.into(), prelude);

            assert_eq!(masterlist, result);
        }

        #[test]
        fn should_replace_a_prelude_at_the_start_of_the_masterlist() {
            let prelude = "globals:
  - type: note
    content: A message.
";
            let masterlist = "prelude:
  a: b

plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude);

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result);
        }

        #[test]
        fn should_change_a_masterlist_that_ends_with_a_prelude() {
            let prelude = "globals:
  - type: note
    content: A message.
";
            let masterlist = "plugins:
  - name: a.esp
prelude:
  a: b

";

            let result = replace_prelude(masterlist.into(), prelude);

            let expected_result = "plugins:
  - name: a.esp
prelude:
  globals:
    - type: note
      content: A message.
";

            assert_eq!(expected_result, result);
        }

        #[test]
        fn should_replace_only_the_prelude_in_the_masterlist() {
            let prelude = "

globals:
  - type: note
    content: A message.

";
            let masterlist = "
common:
  key: value
prelude:
  a: b
plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude);

            let expected_result = "
common:
  key: value
prelude:


  globals:
    - type: note
      content: A message.


plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result);
        }

        #[test]
        fn should_succeed_given_block_style_prelude_and_masterlist() {
            let prelude = "globals:
  - type: note
    content: A message.
";
            let masterlist = "prelude:
  a: b

plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude);

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result);
        }

        #[test]
        fn should_succeed_given_a_flow_style_prelude_and_a_block_style_masterlist() {
            let prelude = "globals: [{type: note, content: A message.}]";
            let masterlist = "prelude:
  a: b

plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude);

            let expected_result = "prelude:
  globals: [{type: note, content: A message.}]
plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result);
        }

        #[test]
        fn should_not_stop_at_comments() {
            let prelude = "globals:
  - type: note
    content: A message.
";
            let masterlist = "prelude:
  a: b
# Comment line
  c: d

plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude);

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result);
        }

        #[test]
        fn should_not_stop_at_a_blank_line() {
            let prelude = "globals:
  - type: note
    content: A message.
";
            let masterlist = "prelude:
  a: b


plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude);

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result);
        }
    }
}
