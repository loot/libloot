use std::borrow::Cow;

use fancy_regex::{Error as RegexImplError, Regex};
use saphyr::MarkedYaml;

use crate::{Database, case_insensitive_regex, error::ConditionEvaluationError, logging};

use super::{
    error::{MetadataParsingErrorReason, ParseMetadataError, RegexError},
    file::File,
    location::Location,
    message::Message,
    plugin_cleaning_data::PluginCleaningData,
    tag::Tag,
    yaml::{
        EmitYaml, TryFromYaml, YamlEmitter, YamlObjectType, as_mapping, get_as_slice,
        get_required_string_value, get_string_value,
    },
};

pub(crate) const GHOST_FILE_EXTENSION: &str = ".ghost";

/// Represents a plugin's metadata.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct PluginMetadata {
    name: PluginName,
    group: Option<Box<str>>,
    load_after: Box<[File]>,
    requirements: Box<[File]>,
    incompatibilities: Box<[File]>,
    messages: Box<[Message]>,
    tags: Box<[Tag]>,
    dirty_info: Box<[PluginCleaningData]>,
    clean_info: Box<[PluginCleaningData]>,
    locations: Box<[Location]>,
}

impl PluginMetadata {
    /// Construct a [PluginMetadata] object with no metadata for a plugin with
    /// the given filename.
    pub fn new(name: &str) -> Result<Self, RegexError> {
        Ok(Self {
            name: PluginName::new(name)?,
            ..Default::default()
        })
    }

    /// Get the plugin name.
    pub fn name(&self) -> &str {
        self.name.as_str()
    }

    /// Get the plugin's group.
    ///
    /// The [Option] is `None` if no group is explicitly set.
    pub fn group(&self) -> Option<&str> {
        self.group.as_deref()
    }

    /// Get the plugins that the plugin must load after.
    pub fn load_after_files(&self) -> &[File] {
        &self.load_after
    }

    /// Get the files that the plugin requires to be installed.
    pub fn requirements(&self) -> &[File] {
        &self.requirements
    }

    /// Get the files that the plugin is incompatible with.
    pub fn incompatibilities(&self) -> &[File] {
        &self.incompatibilities
    }

    /// Get the plugin's messages.
    pub fn messages(&self) -> &[Message] {
        &self.messages
    }

    /// Get the plugin's Bash Tag suggestions.
    pub fn tags(&self) -> &[Tag] {
        &self.tags
    }

    /// Get the plugin's dirty plugin information.
    pub fn dirty_info(&self) -> &[PluginCleaningData] {
        &self.dirty_info
    }

    /// Get the plugin's clean plugin information.
    pub fn clean_info(&self) -> &[PluginCleaningData] {
        &self.clean_info
    }

    /// Get the locations at which this plugin can be found.
    pub fn locations(&self) -> &[Location] {
        &self.locations
    }

    /// Set the plugin's group.
    pub fn set_group(&mut self, group: String) {
        self.group = Some(group.into_boxed_str());
    }

    /// Unsets the plugin's group, so that it is implicitly a member of the
    /// default group.
    pub fn unset_group(&mut self) {
        self.group = None;
    }

    /// Get the plugins that the plugin must load after.
    pub fn set_load_after_files(&mut self, files: Vec<File>) {
        self.load_after = files.into_boxed_slice();
    }

    /// Get the files that the plugin requires to be installed.
    pub fn set_requirements(&mut self, files: Vec<File>) {
        self.requirements = files.into_boxed_slice();
    }

    /// Get the files that the plugin is incompatible with.
    pub fn set_incompatibilities(&mut self, files: Vec<File>) {
        self.incompatibilities = files.into_boxed_slice();
    }

    /// Get the plugin's messages.
    pub fn set_messages(&mut self, messages: Vec<Message>) {
        self.messages = messages.into_boxed_slice();
    }

    /// Get the plugin's Bash Tag suggestions.
    pub fn set_tags(&mut self, tags: Vec<Tag>) {
        self.tags = tags.into_boxed_slice();
    }

    /// Get the plugin's dirty plugin information.
    pub fn set_dirty_info(&mut self, dirty_info: Vec<PluginCleaningData>) {
        self.dirty_info = dirty_info.into_boxed_slice();
    }

    /// Get the plugin's clean plugin information.
    pub fn set_clean_info(&mut self, clean_info: Vec<PluginCleaningData>) {
        self.clean_info = clean_info.into_boxed_slice();
    }

    /// Get the locations at which this plugin can be found.
    pub fn set_locations(&mut self, locations: Vec<Location>) {
        self.locations = locations.into_boxed_slice();
    }

    /// Merge metadata from the given [PluginMetadata] object into this object.
    ///
    /// If an equal metadata object already exists in this PluginMetadata
    /// object, it is not duplicated. This object's group is replaced by the
    /// given object's group if the latter is explicit.
    pub fn merge_metadata(&mut self, plugin: &PluginMetadata) {
        if plugin.has_name_only() {
            return;
        }

        if self.group.is_none() && plugin.group.is_some() {
            self.group.clone_from(&plugin.group);
        }

        merge_slices(&mut self.load_after, &plugin.load_after);
        merge_slices(&mut self.requirements, &plugin.requirements);
        merge_slices(&mut self.incompatibilities, &plugin.incompatibilities);
        merge_slices(&mut self.tags, &plugin.tags);

        self.messages = self
            .messages
            .iter()
            .chain(plugin.messages.iter())
            .cloned()
            .collect();

        merge_slices(&mut self.dirty_info, &plugin.dirty_info);
        merge_slices(&mut self.clean_info, &plugin.clean_info);
        merge_slices(&mut self.locations, &plugin.locations);
    }

    /// Check if no plugin metadata is set.
    pub fn has_name_only(&self) -> bool {
        self.group.is_none()
            && self.load_after.is_empty()
            && self.requirements.is_empty()
            && self.incompatibilities.is_empty()
            && self.messages.is_empty()
            && self.tags.is_empty()
            && self.dirty_info.is_empty()
            && self.clean_info.is_empty()
            && self.locations.is_empty()
    }

    /// Check if the plugin name is a regular expression.
    ///
    /// Returns `true` if the plugin name contains any of the characters `:\*?|`
    /// and `false` otherwise.
    pub fn is_regex_plugin(&self) -> bool {
        self.name.is_regex()
    }

    /// Check if the given plugin name matches this plugin metadata object's
    /// name field.
    ///
    /// If the name field is a regular expression, the given plugin name will be
    /// matched against it, otherwise the strings will be compared
    /// case-insensitively. The given plugin name must be literal, i.e. not a
    /// regular expression.
    pub fn name_matches(&self, other_name: &str) -> bool {
        self.name.matches(other_name)
    }

    /// Serialises the plugin metadata as YAML.
    pub fn as_yaml(&self) -> String {
        let mut emitter = YamlEmitter::new();
        self.emit_yaml(&mut emitter);
        emitter.into_string()
    }

    pub(crate) fn filter_by_constraints(
        mut self,
        database: &Database,
    ) -> Result<Self, ConditionEvaluationError> {
        self.load_after = filter_files_by_constraint(self.load_after, database)?;
        self.requirements = filter_files_by_constraint(self.requirements, database)?;

        Ok(self)
    }
}

fn filter_files_by_constraint(
    files: Box<[File]>,
    database: &Database,
) -> Result<Box<[File]>, ConditionEvaluationError> {
    files
        .into_iter()
        .filter_map(|f| {
            if let Some(c) = f.constraint() {
                database.evaluate(c).map(|r| r.then_some(f)).transpose()
            } else {
                Some(Ok(f))
            }
        })
        .collect()
}

#[derive(Clone, Debug, Default)]
struct PluginName {
    string: Box<str>,
    regex: Option<Regex>,
}

impl PluginName {
    fn new(name: &str) -> Result<Self, Box<RegexImplError>> {
        let name: Box<str> = trim_dot_ghost(name).into();

        if is_regex_name(&name) {
            let non_capturing_name = replace_capturing_groups(&name);

            let regex = case_insensitive_regex(&format!("^{}$", &non_capturing_name))?;

            Ok(Self {
                string: name,
                regex: Some(regex),
            })
        } else {
            Ok(Self {
                string: name,
                regex: None,
            })
        }
    }

    fn matches(&self, other_name: &str) -> bool {
        if let Some(regex) = &self.regex {
            is_regex_match(regex, other_name)
        } else {
            unicase::eq(self.string.as_ref(), other_name)
        }
    }

    fn is_regex(&self) -> bool {
        self.regex.is_some()
    }

    fn as_str(&self) -> &str {
        &self.string
    }
}

impl std::cmp::PartialEq for PluginName {
    fn eq(&self, other: &Self) -> bool {
        self.as_str() == other.as_str()
    }
}

impl std::cmp::Eq for PluginName {}

impl std::cmp::PartialOrd for PluginName {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl std::cmp::Ord for PluginName {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.as_str().cmp(other.as_str())
    }
}

impl std::hash::Hash for PluginName {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.as_str().hash(state);
    }
}

pub(crate) fn trim_dot_ghost(string: &str) -> &str {
    let suffix_start_index = string.len().saturating_sub(GHOST_FILE_EXTENSION.len());
    if let Some((first, last)) = string.split_at_checked(suffix_start_index) {
        if last.eq_ignore_ascii_case(GHOST_FILE_EXTENSION) {
            first
        } else {
            string
        }
    } else {
        string
    }
}

pub(crate) fn iends_with_ascii(string: &str, suffix: &str) -> bool {
    // as_bytes().into_iter() is faster than bytes().
    string.len() >= suffix.len()
        && string
            .as_bytes()
            .iter()
            .rev()
            .zip(suffix.as_bytes().iter().rev())
            .all(|(string_byte, suffix_byte)| string_byte.eq_ignore_ascii_case(suffix_byte))
}

fn is_regex_name(name: &str) -> bool {
    name.contains([':', '\\', '*', '?', '|'])
}

fn merge_slices<T: Clone + PartialEq>(target: &mut Box<[T]>, source: &[T]) {
    let mut vec = target.to_vec();
    for element in source {
        if !target.contains(element) {
            vec.push(element.clone());
        }
    }

    *target = vec.into_boxed_slice();
}

fn replace_capturing_groups(regex_string: &str) -> Cow<'_, str> {
    let mut iter = regex_string.split_inclusive('(').peekable();

    let mut parts = Vec::new();

    while let Some(before) = iter.next() {
        parts.push(before);

        let Some(after) = iter.peek() else {
            break;
        };

        if !after.starts_with('?') && (!before.ends_with("\\(") || before.ends_with("\\\\(")) {
            parts.push("?:");
        }
    }

    let new_length: usize = parts.iter().map(|s| s.len()).sum();

    if new_length == regex_string.len() {
        Cow::Borrowed(regex_string)
    } else {
        Cow::Owned(parts.into_iter().collect())
    }
}

fn is_regex_match(regex: &Regex, string: &str) -> bool {
    regex
        .is_match(string)
        .inspect_err(|e| {
            logging::error!(
                "Encountered an error while trying to match the regex {} to the string {}: {}",
                regex.as_str(),
                string,
                e
            );
        })
        .unwrap_or(false)
}

impl TryFromYaml for PluginMetadata {
    fn try_from_yaml(value: &MarkedYaml) -> Result<Self, ParseMetadataError> {
        let mapping = as_mapping(value, YamlObjectType::PluginMetadata)?;

        let name = get_required_string_value(
            value.span.start,
            mapping,
            "name",
            YamlObjectType::PluginMetadata,
        )?;
        let name = match PluginName::new(name) {
            Ok(n) => n,
            Err(e) => {
                return Err(ParseMetadataError::new(
                    value.span.start,
                    MetadataParsingErrorReason::InvalidRegex(e),
                ));
            }
        };

        let group = get_string_value(mapping, "group", YamlObjectType::PluginMetadata)?;

        let load_after = get_boxed_slice(mapping, "after")?;
        let requirements = get_boxed_slice(mapping, "req")?;
        let incompatibilities = get_boxed_slice(mapping, "inc")?;
        let messages = get_boxed_slice(mapping, "msg")?;
        let tags = get_boxed_slice(mapping, "tag")?;
        let dirty_info = get_boxed_slice(mapping, "dirty")?;
        let clean_info = get_boxed_slice(mapping, "clean")?;
        let locations = get_boxed_slice(mapping, "url")?;

        Ok(PluginMetadata {
            name,
            group: group.map(|g| g.1.into()),
            load_after,
            requirements,
            incompatibilities,
            messages,
            dirty_info,
            clean_info,
            tags,
            locations,
        })
    }
}

fn get_boxed_slice<T: TryFromYaml>(
    mapping: &saphyr::AnnotatedMapping<MarkedYaml>,
    key: &'static str,
) -> Result<Box<[T]>, ParseMetadataError> {
    get_as_slice(mapping, key, YamlObjectType::PluginMetadata)?
        .iter()
        .map(|e| T::try_from_yaml(e))
        .collect()
}

impl EmitYaml for PluginMetadata {
    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        emitter.begin_map();

        emitter.map_key("name");
        emitter.single_quoted_str(self.name());

        if !self.locations.is_empty() {
            emitter.map_key("url");
            self.locations.emit_yaml(emitter);
        }

        if let Some(group) = &self.group {
            emitter.map_key("group");
            emitter.single_quoted_str(group);
        }

        if !self.load_after.is_empty() {
            emitter.map_key("after");
            self.load_after.emit_yaml(emitter);
        }

        if !self.requirements.is_empty() {
            emitter.map_key("req");
            self.requirements.emit_yaml(emitter);
        }

        if !self.incompatibilities.is_empty() {
            emitter.map_key("inc");
            self.incompatibilities.emit_yaml(emitter);
        }

        if !self.messages.is_empty() {
            emitter.map_key("msg");
            self.messages.emit_yaml(emitter);
        }

        if !self.tags.is_empty() {
            emitter.map_key("tag");
            self.tags.emit_yaml(emitter);
        }

        if !self.dirty_info.is_empty() {
            emitter.map_key("dirty");
            self.dirty_info.emit_yaml(emitter);
        }

        if !self.clean_info.is_empty() {
            emitter.map_key("clean");
            self.clean_info.emit_yaml(emitter);
        }

        emitter.end_map();
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        metadata::{MessageType, TagSuggestion},
        tests::{BLANK_DIFFERENT_ESM, BLANK_DIFFERENT_ESP, BLANK_ESM, BLANK_ESP},
    };

    use super::*;

    mod name_matches {
        use super::*;

        #[test]
        fn should_use_case_insensitive_comparison_for_non_regex_names() {
            let plugin = PluginMetadata::new("BLANK.ESM").unwrap();

            assert!(plugin.name_matches("blank.esm"));
            assert!(!plugin.name_matches("other.esm"));
        }

        #[test]
        fn should_treat_given_plugin_name_as_literal() {
            let plugin = PluginMetadata::new("Blank.esm").unwrap();

            assert!(!plugin.name_matches(".+"));
        }

        #[test]
        fn should_use_case_insensitive_regex_matching_for_a_regex_name() {
            let plugin = PluginMetadata::new("Blank.ES(m|p)").unwrap();

            assert!(plugin.name_matches("blank.esm"));
        }
    }

    mod merge_metadata {
        use super::*;

        #[test]
        fn should_not_change_name() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let plugin2 = PluginMetadata::new(BLANK_DIFFERENT_ESM).unwrap();

            plugin1.merge_metadata(&plugin2);

            assert_eq!(BLANK_ESM, plugin1.name());
        }

        #[test]
        fn should_not_use_other_group_if_current_group_is_set() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin1.set_group("group1".into());
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin2.set_group("group2".into());

            plugin1.merge_metadata(&plugin2);

            assert_eq!("group1", plugin1.group().unwrap());

            plugin2.unset_group();

            plugin1.merge_metadata(&plugin2);

            assert_eq!("group1", plugin1.group().unwrap());
        }

        #[test]
        fn should_use_other_group_if_current_group_is_none() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin2.set_group("group2".into());

            plugin1.merge_metadata(&plugin2);

            assert_eq!("group2", plugin1.group().unwrap());
        }

        #[test]
        fn should_merge_load_after_files() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let file1 = File::new(BLANK_DIFFERENT_ESM.into());
            let file2 = File::new(BLANK_ESP.into());
            let file3 = File::new(BLANK_DIFFERENT_ESP.into());
            plugin1.set_load_after_files(vec![file1.clone(), file2.clone()]);
            plugin2.set_load_after_files(vec![file1.clone(), file3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(
                &[file1.clone(), file2.clone(), file3.clone()],
                plugin1.load_after_files()
            );
        }

        #[test]
        fn should_merge_requirements() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let file1 = File::new(BLANK_DIFFERENT_ESM.into());
            let file2 = File::new(BLANK_ESP.into());
            let file3 = File::new(BLANK_DIFFERENT_ESP.into());
            plugin1.set_requirements(vec![file1.clone(), file2.clone()]);
            plugin2.set_requirements(vec![file1.clone(), file3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(
                &[file1.clone(), file2.clone(), file3.clone()],
                plugin1.requirements()
            );
        }

        #[test]
        fn should_merge_incompatibilities() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let file1 = File::new(BLANK_DIFFERENT_ESM.into());
            let file2 = File::new(BLANK_ESP.into());
            let file3 = File::new(BLANK_DIFFERENT_ESP.into());
            plugin1.set_incompatibilities(vec![file1.clone(), file2.clone()]);
            plugin2.set_incompatibilities(vec![file1.clone(), file3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(
                &[file1.clone(), file2.clone(), file3.clone()],
                plugin1.incompatibilities()
            );
        }

        #[test]
        fn should_merge_messages() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let message1 = Message::new(MessageType::Say, "content1".into());
            let message2 = Message::new(MessageType::Say, "content2".into());
            let message3 = Message::new(MessageType::Say, "content3".into());
            plugin1.set_messages(vec![message1.clone(), message2.clone()]);
            plugin2.set_messages(vec![message1.clone(), message3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(
                &[
                    message1.clone(),
                    message2.clone(),
                    message1.clone(),
                    message3.clone()
                ],
                plugin1.messages()
            );
        }

        #[test]
        fn should_merge_tags() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let tag1 = Tag::new("Relev".into(), TagSuggestion::Addition);
            let tag2 = Tag::new("Delev".into(), TagSuggestion::Addition);
            let tag3 = Tag::new("Relev".into(), TagSuggestion::Removal);
            plugin1.set_tags(vec![tag1.clone(), tag2.clone()]);
            plugin2.set_tags(vec![tag1.clone(), tag3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(&[tag1.clone(), tag2.clone(), tag3.clone()], plugin1.tags());
        }

        #[test]
        fn should_merge_dirty_info() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let data1 = PluginCleaningData::new(0x1234_5678, "util1".into());
            let data2 = PluginCleaningData::new(0xDEAD_BEEF, "util2".into());
            let data3 = PluginCleaningData::new(0xFEED_CAFE, "util3".into());
            plugin1.set_dirty_info(vec![data1.clone(), data2.clone()]);
            plugin2.set_dirty_info(vec![data1.clone(), data3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(
                &[data1.clone(), data2.clone(), data3.clone()],
                plugin1.dirty_info()
            );
        }

        #[test]
        fn should_merge_clean_info() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let data1 = PluginCleaningData::new(0x1234_5678, "util1".into());
            let data2 = PluginCleaningData::new(0xDEAD_BEEF, "util2".into());
            let data3 = PluginCleaningData::new(0xFEED_CAFE, "util3".into());
            plugin1.set_clean_info(vec![data1.clone(), data2.clone()]);
            plugin2.set_clean_info(vec![data1.clone(), data3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(
                &[data1.clone(), data2.clone(), data3.clone()],
                plugin1.clean_info()
            );
        }

        #[test]
        fn should_merge_locations() {
            let mut plugin1 = PluginMetadata::new(BLANK_ESM).unwrap();
            let mut plugin2 = PluginMetadata::new(BLANK_ESM).unwrap();

            let location1 = Location::new("url1".into());
            let location2 = Location::new("url2".into());
            let location3 = Location::new("url3".into());
            plugin1.set_locations(vec![location1.clone(), location2.clone()]);
            plugin2.set_locations(vec![location1.clone(), location3.clone()]);

            plugin1.merge_metadata(&plugin2);

            assert_eq!(
                &[location1.clone(), location2.clone(), location3.clone()],
                plugin1.locations()
            );
        }
    }

    #[test]
    fn unset_group_should_set_group_to_none() {
        let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();

        plugin.set_group("group1".into());
        plugin.unset_group();

        assert!(plugin.group().is_none());
    }

    mod has_name_only {
        use super::*;

        #[test]
        fn should_be_true_if_only_a_name_is_set() {
            assert!(PluginMetadata::new(BLANK_ESM).unwrap().has_name_only());
        }

        #[test]
        fn should_be_false_if_a_group_is_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_group("group1".into());

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_load_after_files_are_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_load_after_files(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_requirements_are_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_requirements(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_incompatibilities_are_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_incompatibilities(vec![File::new(BLANK_DIFFERENT_ESM.into())]);

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_messages_are_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_messages(vec![Message::new(MessageType::Say, "content1".into())]);

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_tags_are_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_tags(vec![Tag::new("Relev".into(), TagSuggestion::Addition)]);

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_dirty_info_is_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_dirty_info(vec![PluginCleaningData::new(0x1234_5678, "util1".into())]);

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_clean_info_is_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_clean_info(vec![PluginCleaningData::new(0x1234_5678, "util1".into())]);

            assert!(!plugin.has_name_only());
        }

        #[test]
        fn should_be_false_if_locations_are_set() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_locations(vec![Location::new("url1".into())]);

            assert!(!plugin.has_name_only());
        }
    }

    mod is_regex_plugin {
        use super::*;

        #[test]
        fn should_be_false_for_an_empty_name() {
            let plugin = PluginMetadata::new("").unwrap();

            assert!(!plugin.is_regex_plugin());
        }

        #[test]
        fn should_be_false_for_an_exact_plugin_name() {
            let plugin = PluginMetadata::new(BLANK_ESM).unwrap();

            assert!(!plugin.is_regex_plugin());
        }

        #[test]
        fn should_be_true_if_the_plugin_name_contains_a_colon() {
            let plugin = PluginMetadata::new("Blank:.esm").unwrap();

            assert!(plugin.is_regex_plugin());
        }

        #[test]
        fn should_be_true_if_the_plugin_name_contains_a_backslash() {
            let plugin = PluginMetadata::new("Blank\\.esm").unwrap();

            assert!(plugin.is_regex_plugin());
        }

        #[test]
        fn should_be_true_if_the_plugin_name_contains_an_asterisk() {
            let plugin = PluginMetadata::new("Blank*.esm").unwrap();

            assert!(plugin.is_regex_plugin());
        }

        #[test]
        fn should_be_true_if_the_plugin_name_contains_a_question_mark() {
            let plugin = PluginMetadata::new("Blank?.esm").unwrap();

            assert!(plugin.is_regex_plugin());
        }

        #[test]
        fn should_be_true_if_the_plugin_name_contains_a_vertical_bar() {
            let plugin = PluginMetadata::new("Blank|.esm").unwrap();

            assert!(plugin.is_regex_plugin());
        }
    }

    mod as_yaml {
        use super::*;

        #[test]
        fn should_return_a_yaml_string_representation() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_load_after_files(vec![File::new("other.esp".into())]);
            let yaml = plugin.as_yaml();

            assert_eq!(
                format!(
                    "name: '{}'\nafter: ['{}']",
                    plugin.name(),
                    plugin.load_after[0].name()
                ),
                yaml
            );
        }
    }

    mod try_from_yaml {
        use crate::metadata::parse;

        use super::*;

        #[test]
        fn should_error_if_given_a_scalar() {
            let yaml = parse("name1");

            assert!(PluginMetadata::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_error_if_given_a_list() {
            let yaml = parse("[0, 1, 2]");

            assert!(PluginMetadata::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_store_all_given_data() {
            let yaml = parse(
                "
                name: 'Blank.esp'
                after:
                  - 'Blank.esm'
                req:
                  - 'Blank - Different.esm'
                inc:
                  - 'Blank - Different.esp'
                msg:
                  - type: say
                    content: 'content'
                tag:
                  - Relev
                dirty:
                  - crc: 0x5
                    util: 'utility'
                clean:
                  - crc: 0x6
                    util: 'utility'
                url:
                  - 'https://www.example.com'",
            );

            let plugin = PluginMetadata::try_from_yaml(&yaml).unwrap();

            assert_eq!(BLANK_ESP, plugin.name());
            assert_eq!(&[File::new(BLANK_ESM.into())], plugin.load_after_files());
            assert_eq!(
                &[File::new(BLANK_DIFFERENT_ESM.into())],
                plugin.requirements()
            );
            assert_eq!(
                &[File::new(BLANK_DIFFERENT_ESP.into())],
                plugin.incompatibilities()
            );
            assert_eq!(
                &[Message::new(MessageType::Say, "content".into())],
                plugin.messages()
            );
            assert_eq!(
                &[Tag::new("Relev".into(), TagSuggestion::Addition)],
                plugin.tags()
            );
            assert_eq!(
                &[PluginCleaningData::new(0x5, "utility".into())],
                plugin.dirty_info()
            );
            assert_eq!(
                &[PluginCleaningData::new(0x6, "utility".into())],
                plugin.clean_info()
            );
            assert_eq!(
                &[Location::new("https://www.example.com".into())],
                plugin.locations()
            );
        }

        #[test]
        fn should_not_error_if_regex_metadata_contains_dirty_or_clean_info() {
            let yaml = parse(
                "
                name: 'Blank\\.esp'
                dirty:
                  - crc: 0x5
                    util: 'utility'
                clean:
                  - crc: 0x6
                    util: 'utility'",
            );

            let plugin = PluginMetadata::try_from_yaml(&yaml).unwrap();

            assert_eq!("Blank\\.esp", plugin.name());
            assert_eq!(
                &[PluginCleaningData::new(0x5, "utility".into())],
                plugin.dirty_info()
            );
            assert_eq!(
                &[PluginCleaningData::new(0x6, "utility".into())],
                plugin.clean_info()
            );
        }

        #[test]
        fn should_error_if_regex_name_is_invalid() {
            let yaml = parse("{name: 'RagnvaldBook(Farengar(+Ragnvald)?)?\\.esp'}");

            assert!(PluginMetadata::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_error_if_a_field_that_should_be_an_array_is_not() {
            let yaml = parse("{name: 'Blank.esp', after: Blank.esm}");

            assert!(PluginMetadata::try_from_yaml(&yaml).is_err());
        }
    }

    mod emit_yaml {
        use super::*;
        use crate::metadata::emit;

        #[test]
        fn should_omit_group_if_not_set() {
            let plugin = PluginMetadata::new("test.esp").unwrap();
            let yaml = emit(&plugin);

            assert_eq!(format!("name: '{}'", plugin.name()), yaml);
        }

        #[test]
        fn should_emit_group_if_set() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_group("group1".into());
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\ngroup: '{}'",
                    plugin.name(),
                    plugin.group.as_ref().unwrap()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_a_single_scalar_load_after_file_in_flow_style() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_load_after_files(vec![File::new("other.esp".into())]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\nafter: ['{}']",
                    plugin.name(),
                    plugin.load_after[0].name()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_a_single_non_scalar_load_after_file_in_block_style() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_load_after_files(vec![
                File::new("other.esp".into()).with_condition("condition1".into()),
            ]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\nafter:\n  - name: '{}'\n    condition: '{}'",
                    plugin.name(),
                    plugin.load_after[0].name(),
                    plugin.load_after[0].condition().unwrap(),
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_multiple_load_after_files_in_block_style() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_load_after_files(vec![
                File::new("other1.esp".into()),
                File::new("other2.esp".into()),
            ]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\nafter:\n  - '{}'\n  - '{}'",
                    plugin.name(),
                    plugin.load_after[0].name(),
                    plugin.load_after[1].name(),
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_a_single_scalar_requirements_in_flow_style() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_requirements(vec![File::new("other.esp".into())]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\nreq: ['{}']",
                    plugin.name(),
                    plugin.requirements[0].name()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_a_single_scalar_incompatibility_in_flow_style() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_incompatibilities(vec![File::new("other.esp".into())]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\ninc: ['{}']",
                    plugin.name(),
                    plugin.incompatibilities[0].name()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_messages() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_messages(vec![
                Message::new(MessageType::Say, "content1".into()),
                Message::new(MessageType::Say, "content2".into()),
            ]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\nmsg:\n  - type: {}\n    content: '{}'\n  - type: {}\n    content: '{}'",
                    plugin.name(),
                    plugin.messages[0].message_type(),
                    plugin.messages[0].content()[0].text(),
                    plugin.messages[1].message_type(),
                    plugin.messages[1].content()[0].text(),
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_a_single_scalar_tag_in_flow_style() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_tags(vec![Tag::new("Relev".into(), TagSuggestion::Addition)]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\ntag: [{}]",
                    plugin.name(),
                    plugin.tags[0].name()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_dirty_info() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_dirty_info(vec![PluginCleaningData::new(0xDEAD_BEEF, "utility".into())]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\ndirty:\n  - crc: 0x{:8X}\n    util: '{}'",
                    plugin.name(),
                    plugin.dirty_info[0].crc(),
                    plugin.dirty_info[0].cleaning_utility()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_clean_info() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_clean_info(vec![PluginCleaningData::new(0xDEAD_BEEF, "utility".into())]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\nclean:\n  - crc: 0x{:8X}\n    util: '{}'",
                    plugin.name(),
                    plugin.clean_info[0].crc(),
                    plugin.clean_info[0].cleaning_utility()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_a_single_scalar_location_in_flow_style() {
            let mut plugin = PluginMetadata::new("test.esp").unwrap();
            plugin.set_locations(vec![Location::new("https://www.example.com".into())]);
            let yaml = emit(&plugin);

            assert_eq!(
                format!(
                    "name: '{}'\nurl: ['{}']",
                    plugin.name(),
                    plugin.locations[0].url()
                ),
                yaml
            );
        }
    }

    mod replace_capturing_groups {
        use super::*;

        #[test]
        fn should_replace_capturing_groups_with_non_capturing_groups() {
            let input = r"(a)?\((b)\)((?x)|\\(d))(?:e)\(";

            let output = replace_capturing_groups(input);

            assert_eq!(r"(?:a)?\((?:b)\)(?:(?x)|\\(?:d))(?:e)\(", output);
        }

        #[test]
        fn should_not_clone_string_if_no_opening_parentheses_are_found() {
            let input = "no parentheses";

            match replace_capturing_groups(input) {
                Cow::Borrowed(output) => assert_eq!(input, output),
                Cow::Owned(output) => panic!("Expected borrowed output, got \"{output}\""),
            }
        }

        #[test]
        fn should_not_clone_string_if_no_capturing_groups_are_found() {
            let input = "no paren(?:thes)es";

            match replace_capturing_groups(input) {
                Cow::Borrowed(output) => assert_eq!(input, output),
                Cow::Owned(output) => panic!("Expected borrowed output, got \"{output}\""),
            }

            let input = "no paren(?:th(?:e)s)es";

            match replace_capturing_groups(input) {
                Cow::Borrowed(output) => assert_eq!(input, output),
                Cow::Owned(output) => panic!("Expected borrowed output, got \"{output}\""),
            }
        }

        #[test]
        fn should_clone_non_capturing_prefix_correctly() {
            let input = r"(?:a)?\(?:(b)\)((?x)|\\(d))(?:e)\(";

            let output = replace_capturing_groups(input);

            assert_eq!(r"(?:a)?\(?:(?:b)\)(?:(?x)|\\(?:d))(?:e)\(", output);
        }
    }
}
