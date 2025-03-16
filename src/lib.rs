mod archive;
mod database;
pub mod error;
mod game;
mod logging;
pub mod metadata;
mod plugin;
mod sorting;
#[cfg(test)]
mod tests;
mod version;

pub use database::Database;
pub use game::{Game, GameType};
pub use logging::{LogLevel, set_log_level, set_logging_callback};
pub use plugin::Plugin;
pub use sorting::vertex::{EdgeType, Vertex};
pub use version::{
    LIBLOOT_VERSION_MAJOR, LIBLOOT_VERSION_MINOR, LIBLOOT_VERSION_PATCH, is_compatible,
    libloot_revision, libloot_version,
};
