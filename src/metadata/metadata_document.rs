use std::{
    collections::{HashMap, HashSet},
    path::Path,
    sync::LazyLock,
};

use saphyr::{MarkedYaml, YamlData};

use crate::logging;

use super::{
    error::{
        ExpectedType, LoadMetadataError, MetadataDocumentParsingError, ParseMetadataError,
        RegexError, WriteMetadataError, YamlMergeKeyError,
    },
    file::Filename,
    group::Group,
    message::Message,
    plugin_metadata::PluginMetadata,
    yaml::{YamlObjectType, as_string_node, get_as_slice},
    yaml_emit::{EmitYaml, YamlEmitter},
};

static MERGE_KEY: LazyLock<MarkedYaml> = LazyLock::new(|| as_string_node("<<"));

#[derive(Clone, Debug, Default, Eq, PartialEq)]
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

        log::trace!("Loading file: {:?}", file_path);

        let content = std::fs::read_to_string(file_path)
            .map_err(|e| LoadMetadataError::from_io_error(file_path.into(), e))?;

        self.load_from_str(&content)
            .map_err(|e| LoadMetadataError::new(file_path.into(), e))?;

        log::trace!(
            "Successfully loaded metadata from file at \"{:?}\".",
            file_path
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

        let masterlist = replace_prelude(masterlist, prelude);

        self.load_from_str(&masterlist)
            .map_err(|e| LoadMetadataError::new(masterlist_path.into(), e))?;

        log::trace!(
            "Successfully loaded metadata from file at \"{:?}\".",
            masterlist_path
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

        let doc = match doc.data {
            YamlData::Hash(h) => h,
            _ => {
                return Err(ParseMetadataError::unexpected_type(
                    doc.span.start,
                    YamlObjectType::MetadataDocument,
                    ExpectedType::Map,
                )
                .into());
            }
        };

        let mut plugins: HashMap<Filename, PluginMetadata> = HashMap::new();
        let mut regex_plugins: Vec<PluginMetadata> = Vec::new();
        for plugin_yaml in get_as_slice(&doc, "plugins", YamlObjectType::MetadataDocument)? {
            let plugin = PluginMetadata::try_from(plugin_yaml)?;
            if plugin.is_regex_plugin() {
                regex_plugins.push(plugin);
            } else {
                let filename = Filename::new(plugin.name().to_string());
                if plugins.contains_key(&filename) {
                    return Err(ParseMetadataError::duplicate_entry(
                        plugin_yaml.span.start,
                        plugin.name().to_string(),
                        YamlObjectType::PluginMetadata,
                    )
                    .into());
                }
                plugins.insert(filename, plugin);
            }
        }

        let messages = get_as_slice(&doc, "globals", YamlObjectType::MetadataDocument)?
            .iter()
            .map(Message::try_from)
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
                    bash_tag.to_string(),
                    YamlObjectType::BashTagsElement,
                )
                .into());
            }

            bash_tags.push(bash_tag.to_string());
            str_set.insert(bash_tag);
        }

        let mut group_names = HashSet::new();
        let mut groups = Vec::new();
        for group_yaml in get_as_slice(&doc, "groups", YamlObjectType::MetadataDocument)? {
            let group = Group::try_from(group_yaml)?;

            let name = group.name().to_string();
            if group_names.contains(&name) {
                return Err(ParseMetadataError::duplicate_entry(
                    group_yaml.span.start,
                    group.name().to_string(),
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
        logging::trace!("Saving metadata list to: {}", file_path.display());

        let mut emitter = YamlEmitter::new();

        if !self.bash_tags.is_empty() {
            emitter.map_key("bash_tags");

            emitter.begin_array();

            for tag in &self.bash_tags {
                emitter.unquoted_str(tag);
            }

            emitter.end_array();
        }

        if !self.groups.is_empty() {
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

            for plugin in self.plugins() {
                plugin.emit_yaml(&mut emitter);
            }

            emitter.end_array();
        }

        std::fs::write(file_path, emitter.into_string())
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

    pub fn plugins(&self) -> impl Iterator<Item = &PluginMetadata> {
        self.plugins.values().chain(self.regex_plugins.iter())
    }

    pub fn find_plugin(&self, plugin_name: &str) -> Result<Option<PluginMetadata>, RegexError> {
        let mut metadata = match self.plugins.get(&Filename::new(plugin_name.to_string())) {
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
        self.groups = groups;
    }

    pub fn set_plugin_metadata(&mut self, plugin_metadata: PluginMetadata) {
        self.plugins.insert(
            Filename::new(plugin_metadata.name().to_string()),
            plugin_metadata,
        );
    }

    pub fn remove_plugin_metadata(&mut self, plugin_name: &str) {
        self.plugins.remove(&Filename::new(plugin_name.to_string()));
    }

    pub fn clear(&mut self) {
        self.bash_tags.clear();
        self.groups.clear();
        self.messages.clear();
        self.plugins.clear();
        self.regex_plugins.clear();
    }
}

fn process_merge_keys(mut yaml: MarkedYaml) -> Result<MarkedYaml, YamlMergeKeyError> {
    match yaml.data {
        YamlData::Array(a) => {
            yaml.data = merge_array_elements(a).map(YamlData::Array)?;
            Ok(yaml)
        }
        YamlData::Hash(h) => {
            yaml.data = merge_hash_keys(h).map(YamlData::Hash)?;
            Ok(yaml)
        }
        _ => Ok(yaml),
    }
}

fn merge_array_elements(
    array: saphyr::AnnotatedArray<MarkedYaml>,
) -> Result<saphyr::AnnotatedArray<MarkedYaml>, YamlMergeKeyError> {
    array.into_iter().map(process_merge_keys).collect()
}

fn merge_hash_keys(
    hash: saphyr::AnnotatedHash<MarkedYaml>,
) -> Result<saphyr::AnnotatedHash<MarkedYaml>, YamlMergeKeyError> {
    let mut hash: saphyr::AnnotatedHash<MarkedYaml> = hash
        .into_iter()
        .map(|(key, value)| {
            process_merge_keys(key)
                .and_then(|key| process_merge_keys(value).map(|value| (key, value)))
        })
        .collect::<Result<_, _>>()?;

    if let Some(value) = hash.remove(&MERGE_KEY) {
        merge_into_hash(hash, value)
    } else {
        Ok(hash)
    }
}

fn merge_into_hash(
    hash: saphyr::AnnotatedHash<MarkedYaml>,
    value: MarkedYaml,
) -> Result<saphyr::AnnotatedHash<MarkedYaml>, YamlMergeKeyError> {
    match value.data {
        YamlData::<MarkedYaml>::Array(a) => a.into_iter().try_fold(hash, |acc, e| {
            if let YamlData::Hash(h) = e.data {
                Ok(merge_hashes(acc, h))
            } else {
                Err(YamlMergeKeyError::new(e))
            }
        }),
        YamlData::<MarkedYaml>::Hash(h) => Ok(merge_hashes(hash, h)),
        _ => Err(YamlMergeKeyError::new(value)),
    }
}

fn merge_hashes(
    mut hash1: saphyr::AnnotatedHash<MarkedYaml>,
    hash2: saphyr::AnnotatedHash<MarkedYaml>,
) -> saphyr::AnnotatedHash<MarkedYaml> {
    for (key, value) in hash2 {
        hash1.entry(key).or_insert(value);
    }
    hash1
}

fn replace_prelude(masterlist: String, prelude: String) -> String {
    if let Some((start, end)) = find_prelude_bounds(&masterlist) {
        let prelude = indent_prelude(prelude);

        masterlist[..start].to_string() + &prelude + &masterlist[end..]
    } else {
        masterlist
    }
}

fn find_prelude_bounds(masterlist: &str) -> Option<(usize, usize)> {
    let prelude_on_first_line = "prelude:";
    let prelude_on_new_line = "\nprelude:";

    let start = if masterlist.starts_with(prelude_on_first_line) {
        prelude_on_first_line.len()
    } else if let Some(pos) = masterlist.find(prelude_on_new_line) {
        pos + prelude_on_new_line.len()
    } else {
        return None;
    };

    let mut pos = start;
    while let Some(next_line_break_pos) = masterlist[pos..].find('\n') {
        if next_line_break_pos == masterlist.len() - 1 {
            break;
        }

        pos = next_line_break_pos + 1;

        if let Some(c) = masterlist.as_bytes().get(pos) {
            if *c != b' ' && *c != b'#' && *c != b'\n' {
                return Some((start, next_line_break_pos));
            }
        }
    }

    Some((start, masterlist.len()))
}

fn indent_prelude(prelude: String) -> String {
    let prelude = ("\n  ".to_string() + &prelude.replace("\n", "\n  ")).replace("  \n", "\n");

    if prelude.ends_with("\n  ") {
        prelude[..prelude.len() - 2].to_string()
    } else {
        prelude
    }
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

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
    fn load_should_resolve_aliases() {
        let tmp_dir = tempdir().unwrap();
        let yaml = r#"
        prelude:
          - &anchor
            type: say
            content: test message

        globals:
          - *anchor
        "#;

        let path = tmp_dir.path().join("masterlist.yaml");
        std::fs::write(&path, yaml).unwrap();

        let mut metadata_list = MetadataDocument::default();
        metadata_list.load(&path).unwrap();
    }

    #[test]
    fn load_should_resolve_merge_keys() {
        let tmp_dir = tempdir().unwrap();
        let yaml = r#"
        prelude:
          - &anchor
            type: say
            content: test message

        globals:
          - <<: *anchor
            condition: file("test.esp")
        "#;

        let path = tmp_dir.path().join("masterlist.yaml");
        std::fs::write(&path, yaml).unwrap();

        let mut metadata_list = MetadataDocument::default();
        metadata_list.load(&path).unwrap();
    }

    #[test]
    fn load_should_deserialise_masterlist() {
        let tmp_dir = tempdir().unwrap();

        let path = tmp_dir.path().join("masterlist.yaml");
        std::fs::write(&path, METADATA_LIST_YAML).unwrap();

        let mut metadata_list = MetadataDocument::default();
        metadata_list.load(&path).unwrap();
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
}
