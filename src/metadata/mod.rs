//! Holds all types related to LOOT metadata.
pub mod error;
mod file;
mod group;
mod location;
mod message;
pub(crate) mod metadata_document;
mod plugin_cleaning_data;
pub(crate) mod plugin_metadata;
mod tag;
mod yaml;

pub use file::{File, Filename};
pub use group::Group;
pub use location::Location;
pub use message::{Message, MessageContent, MessageType, select_message_content};
pub use plugin_cleaning_data::PluginCleaningData;
pub use plugin_metadata::PluginMetadata;
pub use tag::{Tag, TagSuggestion};

#[cfg(test)]
use crate::metadata::yaml::YamlAnchors;

#[derive(Debug)]
struct PreludeDiffSpan {
    start_line: usize,
    end_line: usize,
    lines_added_count: isize,
}

#[cfg(test)]
fn emit<T: yaml::EmitYaml>(metadata: &T) -> String {
    let mut emitter = yaml::YamlEmitter::new(YamlAnchors::new());
    metadata.emit_yaml(&mut emitter);

    emitter.into_string()
}

#[cfg(test)]
fn emit_with_anchors<T: yaml::EmitYaml>(metadata: &T, anchors: YamlAnchors) -> String {
    let mut emitter = yaml::YamlEmitter::new(anchors);
    metadata.emit_yaml(&mut emitter);

    emitter.into_string()
}

#[cfg(test)]
fn parse(yaml: &str) -> saphyr::MarkedYaml<'_> {
    use saphyr::LoadableYamlNode;

    saphyr::MarkedYaml::load_from_str(yaml)
        .unwrap()
        .pop()
        .unwrap()
}
