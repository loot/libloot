// Allow some lints that are denied at the workspace level.
#![allow(
    clippy::exhaustive_enums,
    clippy::missing_errors_doc,
    clippy::must_use_candidate,
    clippy::needless_pass_by_value
)]

mod database;
mod error;
mod game;
mod metadata;
mod plugin;

use napi::threadsafe_function::{ThreadsafeFunction, ThreadsafeFunctionCallMode};
use napi_derive::napi;

pub use metadata::select_message_content;

#[napi]
pub fn is_compatible(major: u32, minor: u32, patch: u32) -> bool {
    libloot::is_compatible(major, minor, patch)
}

#[napi]
pub fn libloot_revision() -> String {
    libloot::libloot_revision()
}

#[napi]
pub fn libloot_version() -> String {
    libloot::libloot_version()
}

#[napi]
pub const LIBLOOT_VERSION_MAJOR: u32 = libloot::LIBLOOT_VERSION_MAJOR;

#[napi]
pub const LIBLOOT_VERSION_MINOR: u32 = libloot::LIBLOOT_VERSION_MINOR;

#[napi]
pub const LIBLOOT_VERSION_PATCH: u32 = libloot::LIBLOOT_VERSION_PATCH;

#[napi]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
}

impl From<LogLevel> for libloot::LogLevel {
    fn from(value: LogLevel) -> Self {
        match value {
            LogLevel::Trace => libloot::LogLevel::Trace,
            LogLevel::Debug => libloot::LogLevel::Debug,
            LogLevel::Info => libloot::LogLevel::Info,
            LogLevel::Warning => libloot::LogLevel::Warning,
            LogLevel::Error => libloot::LogLevel::Error,
        }
    }
}

impl From<libloot::LogLevel> for LogLevel {
    fn from(value: libloot::LogLevel) -> Self {
        match value {
            libloot::LogLevel::Trace => LogLevel::Trace,
            libloot::LogLevel::Debug => LogLevel::Debug,
            libloot::LogLevel::Info => LogLevel::Info,
            libloot::LogLevel::Warning => LogLevel::Warning,
            libloot::LogLevel::Error => LogLevel::Error,
        }
    }
}

#[napi]
pub fn set_log_level(level: LogLevel) {
    libloot::set_log_level(level.into());
}

#[napi(ts_args_type = "callback: (logLevel: LogLevel, message: string) => void")]
pub fn set_logging_callback(callback: ThreadsafeFunction<(LogLevel, String)>) -> napi::Result<()> {
    let rust_callback = move |level: libloot::LogLevel, message: &str| {
        callback.call(
            Ok((LogLevel::from(level), message.to_owned())),
            ThreadsafeFunctionCallMode::Blocking,
        );
    };

    libloot::set_logging_callback(rust_callback);
    Ok(())
}
