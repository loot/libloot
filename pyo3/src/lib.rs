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

mod database;
mod error;
mod game;
mod metadata;
mod plugin;

use database::{Database, EdgeType, Vertex};
use game::{Game, GameType};
use metadata::{
    File, Filename, Group, Location, Message, MessageContent, MessageType, PluginCleaningData,
    PluginMetadata, Tag, TagSuggestion, select_message_content,
};
use plugin::Plugin;
use pyo3::{create_exception, exceptions::PyException, prelude::*};

#[pyfunction]
fn is_compatible(major: u32, minor: u32, patch: u32) -> bool {
    libloot::is_compatible(major, minor, patch)
}

#[pyfunction]
fn libloot_revision() -> String {
    libloot::libloot_revision()
}

#[pyfunction]
fn libloot_version() -> String {
    libloot::libloot_version()
}

create_exception!(loot, CyclicInteractionError, PyException);
create_exception!(loot, UndefinedGroupError, PyException);
create_exception!(loot, EspluginError, PyException);

/// A Python module implemented in Rust.
#[pymodule(name = "loot")]
fn libloot_pyo3(py: Python<'_>, m: &Bound<'_, PyModule>) -> PyResult<()> {
    pyo3_log::init();

    m.add("LIBLOOT_VERSION_MAJOR", libloot::LIBLOOT_VERSION_MAJOR)?;
    m.add("LIBLOOT_VERSION_MINOR", libloot::LIBLOOT_VERSION_MINOR)?;
    m.add("LIBLOOT_VERSION_PATCH", libloot::LIBLOOT_VERSION_PATCH)?;

    m.add_function(wrap_pyfunction!(is_compatible, m)?)?;
    m.add_function(wrap_pyfunction!(libloot_revision, m)?)?;
    m.add_function(wrap_pyfunction!(libloot_version, m)?)?;

    m.add_function(wrap_pyfunction!(select_message_content, m)?)?;

    m.add_class::<Vertex>()?;
    m.add_class::<EdgeType>()?;
    m.add_class::<GameType>()?;
    m.add_class::<Game>()?;
    m.add_class::<Database>()?;
    m.add_class::<Plugin>()?;
    m.add_class::<Group>()?;
    m.add_class::<MessageContent>()?;
    m.add_class::<MessageType>()?;
    m.add_class::<Message>()?;
    m.add_class::<File>()?;
    m.add_class::<Filename>()?;
    m.add_class::<PluginCleaningData>()?;
    m.add_class::<Tag>()?;
    m.add_class::<TagSuggestion>()?;
    m.add_class::<Location>()?;
    m.add_class::<PluginMetadata>()?;

    m.add(
        "CyclicInteractionError",
        py.get_type::<CyclicInteractionError>(),
    )?;
    m.add("UndefinedGroupError", py.get_type::<UndefinedGroupError>())?;
    m.add("EspluginError", py.get_type::<EspluginError>())?;

    Ok(())
}
