mod archive;
mod database;
pub mod error;
mod game;
mod logging;
pub mod metadata;
mod plugin;
mod sorting;
mod version;

pub use database::Database;
use fancy_regex::{Regex, RegexBuilder};
pub use game::{Game, GameType};
pub use logging::{LogLevel, set_logging_callback};
pub use plugin::Plugin;
pub use sorting::vertex::{EdgeType, Vertex};
pub use version::{
    LIBLOOT_VERSION_MAJOR, LIBLOOT_VERSION_MINOR, LIBLOOT_VERSION_PATCH, is_compatible,
    libloot_revision, libloot_version,
};

fn regex(name: &str) -> Result<Regex, Box<fancy_regex::Error>> {
    Ok(RegexBuilder::new(name).case_insensitive(true).build()?)
}
