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
    unreachable_pub
)]
#![deny(clippy::pedantic)]
#![allow(clippy::missing_errors_doc)]
// Selectively deny clippy restriction lints.
#![deny(
    clippy::allow_attributes,
    clippy::as_conversions,
    clippy::as_underscore,
    clippy::assertions_on_result_states,
    clippy::big_endian_bytes,
    clippy::cfg_not_test,
    clippy::clone_on_ref_ptr,
    clippy::create_dir,
    clippy::dbg_macro,
    clippy::decimal_literal_representation,
    clippy::default_numeric_fallback,
    clippy::doc_include_without_cfg,
    clippy::empty_drop,
    clippy::error_impl_error,
    clippy::exit,
    clippy::exhaustive_enums,
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
use std::{error::Error, ffi::c_int};

use libloot::error::{LoadOrderError, PluginDataError};

mod esplugin;
mod libloadorder;

// It's important for API stability that these variants' values don't change.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(u8)]
#[non_exhaustive]
pub enum SystemErrorCategory {
    Esplugin = 1,
    Libloadorder = 2,
}

impl std::fmt::Display for SystemErrorCategory {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SystemErrorCategory::Esplugin => write!(f, "esplugin"),
            SystemErrorCategory::Libloadorder => write!(f, "libloadorder"),
        }
    }
}

#[derive(Debug)]
pub struct SystemError {
    code: c_int,
    category: SystemErrorCategory,
    message: String,
}

impl SystemError {
    #[must_use]
    pub fn code(&self) -> c_int {
        self.code
    }

    #[must_use]
    pub fn category(&self) -> SystemErrorCategory {
        self.category
    }

    #[must_use]
    pub fn message(&self) -> &str {
        &self.message
    }
}

impl std::fmt::Display for SystemError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} error, code {}: {}",
            self.category, self.code, self.message
        )
    }
}

impl std::error::Error for SystemError {}

impl From<PluginDataError> for SystemError {
    fn from(value: PluginDataError) -> Self {
        const ESPLUGIN_ERROR_UNKNOWN: (i32, &str) = (
            esplugin::ESP_ERROR_UNKNOWN,
            "Could not retrieve esplugin error message",
        );

        from_error(
            value,
            SystemErrorCategory::Esplugin,
            ESPLUGIN_ERROR_UNKNOWN,
            crate::esplugin::map_error,
        )
    }
}

impl From<LoadOrderError> for SystemError {
    fn from(value: LoadOrderError) -> Self {
        const LIBLOADORDER_ERROR_UNKNOWN: (i32, &str) = (
            libloadorder::LIBLO_ERROR_UNKNOWN,
            "Could not retrieve libloadorder error message",
        );

        from_error(
            value,
            SystemErrorCategory::Libloadorder,
            LIBLOADORDER_ERROR_UNKNOWN,
            crate::libloadorder::map_error,
        )
    }
}

fn from_error<U: Error, V: Error + 'static, F: Fn(&V) -> c_int>(
    error: U,
    category: SystemErrorCategory,
    default: (i32, &'static str),
    to_code: F,
) -> SystemError {
    let (code, message) = error
        .source()
        .and_then(|s| s.downcast_ref::<V>())
        .map_or_else(
            || (default.0, default.1.to_owned()),
            |e| (to_code(e), e.to_string()),
        );

    SystemError {
        code,
        category,
        message,
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct UnsupportedEnumValueError;

impl std::fmt::Display for UnsupportedEnumValueError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Enum value is unsupported")
    }
}

impl std::error::Error for UnsupportedEnumValueError {}

pub fn fmt_error_chain(
    mut error: &dyn std::error::Error,
    f: &mut std::fmt::Formatter<'_>,
) -> std::fmt::Result {
    write!(f, "{error}")?;
    while let Some(source) = error.source() {
        write!(f, ": {source}")?;
        error = source;
    }
    Ok(())
}

#[macro_export]
macro_rules! variant_box_from_error {
    ( $from_type:ident, $to_type:ident::$to_variant:ident ) => {
        impl From<$from_type> for $to_type {
            fn from(value: $from_type) -> Self {
                Self::$to_variant(Box::new(value))
            }
        }
    };
}
