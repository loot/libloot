// Deny some rustc lints that are allow-by-default.
#![deny(
    ambiguous_negative_literals,
    impl_trait_overcaptures,
    let_underscore_drop,
    missing_copy_implementations,
    missing_debug_implementations,
    non_ascii_idents,
    redundant_imports,
    redundant_lifetimes,
    trivial_casts,
    trivial_numeric_casts,
    unit_bindings,
    // unreachable_pub,
    unsafe_code
)]
#![deny(clippy::pedantic)]
// Allow a few clippy pedantic lints.
#![allow(clippy::doc_markdown)]
#![allow(clippy::must_use_candidate)]
#![allow(clippy::missing_errors_doc)]

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

use fancy_regex::{Error as RegexImplError, Regex, RegexBuilder};

pub use database::{Database, WriteMode};
pub use game::{Game, GameType};
pub use logging::{LogLevel, set_log_level, set_logging_callback};
pub use plugin::Plugin;
pub use sorting::vertex::{EdgeType, Vertex};
pub use version::{
    LIBLOOT_VERSION_MAJOR, LIBLOOT_VERSION_MINOR, LIBLOOT_VERSION_PATCH, is_compatible,
    libloot_revision, libloot_version,
};

fn case_insensitive_regex(value: &str) -> Result<Regex, Box<RegexImplError>> {
    RegexBuilder::new(value)
        .case_insensitive(true)
        .build()
        .map_err(Into::into)
}
