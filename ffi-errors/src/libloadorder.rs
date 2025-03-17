use loadorder::Error;
use std::ffi::c_int;

/// There is a mismatch between the files used to keep track of load order.
///
/// This warning can only occur when using libloadorder with a game that uses the textfile-based
/// load order system. The load order in the active plugins list file (`plugins.txt`) does not
/// match the load order in the full load order file (`loadorder.txt`). Synchronisation between
/// the two is automatic when load order is managed through libloadorder. It is left to the client
/// to decide how best to restore synchronisation.
#[unsafe(no_mangle)]
pub static LIBLO_WARN_LO_MISMATCH: c_int = 2;

/// The specified file could not be found.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_FILE_NOT_FOUND: c_int = 6;

/// A file could not be renamed.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_FILE_RENAME_FAIL: c_int = 7;

/// There was an error parsing a plugin file.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_FILE_PARSE_FAIL: c_int = 10;

/// Invalid arguments were given for the function.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_INVALID_ARGS: c_int = 12;

/// A thread lock was poisoned.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_POISONED_THREAD_LOCK: c_int = 14;

/// An unknown I/O error occurred. This is used when the I/O error kind doesn't fit another error
/// code.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_IO_ERROR: c_int = 15;

/// Permission denied while trying to access a filesystem path.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_IO_PERMISSION_DENIED: c_int = 16;

/// A plugin filename contains characters that do not have Windows-1252 code points, or a character
/// string contains a null character.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_TEXT_ENCODE_FAIL: c_int = 17;

/// Text expected to be encoded in Windows-1252 could not be decoded to UTF-8.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_TEXT_DECODE_FAIL: c_int = 18;

/// The library encountered an error that should not have been possible to encounter.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_INTERNAL_LOGIC_ERROR: c_int = 19;

/// Something panicked.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_PANICKED: c_int = 20;

/// A path cannot be encoded in UTF-8.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_PATH_ENCODE_FAIL: c_int = 21;

/// An unknown operating system error occurred.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_SYSTEM_ERROR: c_int = 22;

/// A system path definition (e.g. for local app data on Windows, or $HOME on Linux) could not be
/// found.
#[unsafe(no_mangle)]
pub static LIBLO_ERROR_NO_PATH: c_int = 23;

/// Matches the value of the highest-numbered return code.
///
/// Provided in case clients wish to incorporate additional return codes in their implementation
/// and desire some method of avoiding value conflicts.
#[unsafe(no_mangle)]
pub static LIBLO_RETURN_MAX: c_int = 23;

fn map_io_error(err: &std::io::Error) -> c_int {
    use std::io::ErrorKind::*;
    match err.kind() {
        NotFound => LIBLO_ERROR_FILE_NOT_FOUND,
        AlreadyExists => LIBLO_ERROR_FILE_RENAME_FAIL,
        PermissionDenied => LIBLO_ERROR_IO_PERMISSION_DENIED,
        _ => LIBLO_ERROR_IO_ERROR,
    }
}

pub fn map_error(err: &Error) -> c_int {
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
