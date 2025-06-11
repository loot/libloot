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
    unreachable_pub,
    unsafe_code
)]
#![deny(clippy::pedantic)]
#![allow(clippy::missing_errors_doc)]
#![allow(clippy::must_use_candidate)]
#![allow(clippy::needless_pass_by_value)]
// Selectively deny clippy restriction lints.
#![deny(
    clippy::allow_attributes,
    clippy::as_conversions,
    clippy::as_underscore,
    clippy::assertions_on_result_states,
    clippy::big_endian_bytes,
    clippy::clone_on_ref_ptr,
    clippy::create_dir,
    clippy::dbg_macro,
    clippy::decimal_literal_representation,
    clippy::default_numeric_fallback,
    clippy::doc_include_without_cfg,
    clippy::empty_drop,
    clippy::error_impl_error,
    clippy::exit,
    // clippy::exhaustive_enums,
    clippy::expect_used,
    clippy::filetype_is_file,
    clippy::float_cmp_const,
    clippy::fn_to_numeric_cast_any,
    clippy::get_unwrap,
    clippy::host_endian_bytes,
    clippy::if_then_some_else_none,
    clippy::indexing_slicing,
    clippy::infinite_loop,
    clippy::integer_division,
    clippy::integer_division_remainder_used,
    clippy::iter_over_hash_type,
    clippy::let_underscore_must_use,
    clippy::lossy_float_literal,
    clippy::map_err_ignore,
    clippy::map_with_unused_argument_over_ranges,
    clippy::mem_forget,
    clippy::missing_assert_message,
    clippy::missing_asserts_for_indexing,
    clippy::mixed_read_write_in_expression,
    clippy::multiple_inherent_impl,
    clippy::multiple_unsafe_ops_per_block,
    clippy::mutex_atomic,
    clippy::mutex_integer,
    clippy::needless_raw_strings,
    clippy::non_ascii_literal,
    clippy::non_zero_suggestions,
    clippy::panic,
    clippy::panic_in_result_fn,
    clippy::partial_pub_fields,
    clippy::pathbuf_init_then_push,
    clippy::precedence_bits,
    clippy::print_stderr,
    clippy::print_stdout,
    clippy::rc_buffer,
    clippy::rc_mutex,
    clippy::redundant_type_annotations,
    clippy::ref_patterns,
    clippy::rest_pat_in_fully_bound_structs,
    clippy::str_to_string,
    clippy::string_lit_chars_any,
    clippy::string_slice,
    clippy::string_to_string,
    clippy::suspicious_xor_used_as_pow,
    clippy::tests_outside_test_module,
    clippy::todo,
    clippy::try_err,
    clippy::undocumented_unsafe_blocks,
    clippy::unimplemented,
    clippy::unnecessary_safety_comment,
    clippy::unneeded_field_pattern,
    clippy::unreachable,
    clippy::unused_result_ok,
    clippy::unwrap_in_result,
    clippy::unwrap_used,
    clippy::use_debug,
    clippy::verbose_file_reads,
    clippy::wildcard_enum_match_arm
)]

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
#[derive(Debug)]
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
