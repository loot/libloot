// Unless otherwise noted, these constants and the mapping logic are copied from
// libloadorder-ffi.

use loadorder::Error;
use std::{ffi::c_int, io::ErrorKind};

/// The specified file could not be found.
const LIBLO_ERROR_FILE_NOT_FOUND: c_int = 6;

/// A file could not be renamed.
const LIBLO_ERROR_FILE_RENAME_FAIL: c_int = 7;

/// There was an error parsing a plugin file.
const LIBLO_ERROR_FILE_PARSE_FAIL: c_int = 10;

/// Invalid arguments were given for the function.
const LIBLO_ERROR_INVALID_ARGS: c_int = 12;

/// An unknown I/O error occurred. This is used when the I/O error kind doesn't
/// fit another error code.
const LIBLO_ERROR_IO_ERROR: c_int = 15;

/// Permission denied while trying to access a filesystem path.
const LIBLO_ERROR_IO_PERMISSION_DENIED: c_int = 16;

/// A plugin filename contains characters that do not have Windows-1252 code
/// points, or a character string contains a null character.
const LIBLO_ERROR_TEXT_ENCODE_FAIL: c_int = 17;

/// Text expected to be encoded in Windows-1252 could not be decoded to UTF-8.
const LIBLO_ERROR_TEXT_DECODE_FAIL: c_int = 18;

/// The library encountered an error that should not have been possible to
/// encounter.
const LIBLO_ERROR_INTERNAL_LOGIC_ERROR: c_int = 19;

/// An unknown operating system error occurred.
const LIBLO_ERROR_SYSTEM_ERROR: c_int = 22;

/// A system path definition (e.g. for local app data on Windows, or $HOME on
/// Linux) could not be found.
const LIBLO_ERROR_NO_PATH: c_int = 23;

// This constant is not copied from libloadorder-ffi, but does not conflict with
// any values defined there.
pub(crate) const LIBLO_ERROR_UNKNOWN: c_int = c_int::MAX;

#[expect(
    clippy::wildcard_enum_match_arm,
    reason = "It doesn't matter if other I/O error kinds are added in the future"
)]
fn map_io_error(err: &std::io::Error) -> c_int {
    match err.kind() {
        ErrorKind::NotFound => LIBLO_ERROR_FILE_NOT_FOUND,
        ErrorKind::AlreadyExists => LIBLO_ERROR_FILE_RENAME_FAIL,
        ErrorKind::PermissionDenied => LIBLO_ERROR_IO_PERMISSION_DENIED,
        _ => LIBLO_ERROR_IO_ERROR,
    }
}

#[must_use]
pub(crate) fn map_error(err: &Error) -> c_int {
    match err {
        Error::InvalidPath(_) => LIBLO_ERROR_FILE_NOT_FOUND,
        Error::IoError(_, x) => map_io_error(x),
        Error::NoFilename(_)
        | Error::PluginParsingError(_, _)
        | Error::IniParsingError { .. }
        | Error::VdfParsingError(_, _) => LIBLO_ERROR_FILE_PARSE_FAIL,
        Error::DecodeError(_) => LIBLO_ERROR_TEXT_DECODE_FAIL,
        Error::EncodeError(_) => LIBLO_ERROR_TEXT_ENCODE_FAIL,
        Error::PluginNotFound(_)
        | Error::TooManyActivePlugins { .. }
        | Error::DuplicatePlugin(_)
        | Error::NonMasterBeforeMaster { .. }
        | Error::InvalidEarlyLoadingPluginPosition { .. }
        | Error::ImplicitlyActivePlugin(_)
        | Error::NoLocalAppData
        | Error::NoDocumentsPath
        | Error::UnrepresentedHoist { .. }
        | Error::InstalledPlugin(_)
        | Error::InvalidBlueprintPluginPosition { .. } => LIBLO_ERROR_INVALID_ARGS,
        Error::NoUserConfigPath | Error::NoUserDataPath | Error::NoProgramFilesPath => {
            LIBLO_ERROR_NO_PATH
        }
        Error::SystemError(_, _) => LIBLO_ERROR_SYSTEM_ERROR,
        _ => LIBLO_ERROR_INTERNAL_LOGIC_ERROR,
    }
}
