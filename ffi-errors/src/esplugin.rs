// Unless otherwise noted, these constants and the mapping logic are copied from
// esplugin-ffi.

use std::ffi::c_int;

use esplugin::Error;

const ESP_ERROR_PARSE_ERROR: c_int = 5;

const ESP_ERROR_NO_FILENAME: c_int = 7;

const ESP_ERROR_TEXT_DECODE_ERROR: c_int = 8;

const ESP_ERROR_IO_ERROR: c_int = 10;

const ESP_ERROR_FILE_NOT_FOUND: c_int = 11;

const ESP_ERROR_IO_PERMISSION_DENIED: c_int = 12;

const ESP_ERROR_UNRESOLVED_RECORD_IDS: c_int = 13;

const ESP_ERROR_PLUGIN_METADATA_NOT_FOUND: c_int = 14;

// This constant is not copied from esplugin-ffi, but does not conflict with any
// values defined there.
pub(crate) const ESP_ERROR_UNKNOWN: c_int = c_int::MAX;

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
pub(crate) fn map_error(err: &Error) -> c_int {
    match err {
        Error::IoError(x) => map_io_error(x),
        Error::NoFilename(_) => ESP_ERROR_NO_FILENAME,
        Error::ParsingIncomplete(_) | Error::ParsingError(_, _) => ESP_ERROR_PARSE_ERROR,
        Error::DecodeError(_) => ESP_ERROR_TEXT_DECODE_ERROR,
        Error::UnresolvedRecordIds(_) => ESP_ERROR_UNRESOLVED_RECORD_IDS,
        Error::PluginMetadataNotFound(_) => ESP_ERROR_PLUGIN_METADATA_NOT_FOUND,
    }
}
