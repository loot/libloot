// Allow some lints that are denied at the workspace level.
#![allow(
    clippy::must_use_candidate,
    clippy::missing_errors_doc,
    clippy::wildcard_enum_match_arm
)]
#![cfg_attr(
    test,
    allow(
        clippy::assertions_on_result_states,
        clippy::indexing_slicing,
        clippy::missing_asserts_for_indexing,
        clippy::panic,
        clippy::unwrap_used,
    )
)]

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

use std::{path::Path, slice::EscapeAscii};

use regress::{Error as RegexImplError, Regex};

pub use database::{Database, EvalMode, MergeMode, WriteMode};
pub use game::{Game, GameType};
pub use logging::{LogLevel, set_log_level, set_logging_callback};
pub use plugin::Plugin;
pub use sorting::vertex::{EdgeType, Vertex};
pub use version::{
    LIBLOOT_VERSION_MAJOR, LIBLOOT_VERSION_MINOR, LIBLOOT_VERSION_PATCH, is_compatible,
    libloot_revision, libloot_version,
};

fn case_insensitive_regex(value: &str) -> Result<Regex, Box<RegexImplError>> {
    Regex::with_flags(value, "iu").map_err(Into::into)
}

fn escape_ascii(path: &Path) -> EscapeAscii<'_> {
    path.as_os_str().as_encoded_bytes().escape_ascii()
}
