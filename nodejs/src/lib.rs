#![deny(clippy::all)]

mod database;
mod error;
mod game;
mod metadata;
mod plugin;

use napi::{
    threadsafe_function::{ErrorStrategy, ThreadsafeFunction, ThreadsafeFunctionCallMode},
    Either,
};
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
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
}

impl From<LogLevel> for libloot::LogLevel {
    fn from(value: LogLevel) -> Self {
        match value {
            LogLevel::Trace => libloot::LogLevel::Trace,
            LogLevel::Debug => libloot::LogLevel::Debug,
            LogLevel::Info => libloot::LogLevel::Info,
            LogLevel::Warning => libloot::LogLevel::Warning,
            LogLevel::Error => libloot::LogLevel::Error,
            LogLevel::Fatal => libloot::LogLevel::Fatal,
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
            libloot::LogLevel::Fatal => LogLevel::Fatal,
        }
    }
}

#[napi]
pub fn set_log_level(level: LogLevel) {
    libloot::set_log_level(level.into());
}

#[napi(ts_args_type = "callback: (logLevel: LogLevel, message: string) => void")]
pub fn set_logging_callback(callback: napi::JsFunction) -> napi::Result<()> {
    let thread_safe_callback: ThreadsafeFunction<
        (libloot::LogLevel, String),
        ErrorStrategy::Fatal,
    > = callback.create_threadsafe_function(0, |ctx| {
        let (level, message): (libloot::LogLevel, String) = ctx.value;

        Ok(vec![
            Either::A::<LogLevel, _>(level.into()),
            Either::B(message),
        ])
    })?;

    let rust_callback = move |level: libloot::LogLevel, message: &str| {
        thread_safe_callback.call(
            (level, message.to_owned()),
            ThreadsafeFunctionCallMode::Blocking,
        );
    };

    libloot::set_logging_callback(rust_callback);
    Ok(())
}
