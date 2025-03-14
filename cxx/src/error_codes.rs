pub(crate) mod lci {
    use loot_condition_interpreter::Error;
    use std::ffi::c_int;

    /// Invalid arguments were given for the function.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_INVALID_ARGS: c_int = -1;

    /// Something went wrong while parsing the condition expression.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_PARSING_ERROR: c_int = -2;

    /// Something went wrong while getting the version of an executable.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_PE_PARSING_ERROR: c_int = -3;

    /// Some sort of I/O error occurred.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_IO_ERROR: c_int = -4;

    /// Something panicked.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_PANICKED: c_int = -5;

    /// A thread lock was poisoned.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_POISONED_THREAD_LOCK: c_int = -6;

    /// Failed to encode string as a C string, e.g. because there was a nul present.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_TEXT_ENCODE_FAIL: c_int = -7;

    /// The library encountered an error that should not have been possible to encounter.
    #[unsafe(no_mangle)]
    pub static LCI_ERROR_INTERNAL_LOGIC_ERROR: c_int = -8;

    pub(crate) fn map_error(err: &Error) -> c_int {
        match err {
            Error::ParsingIncomplete(_) => LCI_ERROR_PARSING_ERROR,
            Error::UnconsumedInput(_) => LCI_ERROR_PARSING_ERROR,
            Error::ParsingError(_, _) => LCI_ERROR_PARSING_ERROR,
            Error::PeParsingError(_, _) => LCI_ERROR_PE_PARSING_ERROR,
            Error::IoError(_, _) => LCI_ERROR_IO_ERROR,
            _ => LCI_ERROR_INTERNAL_LOGIC_ERROR,
        }
    }
}

pub(crate) mod libloadorder {
    use loadorder::Error;
    use std::ffi::c_uint;

    /// There is a mismatch between the files used to keep track of load order.
    ///
    /// This warning can only occur when using libloadorder with a game that uses the textfile-based
    /// load order system. The load order in the active plugins list file (`plugins.txt`) does not
    /// match the load order in the full load order file (`loadorder.txt`). Synchronisation between
    /// the two is automatic when load order is managed through libloadorder. It is left to the client
    /// to decide how best to restore synchronisation.
    #[unsafe(no_mangle)]
    pub static LIBLO_WARN_LO_MISMATCH: c_uint = 2;

    /// The specified file could not be found.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_FILE_NOT_FOUND: c_uint = 6;

    /// A file could not be renamed.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_FILE_RENAME_FAIL: c_uint = 7;

    /// There was an error parsing a plugin file.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_FILE_PARSE_FAIL: c_uint = 10;

    /// Invalid arguments were given for the function.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_INVALID_ARGS: c_uint = 12;

    /// A thread lock was poisoned.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_POISONED_THREAD_LOCK: c_uint = 14;

    /// An unknown I/O error occurred. This is used when the I/O error kind doesn't fit another error
    /// code.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_IO_ERROR: c_uint = 15;

    /// Permission denied while trying to access a filesystem path.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_IO_PERMISSION_DENIED: c_uint = 16;

    /// A plugin filename contains characters that do not have Windows-1252 code points, or a character
    /// string contains a null character.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_TEXT_ENCODE_FAIL: c_uint = 17;

    /// Text expected to be encoded in Windows-1252 could not be decoded to UTF-8.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_TEXT_DECODE_FAIL: c_uint = 18;

    /// The library encountered an error that should not have been possible to encounter.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_INTERNAL_LOGIC_ERROR: c_uint = 19;

    /// Something panicked.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_PANICKED: c_uint = 20;

    /// A path cannot be encoded in UTF-8.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_PATH_ENCODE_FAIL: c_uint = 21;

    /// An unknown operating system error occurred.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_SYSTEM_ERROR: c_uint = 22;

    /// A system path definition (e.g. for local app data on Windows, or $HOME on Linux) could not be
    /// found.
    #[unsafe(no_mangle)]
    pub static LIBLO_ERROR_NO_PATH: c_uint = 23;

    /// Matches the value of the highest-numbered return code.
    ///
    /// Provided in case clients wish to incorporate additional return codes in their implementation
    /// and desire some method of avoiding value conflicts.
    #[unsafe(no_mangle)]
    pub static LIBLO_RETURN_MAX: c_uint = 23;

    fn map_io_error(err: &std::io::Error) -> c_uint {
        use std::io::ErrorKind::*;
        match err.kind() {
            NotFound => LIBLO_ERROR_FILE_NOT_FOUND,
            AlreadyExists => LIBLO_ERROR_FILE_RENAME_FAIL,
            PermissionDenied => LIBLO_ERROR_IO_PERMISSION_DENIED,
            _ => LIBLO_ERROR_IO_ERROR,
        }
    }

    pub(crate) fn map_error(err: &Error) -> c_uint {
        use Error::*;
        match *err {
            InvalidPath(_) => LIBLO_ERROR_FILE_NOT_FOUND,
            IoError(_, ref x) => map_io_error(x),
            NoFilename(_) => LIBLO_ERROR_FILE_PARSE_FAIL,
            DecodeError(_) => LIBLO_ERROR_TEXT_DECODE_FAIL,
            EncodeError(_) => LIBLO_ERROR_TEXT_ENCODE_FAIL,
            PluginParsingError(_, _) => LIBLO_ERROR_FILE_PARSE_FAIL,
            PluginNotFound(_) => LIBLO_ERROR_INVALID_ARGS,
            TooManyActivePlugins { .. } => LIBLO_ERROR_INVALID_ARGS,
            DuplicatePlugin(_) => LIBLO_ERROR_INVALID_ARGS,
            NonMasterBeforeMaster { .. } => LIBLO_ERROR_INVALID_ARGS,
            InvalidEarlyLoadingPluginPosition { .. } => LIBLO_ERROR_INVALID_ARGS,
            ImplicitlyActivePlugin(_) => LIBLO_ERROR_INVALID_ARGS,
            NoLocalAppData => LIBLO_ERROR_INVALID_ARGS,
            NoDocumentsPath => LIBLO_ERROR_INVALID_ARGS,
            NoUserConfigPath => LIBLO_ERROR_NO_PATH,
            NoUserDataPath => LIBLO_ERROR_NO_PATH,
            NoProgramFilesPath => LIBLO_ERROR_NO_PATH,
            UnrepresentedHoist { .. } => LIBLO_ERROR_INVALID_ARGS,
            InstalledPlugin(_) => LIBLO_ERROR_INVALID_ARGS,
            IniParsingError { .. } => LIBLO_ERROR_FILE_PARSE_FAIL,
            VdfParsingError(_, _) => LIBLO_ERROR_FILE_PARSE_FAIL,
            SystemError(_, _) => LIBLO_ERROR_SYSTEM_ERROR,
            InvalidBlueprintPluginPosition { .. } => LIBLO_ERROR_INVALID_ARGS,
            _ => LIBLO_ERROR_INTERNAL_LOGIC_ERROR,
        }
    }
}

pub(crate) mod esplugin {
    use std::ffi::c_uint;

    use esplugin::Error;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_NULL_POINTER: u32 = 1;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_NOT_UTF8: u32 = 2;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_STRING_CONTAINS_NUL: u32 = 3;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_INVALID_GAME_ID: u32 = 4;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_PARSE_ERROR: u32 = 5;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_PANICKED: u32 = 6;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_NO_FILENAME: u32 = 7;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_TEXT_DECODE_ERROR: u32 = 8;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_TEXT_ENCODE_ERROR: u32 = 9;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_IO_ERROR: u32 = 10;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_FILE_NOT_FOUND: u32 = 11;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_IO_PERMISSION_DENIED: u32 = 12;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_UNRESOLVED_RECORD_IDS: u32 = 13;

    #[unsafe(no_mangle)]
    pub static ESP_ERROR_PLUGIN_METADATA_NOT_FOUND: u32 = 14;

    fn map_io_error(err: &std::io::Error) -> c_uint {
        match err.kind() {
            std::io::ErrorKind::NotFound => ESP_ERROR_FILE_NOT_FOUND,
            std::io::ErrorKind::PermissionDenied => ESP_ERROR_IO_PERMISSION_DENIED,
            _ => ESP_ERROR_IO_ERROR,
        }
    }

    pub(crate) fn map_error(err: &Error) -> c_uint {
        match *err {
            Error::IoError(ref x) => map_io_error(x),
            Error::NoFilename(_) => ESP_ERROR_NO_FILENAME,
            Error::ParsingIncomplete(_) => ESP_ERROR_PARSE_ERROR,
            Error::ParsingError(_, _) => ESP_ERROR_PARSE_ERROR,
            Error::DecodeError(_) => ESP_ERROR_TEXT_DECODE_ERROR,
            Error::UnresolvedRecordIds(_) => ESP_ERROR_UNRESOLVED_RECORD_IDS,
            Error::PluginMetadataNotFound(_) => ESP_ERROR_PLUGIN_METADATA_NOT_FOUND,
        }
    }
}
