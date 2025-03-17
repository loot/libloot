mod database;
mod error;
mod game;
mod metadata;
mod plugin;

use database::{Database, Vertex, new_vertex};
use error::{EmptyOptionalError, UnsupportedEnumValueError, VerboseError};
use game::{Game, new_game, new_game_with_local_path};
use metadata::{
    File, Filename, Group, Location, Message, MessageContent, OptionalMessageContentRef,
    OptionalPluginMetadata, PluginCleaningData, PluginMetadata, Tag, group_default_name,
    message_content_default_language, multilingual_message, new_file, new_filename, new_group,
    new_location, new_message, new_message_content, new_plugin_cleaning_data, new_plugin_metadata,
    new_tag, select_message_content,
};
use plugin::{OptionalPlugin, Plugin};
use std::{
    ffi::{CString, c_char, c_uchar, c_uint, c_void},
    sync::{Mutex, atomic::AtomicPtr},
};
use unicase::UniCase;

use libloot::set_logging_callback;
pub use libloot::{is_compatible, libloot_revision, libloot_version};

#[derive(Debug)]
pub struct OptionalRef<T: ?Sized>(*const T);

impl<T: ?Sized> OptionalRef<T> {
    pub fn is_some(&self) -> bool {
        !self.0.is_null()
    }
}

impl<T> From<Option<&T>> for OptionalRef<T> {
    fn from(value: Option<&T>) -> Self {
        match value {
            Some(p) => OptionalRef(p),
            None => OptionalRef(std::ptr::null()),
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

fn compare_filenames(lhs: &str, rhs: &str) -> i8 {
    match UniCase::new(lhs).cmp(&UniCase::new(rhs)) {
        std::cmp::Ordering::Less => -1,
        std::cmp::Ordering::Equal => 0,
        std::cmp::Ordering::Greater => 1,
    }
}

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

#[allow(clippy::needless_lifetimes)]
#[cxx::bridge(namespace = "loot::rust")]
mod ffi {

    pub enum GameType {
        tes4,
        tes5,
        fo3,
        fonv,
        fo4,
        tes5se,
        fo4vr,
        tes5vr,
        tes3,
        starfield,
        openmw,
    }

    pub enum MessageType {
        say,
        warn,
        error,
    }

    pub enum TagSuggestion {
        Addition,
        Removal,
    }

    pub enum EdgeType {
        hardcoded,
        masterFlag,
        master,
        masterlistRequirement,
        userRequirement,
        masterlistLoadAfter,
        userLoadAfter,
        masterlistGroup,
        userGroup,
        recordOverlap,
        assetOverlap,
        tieBreak,
        blueprintMaster,
    }

    pub enum LogLevel {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Fatal,
    }

    extern "Rust" {
        fn set_log_level(level: LogLevel) -> Result<()>;

        fn is_compatible(major: u32, minor: u32, patch: u32) -> bool;
        fn libloot_version() -> String;
        fn libloot_revision() -> String;

        fn select_message_content(
            contents: &[MessageContent],
            language: &str,
        ) -> Box<OptionalMessageContentRef>;

        fn compare_filenames(lhs: &str, rhs: &str) -> i8;
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

        pub fn new_message(message_type: MessageType, content: String) -> Result<Box<Message>>;

        pub fn multilingual_message(
            message_type: MessageType,
            contents: &[Box<MessageContent>],
        ) -> Result<Box<Message>>;

        pub fn message_type(&self) -> MessageType;

        pub fn content(&self) -> &[MessageContent];

        pub fn condition(&self) -> &str;

        pub fn set_condition(&mut self, condition: String);

        pub fn boxed_clone(&self) -> Box<Message>;
    }

    extern "Rust" {
        type MessageContent;

        pub fn message_content_default_language() -> &'static str;

        pub fn new_message_content(text: String) -> Box<MessageContent>;

        pub fn text(&self) -> &str;

        pub fn language(&self) -> &str;

        pub fn set_language(&mut self, language: String);

        pub fn boxed_clone(&self) -> Box<MessageContent>;
    }

    extern "Rust" {
        type Group;

        pub fn new_group(name: String) -> Box<Group>;

        pub fn group_default_name() -> &'static str;

        pub fn boxed_clone(&self) -> Box<Group>;

        pub fn name(&self) -> &str;

        pub fn description(&self) -> &str;

        pub fn set_description(&mut self, description: String);

        pub fn after_groups(&self) -> &[String];

        pub fn set_after_groups(&mut self, groups: Vec<String>);
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
        pub fn crc(&self) -> i64;

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
        type OptionalMessageContentRef;

        pub fn is_some(&self) -> bool;

        // Again, these lifetimes are wrong.
        pub unsafe fn as_ref<'a>(&'a self) -> Result<&'a MessageContent>;
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

        pub fn new_file(name: String) -> Box<File>;

        pub fn filename(&self) -> &Filename;

        pub fn display_name(&self) -> &str;

        pub fn set_display_name(&mut self, display_name: String);

        pub fn detail(&self) -> &[MessageContent];

        pub fn set_detail(&mut self, detail: &[Box<MessageContent>]) -> Result<()>;

        pub fn condition(&self) -> &str;

        pub fn set_condition(&mut self, condition: String);

        pub fn boxed_clone(&self) -> Box<File>;
    }

    extern "Rust" {
        type Filename;

        pub fn new_filename(name: String) -> Box<Filename>;

        pub fn as_str(&self) -> &str;

        pub fn boxed_clone(&self) -> Box<Filename>;
    }

    extern "Rust" {
        type Tag;

        pub fn new_tag(name: String, suggestion: TagSuggestion) -> Result<Box<Tag>>;

        pub fn name(&self) -> &str;

        pub fn is_addition(&self) -> bool;

        pub fn condition(&self) -> &str;

        pub fn set_condition(&mut self, condition: String);

        pub fn boxed_clone(&self) -> Box<Tag>;
    }

    extern "Rust" {
        type PluginCleaningData;

        pub fn new_plugin_cleaning_data(
            crc: u32,
            cleaning_utility: String,
        ) -> Box<PluginCleaningData>;

        pub fn crc(&self) -> u32;

        pub fn itm_count(&self) -> u32;

        pub fn set_itm_count(&mut self, count: u32);

        pub fn deleted_reference_count(&self) -> u32;

        pub fn set_deleted_reference_count(&mut self, count: u32);

        pub fn deleted_navmesh_count(&self) -> u32;

        pub fn set_deleted_navmesh_count(&mut self, count: u32);

        pub fn cleaning_utility(&self) -> &str;

        pub fn detail(&self) -> &[MessageContent];

        pub fn set_detail(&mut self, detail: &[Box<MessageContent>]) -> Result<()>;

        pub fn boxed_clone(&self) -> Box<PluginCleaningData>;
    }

    extern "Rust" {
        type Location;

        pub fn new_location(url: String) -> Box<Location>;

        pub fn url(&self) -> &str;

        pub fn name(&self) -> &str;

        pub fn set_name(&mut self, name: String);

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
pub static LIBLOOT_LOG_LEVEL_TRACE: c_uchar = libloot::LogLevel::Trace as u8;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_DEBUG: c_uchar = libloot::LogLevel::Debug as u8;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_INFO: c_uchar = libloot::LogLevel::Info as u8;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_WARNING: c_uchar = libloot::LogLevel::Warning as u8;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_ERROR: c_uchar = libloot::LogLevel::Error as u8;

#[unsafe(no_mangle)]
pub static LIBLOOT_LOG_LEVEL_FATAL: c_uchar = libloot::LogLevel::Fatal as u8;

#[unsafe(no_mangle)]
unsafe extern "C" fn libloot_set_logging_callback(
    callback: unsafe extern "C" fn(u8, *const c_char, *mut c_void),
    context: *mut c_void,
) {
    let mutex = Mutex::new(AtomicPtr::new(context));

    set_logging_callback(move |level, message| {
        let (level, c_string) = match CString::new(message) {
            Ok(c) => (level, c),
            Err(_) => {
                let c_string = CString::new(format!(
                    "Attempted to log a message containing a null byte: {}",
                    message.replace('\0', "\\0")
                ))
                .unwrap_or_else(|_| {
                    CString::from(c"Attempted to log a message containing a null byte")
                });
                (libloot::LogLevel::Error, c_string)
            }
        };

        let mut context = match mutex.lock() {
            Ok(c) => c,
            Err(e) => {
                // The stored value is an atomic, since it's atomic it can't have been left in an invalid state.
                mutex.clear_poison();
                e.into_inner()
            }
        };

        unsafe {
            callback(level as u8, c_string.as_ptr(), *context.get_mut());
        }
    });
}
