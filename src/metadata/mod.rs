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
pub(crate) mod yaml;

pub use file::{File, Filename};
pub use group::Group;
pub use location::Location;
pub use message::{Message, MessageContent, MessageType, select_message_content};
pub use plugin_cleaning_data::PluginCleaningData;
pub use plugin_metadata::PluginMetadata;
pub use tag::{Tag, TagSuggestion};
