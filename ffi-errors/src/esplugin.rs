use std::ffi::c_int;

use esplugin::Error;

#[unsafe(no_mangle)]
pub static ESP_ERROR_NULL_POINTER: c_int = 1;

#[unsafe(no_mangle)]
pub static ESP_ERROR_NOT_UTF8: c_int = 2;

#[unsafe(no_mangle)]
pub static ESP_ERROR_STRING_CONTAINS_NUL: c_int = 3;

#[unsafe(no_mangle)]
pub static ESP_ERROR_INVALID_GAME_ID: c_int = 4;

#[unsafe(no_mangle)]
pub static ESP_ERROR_PARSE_ERROR: c_int = 5;

#[unsafe(no_mangle)]
pub static ESP_ERROR_PANICKED: c_int = 6;

#[unsafe(no_mangle)]
pub static ESP_ERROR_NO_FILENAME: c_int = 7;

#[unsafe(no_mangle)]
pub static ESP_ERROR_TEXT_DECODE_ERROR: c_int = 8;

#[unsafe(no_mangle)]
pub static ESP_ERROR_TEXT_ENCODE_ERROR: c_int = 9;

#[unsafe(no_mangle)]
pub static ESP_ERROR_IO_ERROR: c_int = 10;

#[unsafe(no_mangle)]
pub static ESP_ERROR_FILE_NOT_FOUND: c_int = 11;

#[unsafe(no_mangle)]
pub static ESP_ERROR_IO_PERMISSION_DENIED: c_int = 12;

#[unsafe(no_mangle)]
pub static ESP_ERROR_UNRESOLVED_RECORD_IDS: c_int = 13;

#[unsafe(no_mangle)]
pub static ESP_ERROR_PLUGIN_METADATA_NOT_FOUND: c_int = 14;

#[unsafe(no_mangle)]
pub static ESP_ERROR_UNKNOWN: c_int = c_int::MAX;

#[expect(
    clippy::wildcard_enum_match_arm,
    reason = "It doesn't matter if other I/O error kinds are added in the future"
)]
fn map_io_error(err: &std::io::Error) -> c_int {
    match err.kind() {
        std::io::ErrorKind::NotFound => ESP_ERROR_FILE_NOT_FOUND,
        std::io::ErrorKind::PermissionDenied => ESP_ERROR_IO_PERMISSION_DENIED,
        _ => ESP_ERROR_IO_ERROR,
    }
}

#[must_use]
pub fn map_error(err: &Error) -> c_int {
    match err {
        Error::IoError(x) => map_io_error(x),
        Error::NoFilename(_) => ESP_ERROR_NO_FILENAME,
        Error::ParsingIncomplete(_) | Error::ParsingError(_, _) => ESP_ERROR_PARSE_ERROR,
        Error::DecodeError(_) => ESP_ERROR_TEXT_DECODE_ERROR,
        Error::UnresolvedRecordIds(_) => ESP_ERROR_UNRESOLVED_RECORD_IDS,
        Error::PluginMetadataNotFound(_) => ESP_ERROR_PLUGIN_METADATA_NOT_FOUND,
    }
}
