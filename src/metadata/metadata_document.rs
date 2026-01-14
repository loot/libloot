use std::{
    collections::{HashMap, HashSet},
    path::Path,
    sync::Arc,
};

use saphyr::{LoadableYamlNode, MarkedYaml, YamlData};

use crate::{
    escape_ascii, logging,
    metadata::{
        File, MessageContent, PreludeDiffSpan, error::MetadataParsingErrorReason,
        message::emit_message_contents, yaml::YamlAnchors,
    },
};

use super::{
    error::{
        ExpectedType, LoadMetadataError, MetadataDocumentParsingError, ParseMetadataError,
        RegexError, WriteMetadataError,
    },
    file::Filename,
    group::Group,
    message::Message,
    plugin_metadata::PluginMetadata,
    yaml::{
        EmitYaml, TryFromYaml, YamlEmitter, YamlObjectType, get_slice_value, process_merge_keys,
    },
};

/// Options to configure how metadata files are written.
#[derive(Clone, Debug, Default)]
#[expect(
    missing_copy_implementations,
    reason = "Omitted Copy to allow this to become non-Copy-compatible without breaking backwards compatibility."
)]
#[expect(
    clippy::struct_excessive_bools,
    reason = "It's a config object, the bools make sense."
)]
pub struct MetadataWriteOptions {
    truncate: bool,
    write_anchors: bool,
    write_common_section: bool,
    anchor_file_strings: bool,
}

impl MetadataWriteOptions {
    /// Creates a new set of options, all initially set to `false`.
    pub fn new() -> Self {
        MetadataWriteOptions {
            truncate: false,
            write_anchors: false,
            write_common_section: false,
            anchor_file_strings: false,
        }
    }

    /// Sets the option to overwrite the output file if it already exists.
    ///
    /// If true and the file path already exists, its contents will be replaced.
    ///
    /// If false and the file path already exists, an error will be returned.
    ///
    /// This setting has no effect if the file path does not exist.
    pub fn set_truncate(&mut self, truncate: bool) {
        self.truncate = truncate;
    }

    /// Sets the option to write YAML anchors and aliases.
    ///
    /// If true then conditions, constraints, files, file details, plugin
    /// cleaning data details, messages and message contents that appear more
    /// than once in the metadata will be deduplicated by including a YAML
    /// anchor when writing the first occurrence of the value, and writing
    /// YAML aliases in place of further occurrences.
    ///
    /// If false, YAML anchors and aliases will not be used, so no deduplication
    /// will occur.
    pub fn set_write_anchors(&mut self, write_anchors: bool) {
        self.write_anchors = write_anchors;
    }

    /// Sets the option to write YAML anchors in a `common` section.
    ///
    /// If `write_anchors` is true and this is also true, the document's
    /// root-level map will start with a `common` key. Its value will be a list
    /// of all the values for which YAML anchors will be written, so that all
    /// YAML anchors will appear within that list.
    ///
    /// This setting has no effect if `write_anchors` is false.
    pub fn set_write_common_section(&mut self, write_common_section: bool) {
        self.write_common_section = write_common_section;
    }

    /// Sets the option to write anchors for [File] values that only have a
    /// name.
    ///
    /// If `write_anchors` is true and this is also true, then all repeated
    /// [File] metadata values will be deduplicated using YAML anchors and
    /// aliases.
    ///
    /// If this is false, then only [File] metadata that is serialised
    /// as a YAML object will be deduplicated (i.e. [File] values that only have
    /// a name will not be deduplicated).
    ///
    /// This setting has no effect if `write_anchors` is false.
    pub fn set_anchor_file_strings(&mut self, anchor_file_strings: bool) {
        self.anchor_file_strings = anchor_file_strings;
    }

    /// Gets the current value of the `truncate` option.
    pub fn truncate(&self) -> bool {
        self.truncate
    }

    /// Gets the current value of the `write_anchors` option.
    pub fn write_anchors(&self) -> bool {
        self.write_anchors
    }

    /// Gets the current value of the `write_common_section` option.
    pub fn write_common_section(&self) -> bool {
        self.write_common_section
    }

    /// Gets the current value of the `anchor_file_strings` option.
    pub fn anchor_file_strings(&self) -> bool {
        self.anchor_file_strings
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct MetadataDocument {
    bash_tags: Vec<String>,
    groups: Vec<Group>,
    messages: Vec<Message>,
    plugins: HashMap<Arc<Filename>, PluginMetadata>,
    regex_plugins: Vec<PluginMetadata>,
    ordered_plugin_names: Vec<Arc<Filename>>,
}

impl MetadataDocument {
    pub(crate) fn load(&mut self, file_path: &Path) -> Result<(), LoadMetadataError> {
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

    pub(crate) fn load_with_prelude(
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

        let masterlist = replace_prelude(masterlist, &prelude).map_err(|e| {
            LoadMetadataError::new(
                masterlist_path.into(),
                MetadataDocumentParsingError::MetadataParsingError(e),
            )
        })?;

        self.load_from_str(&masterlist.masterlist)
            .map_err(|mut e| {
                match &mut e {
                    MetadataDocumentParsingError::MetadataParsingError(err) => {
                        if let Some(meta) = masterlist.meta {
                            err.adjust_location(&meta);
                        }
                    }
                    MetadataDocumentParsingError::YamlMergeKeyError(err) => {
                        if let Some(meta) = masterlist.meta {
                            err.adjust_location(&meta);
                        }
                    }
                    _ => {
                        // No location to adjust.
                    }
                }
                LoadMetadataError::new(masterlist_path.into(), e)
            })?;

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

        let mut plugins = HashMap::new();
        let mut regex_plugins = Vec::new();
        let mut ordered_plugin_names = Vec::new();
        for plugin_yaml in get_slice_value(&doc, "plugins", YamlObjectType::MetadataDocument)? {
            let plugin = PluginMetadata::try_from_yaml(plugin_yaml)?;
            let filename = Arc::new(Filename::new(plugin.name().to_owned()));

            if plugin.is_regex_plugin() {
                regex_plugins.push(plugin);
            } else if let Some(old) = plugins.insert(Arc::clone(&filename), plugin) {
                return Err(ParseMetadataError::duplicate_entry(
                    plugin_yaml.span.start,
                    old.name().to_owned(),
                    YamlObjectType::PluginMetadata,
                )
                .into());
            }

            ordered_plugin_names.push(filename);
        }

        let messages = get_slice_value(&doc, "globals", YamlObjectType::MetadataDocument)?
            .iter()
            .map(Message::try_from_yaml)
            .collect::<Result<Vec<_>, _>>()?;

        let mut bash_tags = Vec::new();
        for bash_tag_yaml in get_slice_value(&doc, "bash_tags", YamlObjectType::MetadataDocument)? {
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

            bash_tags.push(bash_tag.to_owned());
        }

        let mut group_names = HashSet::new();
        let mut groups = Vec::new();
        for group_yaml in get_slice_value(&doc, "groups", YamlObjectType::MetadataDocument)? {
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
        self.ordered_plugin_names = ordered_plugin_names;
        self.messages = messages;
        self.bash_tags = bash_tags;
        self.groups = groups;

        Ok(())
    }

    pub(crate) fn save(
        &self,
        file_path: &Path,
        options: &MetadataWriteOptions,
    ) -> Result<(), WriteMetadataError> {
        logging::trace!("Saving metadata list to: \"{}\"", escape_ascii(file_path));

        let plugins: Vec<_> = self.ordered_plugins_iter().collect();

        let mut anchors = YamlAnchors::new();

        let common_values = if options.write_anchors {
            get_common_metadata_values(&self.messages, &plugins, options.anchor_file_strings)
        } else {
            CommonValues::default()
        };

        set_anchors(&mut anchors, &common_values);

        let mut emitter = YamlEmitter::new(anchors);

        emitter.begin_map();

        if options.write_anchors && options.write_common_section {
            emit_common_values(&common_values, &mut emitter);
        }

        if !self.bash_tags.is_empty() {
            emitter.write_map_key("bash_tags");

            emitter.begin_array();

            for tag in &self.bash_tags {
                emitter.write_unquoted_str(tag);
            }

            emitter.end_array();
            emitter.write_end_of_line();
        }

        if self.groups.len() > 1 {
            emitter.write_map_key("groups");
            write_list_with_line_breaks(self.groups.iter(), &mut emitter);
        }

        if !self.messages.is_empty() {
            emitter.write_map_key("globals");
            write_list_with_line_breaks(self.messages.iter(), &mut emitter);
        }

        if !plugins.is_empty() {
            emitter.write_map_key("plugins");
            write_list_with_line_breaks(
                plugins.into_iter().filter(|p| !p.has_name_only()),
                &mut emitter,
            );
        }

        emitter.end_map();

        let mut contents = emitter.into_string();
        if contents.is_empty() {
            contents = "{}".into();
        }

        std::fs::write(file_path, contents)
            .map_err(|e| WriteMetadataError::new(file_path.into(), e.into()))?;

        Ok(())
    }

    pub(crate) fn bash_tags(&self) -> &[String] {
        &self.bash_tags
    }

    pub(crate) fn groups(&self) -> &[Group] {
        &self.groups
    }

    pub(crate) fn messages(&self) -> &[Message] {
        &self.messages
    }

    pub(crate) fn ordered_plugins_iter(&self) -> impl Iterator<Item = &PluginMetadata> {
        self.ordered_plugin_names.iter().filter_map(|f| {
            self.plugins.get(f).or_else(|| {
                self.regex_plugins
                    .iter()
                    .find(|r| r.name() == f.as_ref().as_str())
            })
        })
    }

    pub(crate) fn find_plugin(
        &self,
        plugin_name: &str,
    ) -> Result<Option<PluginMetadata>, RegexError> {
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

    pub(crate) fn set_bash_tags(&mut self, bash_tags: Vec<String>) {
        self.bash_tags = bash_tags;
    }

    pub(crate) fn set_groups(&mut self, groups: Vec<Group>) {
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

    pub(crate) fn set_messages(&mut self, messages: Vec<Message>) {
        self.messages = messages;
    }

    pub(crate) fn set_plugin_metadata(&mut self, plugin_metadata: PluginMetadata) {
        let filename = Arc::new(Filename::new(plugin_metadata.name().to_owned()));

        if plugin_metadata.is_regex_plugin() {
            self.regex_plugins.push(plugin_metadata);
            self.ordered_plugin_names.push(filename);
        } else {
            let old_value = self.plugins.insert(Arc::clone(&filename), plugin_metadata);
            if old_value.is_none() {
                self.ordered_plugin_names.push(filename);
            }
        }
    }

    pub(crate) fn remove_plugin_metadata(&mut self, plugin_name: &str) {
        let filename = Filename::new(plugin_name.to_owned());
        let mut was_removed = self.plugins.remove(&filename).is_some();

        // Only remove regex plugins if no specific plugin was removed, because
        // they're mutually exclusive.
        if !was_removed {
            self.regex_plugins.retain(|p| {
                let equal = unicase::eq(p.name(), plugin_name);
                if equal {
                    was_removed = true;
                }
                !equal
            });
        }

        if was_removed {
            self.ordered_plugin_names
                .retain(|f| f.as_ref() != &filename);
        }
    }

    pub(crate) fn clear(&mut self) {
        self.bash_tags.clear();
        self.groups.clear();
        self.messages.clear();
        self.plugins.clear();
        self.regex_plugins.clear();
        self.ordered_plugin_names.clear();
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
            ordered_plugin_names: Vec::default(),
        }
    }
}

#[derive(Debug, Clone)]
struct OrderedCounts<T> {
    order: Vec<T>,
    counts: HashMap<T, u32>,
}

impl<T> OrderedCounts<T> {
    fn new() -> Self {
        Self {
            order: Vec::new(),
            counts: HashMap::new(),
        }
    }
}

impl<T: Eq + std::hash::Hash + Copy> OrderedCounts<T> {
    fn increment_for(&mut self, value: T) -> u32 {
        *self
            .counts
            .entry(value)
            .and_modify(|e| *e += 1)
            .or_insert_with(|| {
                self.order.push(value);
                1
            })
    }

    fn into_common_values(self) -> Vec<T> {
        self.order
            .into_iter()
            .filter(|e| self.counts.get(e).is_some_and(|c| *c > 1))
            .collect()
    }
}

type CommonValues<'a> = (
    Vec<&'a File>,
    Vec<&'a Message>,
    Vec<&'a [MessageContent]>,
    Vec<&'a str>,
);

fn get_common_metadata_values<'a>(
    general_messages: &'a [Message],
    plugins: &[&'a PluginMetadata],
    anchor_scalar_files: bool,
) -> CommonValues<'a> {
    let mut file_counts = OrderedCounts::new();
    let mut message_counts = OrderedCounts::new();
    let mut message_contents_counts = OrderedCounts::new();
    let mut condition_counts = OrderedCounts::new();

    for message in general_messages {
        count_message_values(
            message,
            &mut message_counts,
            &mut message_contents_counts,
            &mut condition_counts,
        );
    }

    for plugin in plugins {
        for file in plugin.load_after_files() {
            count_file_values(
                file,
                anchor_scalar_files,
                &mut file_counts,
                &mut message_contents_counts,
                &mut condition_counts,
            );
        }

        for file in plugin.requirements() {
            count_file_values(
                file,
                anchor_scalar_files,
                &mut file_counts,
                &mut message_contents_counts,
                &mut condition_counts,
            );
        }

        for file in plugin.incompatibilities() {
            count_file_values(
                file,
                anchor_scalar_files,
                &mut file_counts,
                &mut message_contents_counts,
                &mut condition_counts,
            );
        }

        for message in plugin.messages() {
            count_message_values(
                message,
                &mut message_counts,
                &mut message_contents_counts,
                &mut condition_counts,
            );
        }

        for tag in plugin.tags() {
            if let Some(condition) = tag.condition() {
                condition_counts.increment_for(condition);
            }
        }

        for info in plugin.dirty_info() {
            if !info.detail().is_empty() {
                message_contents_counts.increment_for(info.detail());
            }
        }

        for info in plugin.clean_info() {
            if !info.detail().is_empty() {
                message_contents_counts.increment_for(info.detail());
            }
        }
    }

    (
        file_counts.into_common_values(),
        message_counts.into_common_values(),
        message_contents_counts.into_common_values(),
        condition_counts.into_common_values(),
    )
}

fn set_anchors<'a>(anchors: &mut YamlAnchors<'a>, common_values: &CommonValues<'a>) {
    let (files, messages, message_contents, conditions) = common_values;

    let file_anchors = to_anchor_map(files, "file");
    let message_anchors = to_anchor_map(messages, "message");
    let message_contents_anchors = to_anchor_map(message_contents, "contents");
    let condition_anchors = to_anchor_map(conditions, "condition");

    anchors.set_message_anchors(message_anchors);
    anchors.set_message_contents_anchors(message_contents_anchors);
    anchors.set_file_anchors(file_anchors);
    anchors.set_condition_anchors(condition_anchors);
}

fn count_message_values<'a>(
    message: &'a Message,
    message_counts: &mut OrderedCounts<&'a Message>,
    message_contents_counts: &mut OrderedCounts<&'a [crate::metadata::MessageContent]>,
    condition_counts: &mut OrderedCounts<&'a str>,
) {
    let message_count = message_counts.increment_for(message);

    if message_count > 1 {
        return;
    }

    if !message.content().is_empty() {
        message_contents_counts.increment_for(message.content());
    }

    if let Some(condition) = message.condition() {
        condition_counts.increment_for(condition);
    }
}

fn count_file_values<'a>(
    file: &'a crate::metadata::File,
    anchor_scalar_files: bool,
    file_counts: &mut OrderedCounts<&'a crate::metadata::File>,
    message_contents_counts: &mut OrderedCounts<&'a [crate::metadata::MessageContent]>,
    condition_counts: &mut OrderedCounts<&'a str>,
) {
    if anchor_scalar_files || !file.is_scalar() {
        let file_count = file_counts.increment_for(file);

        if file_count > 1 {
            return;
        }
    }

    if !file.detail().is_empty() {
        message_contents_counts.increment_for(file.detail());
    }

    if let Some(condition) = file.condition() {
        condition_counts.increment_for(condition);
    }

    if let Some(constraint) = file.constraint() {
        condition_counts.increment_for(constraint);
    }
}

fn to_anchor_map<'a, T: Eq + std::hash::Hash + ?Sized>(
    values: &[&'a T],
    anchor_prefix: &str,
) -> HashMap<&'a T, String> {
    values
        .iter()
        .enumerate()
        .map(|(i, e)| (*e, format!("{}{}", anchor_prefix, i + 1)))
        .collect()
}

fn emit_common_values(common_values: &CommonValues, emitter: &mut YamlEmitter) {
    let (files, messages, message_contents, conditions) = common_values;

    if files.is_empty()
        && messages.is_empty()
        && message_contents.is_empty()
        && conditions.is_empty()
    {
        return;
    }

    emitter.write_map_key("common");

    emitter.begin_array();
    for contents in message_contents {
        let Some(anchor) = emitter.anchors.message_contents_anchor(contents) else {
            // Should be impossible.
            logging::error!("Found a common message contents without an anchor: {contents:?}");
            continue;
        };

        if !emitter.anchors.is_anchor_written(anchor) {
            emit_message_contents(contents, emitter);
            emitter.write_end_of_line();
        }
    }
    emitter.end_array();

    emitter.begin_array();
    for condition in conditions {
        let Some(anchor) = emitter.anchors.condition_anchor(condition) else {
            // Should be impossible.
            logging::error!("Found a common condition without an anchor: {condition}");
            continue;
        };

        if !emitter.anchors.is_anchor_written(anchor) {
            emitter.write_condition(condition);
            emitter.write_end_of_line();
        }
    }
    emitter.end_array();

    write_list_with_line_breaks(messages.iter(), emitter);

    write_list_with_line_breaks(files.iter(), emitter);
}

fn write_list_with_line_breaks<'a, T, U>(values: T, emitter: &mut YamlEmitter)
where
    T: Iterator<Item = &'a U>,
    U: 'a + EmitYaml,
{
    emitter.begin_array();

    for value in values {
        value.emit_yaml(emitter);
        emitter.write_end_of_line();
    }

    emitter.end_array();
}

struct MasterlistWithReplacedPrelude {
    masterlist: String,
    meta: Option<PreludeDiffSpan>,
}

fn replace_prelude(
    masterlist: String,
    prelude: &str,
) -> Result<MasterlistWithReplacedPrelude, ParseMetadataError> {
    if let Some(SplitMasterlist {
        before_prelude,
        prelude: old_prelude,
        after_prelude,
    }) = split_on_prelude(&masterlist)
    {
        let prelude = indent_prelude(prelude);

        let masterlist = if after_prelude.is_empty() {
            format!("{before_prelude}\n{prelude}")
        } else {
            format!("{before_prelude}\n{prelude}\n{after_prelude}")
        };

        // The old prelude included leading and trailing newlines but the new prelude doesn't.
        let count_modifier = if after_prelude.is_empty() { 1 } else { 2 };
        let old_prelude_line_count = count_lines(old_prelude).saturating_sub(count_modifier);

        // Add one because before_prelude does not include the line break after the prelude: key.
        let prelude_start_line = count_lines(before_prelude) + 1;
        let new_prelude_line_count = count_lines(&prelude);
        // The last line of the prelude.
        let prelude_end_line = prelude_start_line + new_prelude_line_count.saturating_sub(1);

        // TODO: usize::checked_signed_diff() is stable as of Rust 1.91.0, but the org.freedesktop.Sdk.Extension.rust-stable extension for LOOT's Flatpak build environment supplies an older version of Rust, so this can only be updated to use checked_signed_diff() once it's available in that environment.
        let lines_added_count = if new_prelude_line_count >= old_prelude_line_count {
            isize::try_from(new_prelude_line_count - old_prelude_line_count)
        } else {
            isize::try_from(old_prelude_line_count - new_prelude_line_count).map(std::ops::Neg::neg)
        };

        let lines_added_count = lines_added_count.map_err(|_e| {
            ParseMetadataError::new(
                saphyr::Marker::new(before_prelude.len(), prelude_start_line, 0),
                MetadataParsingErrorReason::PreludeSubstitutionOverflow {
                    new_prelude_count: new_prelude_line_count,
                    old_prelude_count: old_prelude_line_count,
                },
            )
        })?;

        Ok(MasterlistWithReplacedPrelude {
            masterlist,
            meta: Some(PreludeDiffSpan {
                start_line: prelude_start_line,
                end_line: prelude_end_line,
                lines_added_count,
            }),
        })
    } else {
        Ok(MasterlistWithReplacedPrelude {
            masterlist,
            meta: None,
        })
    }
}

struct SplitMasterlist<'a> {
    before_prelude: &'a str,
    prelude: &'a str,
    after_prelude: &'a str,
}

fn split_on_prelude(masterlist: &str) -> Option<SplitMasterlist<'_>> {
    let (prefix, remainder) = split_on_prelude_start(masterlist)?;

    let mut iter = remainder.bytes().enumerate().peekable();
    while let Some((index, byte)) = iter.next() {
        if byte != b'\n' {
            continue;
        }

        if let Some((_, next_byte)) = iter.peek()
            && !matches!(next_byte, b' ' | b'#' | b'\n' | b'\r')
        {
            // LIMITATION: Slicing at index should never fail, but the
            // compiler can't see that.
            if let Some((prelude, suffix)) = remainder.split_at_checked(index + 1) {
                return Some(SplitMasterlist {
                    before_prelude: prefix,
                    prelude,
                    after_prelude: suffix,
                });
            }

            logging::error!(
                "Unexpectedly failed to slice the masterlist on a new line at index {}",
                prefix.len() + index
            );
        }
    }

    Some(SplitMasterlist {
        before_prelude: prefix,
        prelude: remainder,
        after_prelude: "",
    })
}

fn count_lines(string: &str) -> usize {
    // lines() ignores a trailing line ending.
    let count = string.lines().count();
    if string.ends_with('\n') {
        count + 1
    } else {
        count
    }
}

/// This includes the line break following the "prelude:" YAML as part of the
/// tuple's second element.
fn split_on_prelude_start(masterlist: &str) -> Option<(&str, &str)> {
    let prelude_on_first_line = "prelude:";
    let prelude_on_new_line = "\nprelude:";

    if let Some(remainder) = masterlist.strip_prefix(prelude_on_first_line) {
        Some((prelude_on_first_line, remainder))
    } else {
        if let Some(pos) = masterlist.find(prelude_on_new_line) {
            let index = pos + prelude_on_new_line.len();
            // LIMITATION: A checked split shouldn't be necessary, but there's
            // no split_inclusive_once() method, so we need to find and split in
            // two steps and there's always the risk of a bug being introduced
            // in the middle.
            if let Some((prefix, remainder)) = masterlist.split_at_checked(index) {
                return Some((prefix, remainder));
            }

            logging::error!(
                "Unexpectedly failed to split the masterlist on the start of the prelude key's value at index {index}"
            );
        }
        None
    }
}

fn indent_prelude(prelude: &str) -> String {
    let prelude = ("  ".to_owned() + &prelude.replace('\n', "\n  "))
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
        use std::error::Error;

        use crate::metadata::{MessageType, PluginCleaningData, Tag, TagSuggestion};

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

            let mut plugin_names_iter = metadata_list
                .ordered_plugins_iter()
                .map(PluginMetadata::name);
            assert_eq!("Blank.esm", plugin_names_iter.next().unwrap());
            assert_eq!("Blank.+\\.esp", plugin_names_iter.next().unwrap());
            assert_eq!(
                "Blank.+(Different)?.*\\.esp",
                plugin_names_iter.next().unwrap()
            );
            assert_eq!("Blank.esp", plugin_names_iter.next().unwrap());
            assert_eq!(None, plugin_names_iter.next());

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
        fn load_with_prelude_should_not_change_yaml_parse_error_line_numbers_if_they_are_before_the_prelude()
         {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "common:\n  - *invalid\nprelude:\n  - &ref\n    type: say\n    content: Loaded from same file\n\n  - &otherRef\n    type: error\n    content: Error from same file\nglobals:\n  - *ref\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    type: say\n    content: Loaded from prelude\n\n  - &otherRef\n    type: error\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            let err_message = metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap_err()
                .source()
                .unwrap()
                .source()
                .unwrap()
                .to_string();

            assert_eq!(
                "encountered a YAML parsing error at line 2 column 5: while parsing node, found unknown anchor",
                err_message
            );
        }

        #[test]
        fn load_with_prelude_should_change_yaml_parse_error_line_numbers_if_within_the_prelude() {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "prelude:\n  - &ref\n    type: say\n    content: Loaded from same file\n\n  - &otherRef\n    type: error\n    content: Error from same file\nglobals:\n  - *ref\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    type\n    content: Loaded from prelude\n\n  - &otherRef\n    type: error\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            let err_message = metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap_err()
                .source()
                .unwrap()
                .source()
                .unwrap()
                .to_string();

            assert_eq!(
                "encountered a YAML parsing error at line 4 column 12 within the prelude: mapping values are not allowed in this context",
                err_message
            );
        }

        #[test]
        fn load_with_prelude_should_change_yaml_parse_error_line_numbers_if_they_are_after_the_prelude()
         {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "prelude:\n  - &ref\n    type: say\n    content: Loaded from same file\n\n  - &otherRef\n    type: error\n    content: Error from same file\nglobals:\n  - *invalid\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    type: say\n    content: Loaded from prelude\n\n  - &otherRef\n    type: error\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            let err_message = metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap_err()
                .source()
                .unwrap()
                .source()
                .unwrap()
                .to_string();

            assert_eq!(
                "encountered a YAML parsing error at line 10 column 5: while parsing node, found unknown anchor",
                err_message
            );
        }

        #[test]
        fn load_with_prelude_should_not_change_yaml_merge_key_error_line_numbers_if_they_are_before_the_prelude()
         {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "common:\n  - <<: 1\nprelude:\n  - &ref\n    type: say\n    content: Loaded from same file\n\n  - &otherRef\n    type: error\n    content: Error from same file\nglobals:\n  - *ref\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    type: say\n    content: Loaded from prelude\n\n  - &otherRef\n    type: error\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            let err_message = metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap_err()
                .source()
                .unwrap()
                .source()
                .unwrap()
                .to_string();

            assert_eq!(
                "invalid YAML merge key value at line 2 column 9: 1",
                err_message
            );
        }

        #[test]
        fn load_with_prelude_should_change_yaml_merge_key_error_line_numbers_if_within_the_prelude()
        {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "prelude:\n  - &ref\n    type: say\n    content: Loaded from same file\n\n  - &otherRef\n    type: error\n    content: Error from same file\nglobals:\n  - *ref\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    <<: type\n    content: Loaded from prelude\n\n  - &otherRef\n    type: error\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            let err_message = metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap_err()
                .source()
                .unwrap()
                .source()
                .unwrap()
                .to_string();

            assert_eq!(
                "invalid YAML merge key value at line 3 column 9 within the prelude: type",
                err_message
            );
        }

        #[test]
        fn load_with_prelude_should_change_yaml_merge_key_error_line_numbers_if_they_are_after_the_prelude()
         {
            let tmp_dir = tempdir().unwrap();

            let masterlist_path = tmp_dir.path().join("masterlist.yaml");
            std::fs::write(&masterlist_path, "prelude:\n  - &ref\n    type: say\n    content: Loaded from same file\n\n  - &otherRef\n    type: error\n    content: Error from same file\nglobals:\n  - <<: 1\n  - *otherRef").unwrap();

            let prelude_path = tmp_dir.path().join("prelude.yaml");
            std::fs::write(
                &prelude_path,
                "common:\n  - &ref\n    type: say\n    content: Loaded from prelude\n\n  - &otherRef\n    type: error\n    content: An error message",
            )
            .unwrap();

            let mut metadata_list = MetadataDocument::default();
            let err_message = metadata_list
                .load_with_prelude(&masterlist_path, &prelude_path)
                .unwrap_err()
                .source()
                .unwrap()
                .source()
                .unwrap()
                .to_string();

            assert_eq!(
                "invalid YAML merge key value at line 10 column 9: 1",
                err_message
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
            metadata
                .save(&other_path, &MetadataWriteOptions::default())
                .unwrap();

            let content = std::fs::read_to_string(&other_path).unwrap();

            assert_eq!(
                "bash_tags:
  - C.Climate
  - Relev

groups:
  - name: 'default'

  - name: 'group1'
    after:
      - group2

  - name: 'group2'
    after:
      - default

globals:
  - type: say
    content: 'A global message.'

plugins:
  - name: 'Blank.esm'
    msg:
      - type: warn
        content: 'This is a warning.'
      - type: say
        content: 'This message should be removed when evaluating conditions.'
        condition: 'active(\"Blank - Different.esm\")'

  - name: 'Blank.+\\.esp'
    after: [ 'Blank.esm' ]

  - name: 'Blank.+(Different)?.*\\.esp'
    inc: [ 'Blank.esp' ]

  - name: 'Blank.esp'
    group: 'group2'
    dirty:
      - crc: 0xDEADBEEF
        util: 'utility'
",
                content
            );

            let mut other_metadata = MetadataDocument::default();
            other_metadata.load(&other_path).unwrap();

            assert_eq!(metadata, other_metadata);
        }

        fn metadata_with_repeated_values() -> MetadataDocument {
            let mut metadata = MetadataDocument::default();

            let mut plugin = PluginMetadata::new("test1.esp").unwrap();

            let condition1 = "file(\"test.txt\")";
            let condition2 = "file(\"other.txt\")";
            let condition3 = "file(\"third.txt\")";
            let contents1 = vec![MessageContent::new("message text 1".to_owned())];
            let contents2 = vec![
                MessageContent::new("message text 1".to_owned()).with_language("en".to_owned()),
                MessageContent::new("message text 2".to_owned()).with_language("fr".to_owned()),
            ];
            let contents3 = vec![MessageContent::new("message text 3".to_owned())];
            let contents4 = vec![
                MessageContent::new("message text 1".to_owned()).with_language("en".to_owned()),
                MessageContent::new("message text 2".to_owned()).with_language("de".to_owned()),
            ];
            let file1 = File::new("file 1".to_owned());
            let file2 = File::new("file 2".to_owned())
                .with_condition(condition1.to_owned())
                .with_detail(contents1.clone())
                .unwrap();
            let file3 = File::new("file 3".to_owned())
                .with_condition(condition1.to_owned())
                .with_constraint(condition2.to_owned());
            let file4 = File::new("file 4".to_owned())
                .with_constraint(condition3.to_owned())
                .with_detail(contents3.clone())
                .unwrap();

            let message1 = Message::new(MessageType::Say, contents1[0].text().to_owned());
            let message2 = Message::multilingual(MessageType::Say, contents2.clone()).unwrap();
            let message3 = Message::multilingual(MessageType::Say, contents4.clone()).unwrap();

            let tag1 = Tag::new("Relev".to_owned(), TagSuggestion::Addition)
                .with_condition(condition2.to_owned());
            let tag2 = Tag::new("Delev".to_owned(), TagSuggestion::Addition)
                .with_condition(condition3.to_owned());

            let info1 = PluginCleaningData::new(0xDEAD_BEEF, "utility".to_owned())
                .with_detail(contents2.clone())
                .unwrap();
            let info2 = PluginCleaningData::new(0xDEAD_BEEF, "utility".to_owned())
                .with_detail(contents3.clone())
                .unwrap();

            plugin.set_load_after_files(vec![file1.clone()]);
            plugin.set_requirements(vec![file1.clone(), file2.clone()]);
            plugin.set_incompatibilities(vec![file2.clone(), file3.clone()]);
            plugin.set_messages(vec![message1.clone(), message2.clone(), message3]);
            plugin.set_tags(vec![tag1, tag2]);
            plugin.set_dirty_info(vec![info1]);
            plugin.set_clean_info(vec![info2]);

            metadata.set_plugin_metadata(plugin);

            let mut plugin = PluginMetadata::new("test2.esp").unwrap();

            plugin.set_messages(vec![message2]);
            plugin.set_load_after_files(vec![file4]);

            metadata.set_plugin_metadata(plugin);

            metadata
        }

        #[test]
        fn save_should_use_anchors_and_aliases_for_repeated_metadata_when_write_anchors_is_true() {
            let tmp_dir = tempdir().unwrap();

            let path = tmp_dir.path().join("masterlist.yaml");

            let metadata = metadata_with_repeated_values();

            let mut options = MetadataWriteOptions::new();
            options.set_write_anchors(true);
            options.set_anchor_file_strings(true);

            metadata.save(&path, &options).unwrap();

            let content = std::fs::read_to_string(&path).unwrap();

            assert_eq!(
                "plugins:
  - name: 'test1.esp'
    after: [ &file1 'file 1' ]
    req:
      - *file1
      - &file2
        name: 'file 2'
        detail: &contents1 'message text 1'
        condition: &condition1 'file(\"test.txt\")'
    inc:
      - *file2
      - name: 'file 3'
        condition: *condition1
        constraint: &condition2 'file(\"other.txt\")'
    msg:
      - type: say
        content: *contents1
      - &message1
        type: say
        content: &contents2
          - lang: en
            text: 'message text 1'
          - lang: fr
            text: 'message text 2'
      - type: say
        content:
          - lang: en
            text: 'message text 1'
          - lang: de
            text: 'message text 2'
    tag:
      - name: Relev
        condition: *condition2
      - name: Delev
        condition: &condition3 'file(\"third.txt\")'
    dirty:
      - crc: 0xDEADBEEF
        util: 'utility'
        detail: *contents2
    clean:
      - crc: 0xDEADBEEF
        util: 'utility'
        detail: &contents3 'message text 3'

  - name: 'test2.esp'
    after:
      - name: 'file 4'
        detail: *contents3
        constraint: *condition3
    msg: [ *message1 ]
",
                content
            );

            // Also check that the written metadata can be read correctly.
            let mut other_metadata = MetadataDocument::default();
            other_metadata.load(&path).unwrap();

            assert_eq!(metadata, other_metadata);
        }

        #[test]
        fn save_should_use_anchors_and_aliases_for_repeated_metadata_when_write_anchors_and_write_common_section_are_true()
         {
            let tmp_dir = tempdir().unwrap();

            let path = tmp_dir.path().join("masterlist.yaml");

            let metadata = metadata_with_repeated_values();

            let mut options = MetadataWriteOptions::new();
            options.set_write_anchors(true);
            options.set_write_common_section(true);
            options.set_anchor_file_strings(true);

            metadata.save(&path, &options).unwrap();

            let content = std::fs::read_to_string(&path).unwrap();

            assert_eq!(
                "common:
  - &contents1 'message text 1'

  - &contents2
    - lang: en
      text: 'message text 1'
    - lang: fr
      text: 'message text 2'

  - &contents3 'message text 3'

  - &condition1 'file(\"test.txt\")'

  - &condition2 'file(\"other.txt\")'

  - &condition3 'file(\"third.txt\")'

  - &message1
    type: say
    content: *contents2

  - &file1 'file 1'

  - &file2
    name: 'file 2'
    detail: *contents1
    condition: *condition1

plugins:
  - name: 'test1.esp'
    after: [ *file1 ]
    req:
      - *file1
      - *file2
    inc:
      - *file2
      - name: 'file 3'
        condition: *condition1
        constraint: *condition2
    msg:
      - type: say
        content: *contents1
      - *message1
      - type: say
        content:
          - lang: en
            text: 'message text 1'
          - lang: de
            text: 'message text 2'
    tag:
      - name: Relev
        condition: *condition2
      - name: Delev
        condition: *condition3
    dirty:
      - crc: 0xDEADBEEF
        util: 'utility'
        detail: *contents2
    clean:
      - crc: 0xDEADBEEF
        util: 'utility'
        detail: *contents3

  - name: 'test2.esp'
    after:
      - name: 'file 4'
        detail: *contents3
        constraint: *condition3
    msg: [ *message1 ]
",
                content
            );

            // Also check that the written metadata can be read correctly.
            let mut other_metadata = MetadataDocument::default();
            other_metadata.load(&path).unwrap();

            assert_eq!(metadata, other_metadata);
        }

        #[test]
        fn save_should_use_not_anchor_scalar_files_when_the_option_is_false() {
            let tmp_dir = tempdir().unwrap();

            let path = tmp_dir.path().join("masterlist.yaml");

            let metadata = metadata_with_repeated_values();

            let mut options = MetadataWriteOptions::new();
            options.set_write_anchors(true);
            options.set_write_common_section(true);

            metadata.save(&path, &options).unwrap();

            let content = std::fs::read_to_string(&path).unwrap();

            assert_eq!(
                "common:
  - &contents1 'message text 1'

  - &contents2
    - lang: en
      text: 'message text 1'
    - lang: fr
      text: 'message text 2'

  - &contents3 'message text 3'

  - &condition1 'file(\"test.txt\")'

  - &condition2 'file(\"other.txt\")'

  - &condition3 'file(\"third.txt\")'

  - &message1
    type: say
    content: *contents2

  - &file1
    name: 'file 2'
    detail: *contents1
    condition: *condition1

plugins:
  - name: 'test1.esp'
    after: [ 'file 1' ]
    req:
      - 'file 1'
      - *file1
    inc:
      - *file1
      - name: 'file 3'
        condition: *condition1
        constraint: *condition2
    msg:
      - type: say
        content: *contents1
      - *message1
      - type: say
        content:
          - lang: en
            text: 'message text 1'
          - lang: de
            text: 'message text 2'
    tag:
      - name: Relev
        condition: *condition2
      - name: Delev
        condition: *condition3
    dirty:
      - crc: 0xDEADBEEF
        util: 'utility'
        detail: *contents2
    clean:
      - crc: 0xDEADBEEF
        util: 'utility'
        detail: *contents3

  - name: 'test2.esp'
    after:
      - name: 'file 4'
        detail: *contents3
        constraint: *condition3
    msg: [ *message1 ]
",
                content
            );

            // Also check that the written metadata can be read correctly.
            let mut other_metadata = MetadataDocument::default();
            other_metadata.load(&path).unwrap();

            assert_eq!(metadata, other_metadata);
        }

        #[test]
        fn clear_should_clear_all_loaded_data() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            assert!(!metadata.messages().is_empty());
            assert!(!metadata.bash_tags().is_empty());
            assert!(!metadata.plugins.is_empty());
            assert!(!metadata.regex_plugins.is_empty());
            assert!(!metadata.ordered_plugin_names.is_empty());

            metadata.clear();

            assert!(metadata.messages().is_empty());
            assert!(metadata.bash_tags().is_empty());
            assert!(metadata.plugins.is_empty());
            assert!(metadata.regex_plugins.is_empty());
            assert!(metadata.ordered_plugin_names.is_empty());
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
        fn set_plugin_metadata_should_store_specific_plugin_metadata() {
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
        fn set_plugin_metadata_should_append_the_specific_plugin_filename_to_ordered_plugin_names_if_it_is_new()
         {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let name = "Blank - Other.esp";
            metadata.set_plugin_metadata(PluginMetadata::new(name).unwrap());

            assert_eq!(name, metadata.ordered_plugin_names.last().unwrap().as_str());
        }

        #[test]
        fn set_plugin_metadata_should_not_append_the_specific_plugin_filename_to_ordered_plugin_names_if_it_is_not_new()
         {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let ordered_plugin_names = metadata.ordered_plugin_names.clone();

            let name = "Blank.esp";
            metadata.set_plugin_metadata(PluginMetadata::new(name).unwrap());

            assert_eq!(ordered_plugin_names, metadata.ordered_plugin_names);
        }

        #[test]
        fn set_plugin_metadata_should_store_given_regex_plugin_metadata() {
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
        fn set_plugin_metadata_should_append_the_regex_plugin_filename_to_ordered_plugin_names() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let name = ".+Dependent\\.esp";
            metadata.set_plugin_metadata(PluginMetadata::new(name).unwrap());

            assert_eq!(name, metadata.ordered_plugin_names.last().unwrap().as_str());
        }

        #[test]
        fn remove_plugin_metadata_should_remove_the_given_plugin_specific_metadata() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let name = "Blank.esp";
            assert!(metadata.find_plugin(name).unwrap().is_some());

            metadata.remove_plugin_metadata(name);

            assert!(metadata.find_plugin(name).unwrap().is_none());

            assert!(
                !metadata
                    .ordered_plugin_names
                    .contains(&Arc::new(Filename::new(name.to_owned())))
            );
        }

        #[test]
        fn remove_plugin_metadata_should_remove_regex_entries_with_the_given_plugin_name() {
            let mut metadata = MetadataDocument::default();

            let regex_name = "Blank.*\\.esp";

            let mut plugin = PluginMetadata::new(regex_name).unwrap();
            plugin.set_load_after_files(vec![File::new("A".to_owned())]);
            metadata.set_plugin_metadata(plugin.clone());

            let mut plugin = PluginMetadata::new(regex_name).unwrap();
            plugin.set_load_after_files(vec![File::new("B".to_owned())]);
            metadata.set_plugin_metadata(plugin.clone());

            let name = "Blank.esp";

            assert!(metadata.find_plugin(name).unwrap().is_some());

            metadata.remove_plugin_metadata(regex_name);

            assert!(metadata.find_plugin(name).unwrap().is_none());

            assert!(
                !metadata
                    .ordered_plugin_names
                    .contains(&Arc::new(Filename::new(name.to_owned())))
            );
        }

        #[test]
        fn remove_plugin_metadata_should_remove_regex_entries_comparing_names_case_insensitively() {
            let mut metadata = MetadataDocument::default();

            let regex_name = "Blank.*\\.esp";

            let mut plugin = PluginMetadata::new(regex_name).unwrap();
            plugin.set_load_after_files(vec![File::new("A".to_owned())]);
            metadata.set_plugin_metadata(plugin.clone());

            let mut plugin = PluginMetadata::new(regex_name).unwrap();
            plugin.set_load_after_files(vec![File::new("B".to_owned())]);
            metadata.set_plugin_metadata(plugin.clone());

            let name = "Blank.esp";

            assert!(metadata.find_plugin(name).unwrap().is_some());

            metadata.remove_plugin_metadata("blank.*\\.esp");

            assert!(metadata.find_plugin(name).unwrap().is_none());

            assert!(
                !metadata
                    .ordered_plugin_names
                    .contains(&Arc::new(Filename::new(name.to_owned())))
            );
        }

        #[test]
        fn remove_plugin_metadata_should_not_remove_matching_regex_plugin_metadata() {
            let mut metadata = MetadataDocument::default();
            metadata.load_from_str(METADATA_LIST_YAML).unwrap();

            let name = "Blank.+\\.esp";
            assert!(metadata.find_plugin(name).unwrap().is_some());

            metadata.remove_plugin_metadata("Blank - Different.esp");

            assert!(metadata.find_plugin(name).unwrap().is_some());

            assert!(
                metadata
                    .ordered_plugin_names
                    .contains(&Arc::new(Filename::new(name.to_owned())))
            );
        }
    }

    mod get_common_metadata_values {
        use crate::metadata::MessageType;

        use super::*;

        #[test]
        fn get_common_metadata_values_should_skip_contents_and_conditions_within_messages_that_have_already_been_seen()
         {
            let content1 = "test message 1";
            let content2 = "test message 2";
            let condition1 = "condition 1";
            let condition2 = "condition 2";
            let message1 = Message::new(MessageType::Say, content1.to_owned())
                .with_condition(condition1.to_owned());
            let message2 = Message::new(MessageType::Say, content2.to_owned())
                .with_condition(condition2.to_owned());
            let message3 = Message::new(MessageType::Warn, content2.to_owned())
                .with_condition(condition2.to_owned());
            let general_messages = &[
                message1.clone(),
                message1.clone(),
                message2.clone(),
                message3,
            ];
            let plugins = &[];

            let (_, common_messages, common_contents, common_conditions) =
                get_common_metadata_values(general_messages, plugins, true);

            assert!(common_messages.contains(&&message1));
            assert!(!common_contents.contains(&message1.content()));
            assert!(!common_conditions.contains(&condition1));
            assert!(common_contents.contains(&message2.content()));
            assert!(common_conditions.contains(&condition2));
        }

        #[test]
        fn get_common_metadata_values_should_skip_details_and_conditions_and_constraints_that_have_already_been_seen()
         {
            let content1 = "test message 1";
            let content2 = "test message 2";
            let condition1 = "condition 1";
            let condition2 = "condition 2";
            let constraint1 = "constraint 1";
            let constraint2 = "constraint 2";
            let file1 = File::new("file 1".to_owned())
                .with_condition(condition1.to_owned())
                .with_constraint(constraint1.to_owned())
                .with_detail(vec![MessageContent::new(content1.to_owned())])
                .unwrap();
            let file2 = File::new("file 2".to_owned())
                .with_condition(condition2.to_owned())
                .with_constraint(constraint2.to_owned())
                .with_detail(vec![MessageContent::new(content2.to_owned())])
                .unwrap();
            let file3 = File::new("file 3".to_owned())
                .with_condition(condition2.to_owned())
                .with_constraint(constraint2.to_owned())
                .with_detail(vec![MessageContent::new(content2.to_owned())])
                .unwrap();
            let mut plugin = PluginMetadata::new("test1.esp").unwrap();
            plugin.set_load_after_files(vec![file1.clone(), file1.clone(), file2.clone(), file3]);
            let messages = &[];
            let plugins = &[&plugin];

            let (common_files, _, common_contents, common_conditions) =
                get_common_metadata_values(messages, plugins, true);

            assert!(common_files.contains(&&file1));
            assert!(!common_contents.contains(&file1.detail()));
            assert!(!common_conditions.contains(&condition1));
            assert!(!common_conditions.contains(&constraint1));
            assert!(common_contents.contains(&file2.detail()));
            assert!(common_conditions.contains(&condition2));
            assert!(common_conditions.contains(&constraint2));
        }
    }

    mod replace_prelude {
        use super::*;

        #[test]
        fn should_return_an_empty_string_if_given_empty_strings() {
            let result = replace_prelude(String::new(), "").unwrap();

            assert!(result.masterlist.is_empty());
            assert!(result.meta.is_none());
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

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            assert_eq!(masterlist, result.masterlist);
            assert!(result.meta.is_none());
        }

        #[test]
        fn should_not_change_a_flow_style_masterlist() {
            let prelude = "globals: [{type: note, content: A message.}]";
            let masterlist = "{prelude: {}, plugins: [{name: a.esp}]}";

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            assert_eq!(masterlist, result.masterlist);
            assert!(result.meta.is_none());
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

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(5, result.meta.as_ref().unwrap().end_line);
            assert_eq!(2, result.meta.as_ref().unwrap().lines_added_count);
        }

        #[test]
        fn should_replace_a_prelude_with_no_line_break() {
            let prelude = "globals:
  - type: note
    content: A message.
";
            let masterlist = "prelude:
  a: b
plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(5, result.meta.as_ref().unwrap().end_line);
            assert_eq!(3, result.meta.as_ref().unwrap().lines_added_count);
        }

        #[test]
        fn should_replace_a_prelude_with_one_that_has_no_trailing_line_break() {
            let prelude = "globals:
  - type: note
    content: A message.";
            let masterlist = "prelude:
  a: b

plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.
plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(4, result.meta.as_ref().unwrap().end_line);
            assert_eq!(1, result.meta.as_ref().unwrap().lines_added_count);
        }

        #[test]
        fn should_replace_a_prelude_with_no_line_break_with_one_that_has_no_trailing_line_break() {
            let prelude = "globals:
  - type: note
    content: A message.";
            let masterlist = "prelude:
  a: b
plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.
plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(4, result.meta.as_ref().unwrap().end_line);
            assert_eq!(2, result.meta.as_ref().unwrap().lines_added_count);
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

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "plugins:
  - name: a.esp
prelude:
  globals:
    - type: note
      content: A message.
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(4, result.meta.as_ref().unwrap().start_line);
            assert_eq!(7, result.meta.as_ref().unwrap().end_line);
            assert_eq!(1, result.meta.as_ref().unwrap().lines_added_count);
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

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

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

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(5, result.meta.as_ref().unwrap().start_line);
            assert_eq!(11, result.meta.as_ref().unwrap().end_line);
            assert_eq!(6, result.meta.as_ref().unwrap().lines_added_count);
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

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(5, result.meta.as_ref().unwrap().end_line);
            assert_eq!(2, result.meta.as_ref().unwrap().lines_added_count);
        }

        #[test]
        fn should_succeed_given_a_flow_style_prelude_and_a_block_style_masterlist() {
            let prelude = "globals: [{type: note, content: A message.}]";
            let masterlist = "prelude:
  a: b

plugins:
  - name: a.esp
";

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals: [{type: note, content: A message.}]
plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(2, result.meta.as_ref().unwrap().end_line);
            assert_eq!(-1, result.meta.as_ref().unwrap().lines_added_count);
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

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(5, result.meta.as_ref().unwrap().end_line);
            assert_eq!(0, result.meta.as_ref().unwrap().lines_added_count);
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

            let result = replace_prelude(masterlist.into(), prelude).unwrap();

            let expected_result = "prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
";

            assert_eq!(expected_result, result.masterlist);
            assert_eq!(2, result.meta.as_ref().unwrap().start_line);
            assert_eq!(5, result.meta.as_ref().unwrap().end_line);
            assert_eq!(1, result.meta.as_ref().unwrap().lines_added_count);
        }
    }
}
