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
    unit_bindings
)]
#![deny(clippy::pedantic)]
// Allow a few clippy pedantic lints.
#![allow(clippy::must_use_candidate)]
#![allow(clippy::missing_errors_doc)]
#![allow(
    clippy::unnecessary_box_returns,
    reason = "CXX requires many returns to be boxed"
)]
// Selectively deny clippy restriction lints.
#![deny(
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

use database::{Database, Vertex, new_vertex};
use error::{EmptyOptionalError, VerboseError};
use ffi::OptionalMessageContentRef;
use game::{Game, new_game, new_game_with_local_path};
use libloot_ffi_errors::UnsupportedEnumValueError;
use metadata::{
    File, Filename, Group, Location, Message, MessageContent, PluginCleaningData, PluginMetadata,
    Tag, group_default_name, message_content_default_language, multilingual_message, new_file,
    new_filename, new_group, new_location, new_message, new_message_content,
    new_plugin_cleaning_data, new_plugin_metadata, new_tag, select_message_content,
};
use plugin::Plugin;
use std::{
    ffi::{CString, c_char, c_uchar, c_uint, c_void},
    sync::{Mutex, atomic::AtomicPtr},
};

use libloot::set_logging_callback;
pub use libloot::{is_compatible, libloot_revision, libloot_version};

impl OptionalMessageContentRef {
    pub fn is_some(&self) -> bool {
        !self.pointer.is_null()
    }

    /// # Safety
    ///
    /// This is safe as long as the pointer in the `OptionalRef` is still valid.
    pub unsafe fn as_ref(&self) -> Result<&MessageContent, EmptyOptionalError> {
        if self.pointer.is_null() {
            Err(EmptyOptionalError)
        } else {
            // SAFETY: This is safe as long as self.0 is still valid.
            unsafe { Ok(&*self.pointer) }
        }
    }
}

impl From<Option<&MessageContent>> for OptionalMessageContentRef {
    fn from(value: Option<&MessageContent>) -> Self {
        match value {
            Some(p) => OptionalMessageContentRef { pointer: p },
            None => OptionalMessageContentRef {
                pointer: std::ptr::null(),
            },
        }
    }
}

#[derive(Debug)]
pub struct Optional<T>(Option<T>);

impl<T> Optional<T> {
    pub fn is_some(&self) -> bool {
        self.0.is_some()
    }

    pub fn as_ref(&self) -> Result<&T, EmptyOptionalError> {
        match &self.0 {
            Some(t) => Ok(t),
            None => Err(EmptyOptionalError),
        }
    }
}

impl<T> From<Option<T>> for Optional<T> {
    fn from(value: Option<T>) -> Self {
        Self(value)
    }
}

pub type OptionalPlugin = Optional<Plugin>;

pub type OptionalPluginMetadata = Optional<PluginMetadata>;

pub type OptionalCrc = Optional<u32>;

fn set_log_level(level: ffi::LogLevel) -> Result<(), VerboseError> {
    libloot::set_log_level(level.try_into()?);
    Ok(())
}

impl TryFrom<ffi::LogLevel> for libloot::LogLevel {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: ffi::LogLevel) -> Result<Self, UnsupportedEnumValueError> {
        match value {
            ffi::LogLevel::Trace => Ok(libloot::LogLevel::Trace),
            ffi::LogLevel::Debug => Ok(libloot::LogLevel::Debug),
            ffi::LogLevel::Info => Ok(libloot::LogLevel::Info),
            ffi::LogLevel::Warning => Ok(libloot::LogLevel::Warning),
            ffi::LogLevel::Error => Ok(libloot::LogLevel::Error),
            ffi::LogLevel::Fatal => Ok(libloot::LogLevel::Fatal),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

#[allow(
    let_underscore_drop,
    missing_debug_implementations,
    clippy::multiple_unsafe_ops_per_block,
    clippy::needless_lifetimes,
    reason = "Required by CXX"
)]
#[cxx::bridge(namespace = "loot::rust")]
mod ffi {

    pub enum GameType {
        Oblivion,
        Skyrim,
        Fallout3,
        FalloutNV,
        Fallout4,
        SkyrimSE,
        Fallout4VR,
        SkyrimVR,
        Morrowind,
        Starfield,
        OpenMW,
        OblivionRemastered,
    }

    pub enum MessageType {
        Say,
        Warn,
        Error,
    }

    pub enum TagSuggestion {
        Addition,
        Removal,
    }

    pub enum EdgeType {
        Hardcoded,
        MasterFlag,
        Master,
        MasterlistRequirement,
        UserRequirement,
        MasterlistLoadAfter,
        UserLoadAfter,
        MasterlistGroup,
        UserGroup,
        RecordOverlap,
        AssetOverlap,
        TieBreak,
        BlueprintMaster,
    }

    pub enum LogLevel {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Fatal,
    }

    #[derive(Debug)]
    struct OptionalMessageContentRef {
        pointer: *const MessageContent,
    }

    extern "Rust" {
        pub fn is_some(self: &OptionalMessageContentRef) -> bool;

        // Again, these lifetimes are wrong.
        pub unsafe fn as_ref<'a>(self: &'a OptionalMessageContentRef)
        -> Result<&'a MessageContent>;
    }

    extern "Rust" {
        fn set_log_level(level: LogLevel) -> Result<()>;

        fn is_compatible(major: u32, minor: u32, patch: u32) -> bool;
        fn libloot_version() -> String;
        fn libloot_revision() -> String;

        fn select_message_content(
            contents: &[MessageContent],
            language: &str,
        ) -> OptionalMessageContentRef;
    }

    extern "Rust" {
        type Game;

        fn new_game(game_type: GameType, game_path: &str) -> Result<Box<Game>>;

        fn new_game_with_local_path(
            game_type: GameType,
            game_path: &str,
            game_local_path: &str,
        ) -> Result<Box<Game>>;

        pub fn game_type(&self) -> Result<GameType>;

        pub fn additional_data_paths(&self) -> Result<Vec<String>>;

        pub fn set_additional_data_paths(&mut self, additional_data_paths: &[&str]) -> Result<()>;

        pub fn database(&self) -> Box<Database>;

        pub fn is_valid_plugin(&self, plugin_path: &str) -> bool;

        pub fn load_plugins(&mut self, plugin_paths: &[&str]) -> Result<()>;

        pub fn load_plugin_headers(&mut self, plugin_paths: &[&str]) -> Result<()>;

        pub fn clear_loaded_plugins(&mut self);

        // The plugin's lifetime is actually less than &Game's, it's only valid so long as the loaded plugin is not overwritten or cleared.
        pub fn plugin(&self, plugin_name: &str) -> Box<OptionalPlugin>;

        // The plugin's lifetime is actually less than &Game's, it's only valid so long as the loaded plugin is not overwritten or cleared.
        pub fn loaded_plugins(&self) -> Vec<Plugin>;

        pub fn sort_plugins(&self, plugin_names: &[&str]) -> Result<Vec<String>>;

        pub fn load_current_load_order_state(&mut self) -> Result<()>;

        pub fn is_load_order_ambiguous(&self) -> Result<bool>;

        pub fn active_plugins_file_path(&self) -> Result<String>;

        pub fn is_plugin_active(&self, plugin_name: &str) -> bool;

        pub fn load_order(&self) -> Vec<String>;

        pub fn set_load_order(&mut self, load_order: &[&str]) -> Result<()>;
    }

    extern "Rust" {
        type Database;

        pub fn load_masterlist(&self, path: &str) -> Result<()>;

        pub fn load_masterlist_with_prelude(
            &self,
            masterlist_path: &str,
            prelude_path: &str,
        ) -> Result<()>;

        pub fn load_userlist(&self, path: &str) -> Result<()>;

        pub fn write_user_metadata(&self, output_path: &str, overwrite: bool) -> Result<()>;

        pub fn write_minimal_list(&self, output_path: &str, overwrite: bool) -> Result<()>;

        pub fn evaluate(&self, condition: &str) -> Result<bool>;

        pub fn known_bash_tags(&self) -> Result<Vec<String>>;

        pub fn general_messages(&self, evaluate_conditions: bool) -> Result<Vec<Message>>;

        pub fn groups(&self, include_user_metadata: bool) -> Result<Vec<Group>>;

        pub fn user_groups(&self) -> Result<Vec<Group>>;

        pub fn set_user_groups(&self, groups: &[Box<Group>]) -> Result<()>;

        pub fn groups_path(
            &self,
            from_group_name: &str,
            to_group_name: &str,
        ) -> Result<Vec<Vertex>>;

        pub fn plugin_metadata(
            &self,
            plugin_name: &str,
            include_user_metadata: bool,
            evaluate_conditions: bool,
        ) -> Result<Box<OptionalPluginMetadata>>;

        pub fn plugin_user_metadata(
            &self,
            plugin_name: &str,
            evaluate_conditions: bool,
        ) -> Result<Box<OptionalPluginMetadata>>;

        pub fn set_plugin_user_metadata(
            &mut self,
            plugin_metadata: Box<PluginMetadata>,
        ) -> Result<()>;

        pub fn discard_plugin_user_metadata(&self, plugin: &str) -> Result<()>;

        pub fn discard_all_user_metadata(&self) -> Result<()>;
    }

    extern "Rust" {
        type Message;

        pub fn new_message(
            message_type: MessageType,
            content: String,
            condition: &str,
        ) -> Result<Box<Message>>;

        pub fn multilingual_message(
            message_type: MessageType,
            contents: &[Box<MessageContent>],
            condition: &str,
        ) -> Result<Box<Message>>;

        pub fn message_type(&self) -> MessageType;

        pub fn content(&self) -> &[MessageContent];

        pub fn condition(&self) -> &str;

        pub fn boxed_clone(&self) -> Box<Message>;
    }

    extern "Rust" {
        type MessageContent;

        pub fn message_content_default_language() -> &'static str;

        pub fn new_message_content(text: String, language: &str) -> Box<MessageContent>;

        pub fn text(&self) -> &str;

        pub fn language(&self) -> &str;

        pub fn boxed_clone(&self) -> Box<MessageContent>;
    }

    extern "Rust" {
        type Group;

        pub fn new_group(name: String, description: &str, after_groups: Vec<String>) -> Box<Group>;

        pub fn group_default_name() -> &'static str;

        pub fn boxed_clone(&self) -> Box<Group>;

        pub fn name(&self) -> &str;

        pub fn description(&self) -> &str;

        pub fn after_groups(&self) -> &[String];
    }

    extern "Rust" {
        type Plugin;

        pub fn name(&self) -> &str;

        // The None case is signalled by NaN.
        pub fn header_version(&self) -> f32;

        // The None case is signalled by an empty string (which is not a valid version).
        pub fn version(&self) -> &str;

        pub fn masters(&self) -> Result<Vec<String>>;

        pub fn bash_tags(&self) -> &[String];

        // The None case is signalled by -1, all other values fit in u32.
        pub fn crc(&self) -> Box<OptionalCrc>;

        pub fn is_master(&self) -> bool;

        pub fn is_light_plugin(&self) -> bool;

        pub fn is_medium_plugin(&self) -> bool;

        pub fn is_update_plugin(&self) -> bool;

        pub fn is_blueprint_plugin(&self) -> bool;

        pub fn is_valid_as_light_plugin(&self) -> Result<bool>;

        pub fn is_valid_as_medium_plugin(&self) -> Result<bool>;

        pub fn is_valid_as_update_plugin(&self) -> Result<bool>;

        pub fn is_empty(&self) -> bool;

        pub fn loads_archive(&self) -> bool;

        pub fn do_records_overlap(&self, plugin: &Plugin) -> Result<bool>;

        pub fn boxed_clone(&self) -> Box<Plugin>;
    }

    extern "Rust" {
        type OptionalPlugin;

        pub fn is_some(&self) -> bool;

        // Again, these lifetimes are wrong.
        pub unsafe fn as_ref<'a>(&'a self) -> Result<&'a Plugin>;
    }

    extern "Rust" {
        type OptionalCrc;

        pub fn is_some(&self) -> bool;

        // Again, these lifetimes are wrong.
        pub unsafe fn as_ref<'a>(&'a self) -> Result<&'a u32>;
    }

    extern "Rust" {
        type Vertex;

        pub fn new_vertex(name: String) -> Box<Vertex>;

        pub fn name(&self) -> &str;

        pub fn out_edge_type(&self) -> Result<u8>;

        pub fn set_out_edge_type(&mut self, #[into] out_edge_type: EdgeType) -> Result<()>;

        pub fn boxed_clone(&self) -> Box<Vertex>;
    }

    extern "Rust" {
        type OptionalPluginMetadata;

        pub fn is_some(&self) -> bool;

        // Again, these lifetimes are wrong.
        pub unsafe fn as_ref<'a>(&'a self) -> Result<&'a PluginMetadata>;
    }

    extern "Rust" {
        type PluginMetadata;

        pub fn new_plugin_metadata(name: &str) -> Result<Box<PluginMetadata>>;

        pub fn name(&self) -> &str;

        pub fn group(&self) -> &str;

        pub fn load_after_files(&self) -> &[File];

        pub fn requirements(&self) -> &[File];

        pub fn incompatibilities(&self) -> &[File];

        pub fn messages(&self) -> &[Message];

        pub fn tags(&self) -> &[Tag];

        pub fn dirty_info(&self) -> &[PluginCleaningData];

        pub fn clean_info(&self) -> &[PluginCleaningData];

        pub fn locations(&self) -> &[Location];

        pub fn set_group(&mut self, group: String);

        pub fn unset_group(&mut self);

        pub fn set_load_after_files(&mut self, files: &[Box<File>]);

        pub fn set_requirements(&mut self, files: &[Box<File>]);

        pub fn set_incompatibilities(&mut self, files: &[Box<File>]);

        pub fn set_messages(&mut self, messages: &[Box<Message>]);

        pub fn set_tags(&mut self, tags: &[Box<Tag>]);

        pub fn set_dirty_info(&mut self, info: &[Box<PluginCleaningData>]);

        pub fn set_clean_info(&mut self, info: &[Box<PluginCleaningData>]);

        pub fn set_locations(&mut self, locations: &[Box<Location>]);

        pub fn merge_metadata(&mut self, plugin: &PluginMetadata);

        pub fn has_name_only(&self) -> bool;

        pub fn is_regex_plugin(&self) -> bool;

        pub fn name_matches(&self, other_name: &str) -> bool;

        pub fn as_yaml(&self) -> String;

        pub fn boxed_clone(&self) -> Box<PluginMetadata>;
    }

    extern "Rust" {
        type File;

        pub fn new_file(
            name: String,
            display_name: &str,
            condition: &str,
            detail: &[Box<MessageContent>],
            constraint: &str,
        ) -> Result<Box<File>>;

        pub fn filename(&self) -> &Filename;

        pub fn display_name(&self) -> &str;

        pub fn detail(&self) -> &[MessageContent];

        pub fn condition(&self) -> &str;

        pub fn constraint(&self) -> &str;

        pub fn boxed_clone(&self) -> Box<File>;
    }

    extern "Rust" {
        type Filename;

        pub fn new_filename(name: String) -> Box<Filename>;

        pub fn as_str(&self) -> &str;

        pub fn boxed_clone(&self) -> Box<Filename>;

        pub fn eq(&self, other: &Filename) -> bool;

        pub fn ne(&self, other: &Filename) -> bool;

        pub fn lt(&self, other: &Filename) -> bool;

        pub fn le(&self, other: &Filename) -> bool;

        pub fn gt(&self, other: &Filename) -> bool;

        pub fn ge(&self, other: &Filename) -> bool;
    }

    extern "Rust" {
        type Tag;

        pub fn new_tag(
            name: String,
            suggestion: TagSuggestion,
            condition: &str,
        ) -> Result<Box<Tag>>;

        pub fn name(&self) -> &str;

        pub fn is_addition(&self) -> bool;

        pub fn condition(&self) -> &str;

        pub fn boxed_clone(&self) -> Box<Tag>;
    }

    extern "Rust" {
        type PluginCleaningData;

        pub fn new_plugin_cleaning_data(
            crc: u32,
            cleaning_utility: String,
            detail: &[Box<MessageContent>],
            itm_count: u32,
            deleted_reference_count: u32,
            deleted_navmesh_count: u32,
        ) -> Result<Box<PluginCleaningData>>;

        pub fn crc(&self) -> u32;

        pub fn itm_count(&self) -> u32;

        pub fn deleted_reference_count(&self) -> u32;

        pub fn deleted_navmesh_count(&self) -> u32;

        pub fn cleaning_utility(&self) -> &str;

        pub fn detail(&self) -> &[MessageContent];

        pub fn boxed_clone(&self) -> Box<PluginCleaningData>;
    }

    extern "Rust" {
        type Location;

        pub fn new_location(url: String, name: &str) -> Box<Location>;

        pub fn url(&self) -> &str;

        pub fn name(&self) -> &str;

        pub fn boxed_clone(&self) -> Box<Location>;
    }
}

#[unsafe(no_mangle)]
pub static LIBLOOT_VERSION_MAJOR: c_uint = libloot::LIBLOOT_VERSION_MAJOR;

#[unsafe(no_mangle)]
pub static LIBLOOT_VERSION_MINOR: c_uint = libloot::LIBLOOT_VERSION_MINOR;

#[unsafe(no_mangle)]
pub static LIBLOOT_VERSION_PATCH: c_uint = libloot::LIBLOOT_VERSION_PATCH;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_TRACE: c_uchar = 0;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_DEBUG: c_uchar = 1;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_INFO: c_uchar = 2;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_WARNING: c_uchar = 3;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_ERROR: c_uchar = 4;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_FATAL: c_uchar = 5;

fn to_u8(value: libloot::LogLevel) -> u8 {
    match value {
        libloot::LogLevel::Trace => LIBLOOT_LOG_LEVEL_TRACE,
        libloot::LogLevel::Debug => LIBLOOT_LOG_LEVEL_DEBUG,
        libloot::LogLevel::Info => LIBLOOT_LOG_LEVEL_INFO,
        libloot::LogLevel::Warning => LIBLOOT_LOG_LEVEL_WARNING,
        libloot::LogLevel::Error => LIBLOOT_LOG_LEVEL_ERROR,
        libloot::LogLevel::Fatal => LIBLOOT_LOG_LEVEL_FATAL,
    }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn libloot_set_logging_callback(
    callback: unsafe extern "C" fn(u8, *const c_char, *mut c_void),
    context: *mut c_void,
) {
    let mutex = Mutex::new(AtomicPtr::new(context));

    set_logging_callback(move |level, message| {
        let (level, c_string) = CString::new(message).map_or_else(
            |_| {
                let c_string = CString::new(format!(
                    "Attempted to log a message containing a null byte: {}",
                    message.replace('\0', "\\0")
                ))
                .unwrap_or_else(|_| {
                    CString::from(c"Attempted to log a message containing a null byte")
                });
                (libloot::LogLevel::Error, c_string)
            },
            |c| (level, c),
        );

        let mut context = match mutex.lock() {
            Ok(c) => c,
            Err(e) => {
                // The stored value is an atomic, since it's atomic it can't have been left in an invalid state.
                mutex.clear_poison();
                e.into_inner()
            }
        };

        // SAFETY: This is safe so long as callback remains a valid function pointer.
        unsafe {
            callback(to_u8(level), c_string.as_ptr(), *context.get_mut());
        }
    });
}
