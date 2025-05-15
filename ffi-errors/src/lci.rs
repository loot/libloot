// Unless otherwise noted, these constants and the mapping logic are copied from
// loot-condition-interpreter-ffi.

use loot_condition_interpreter::Error;
use std::ffi::c_int;

/// Something went wrong while parsing the condition expression.
const LCI_ERROR_PARSING_ERROR: c_int = -2;

/// Something went wrong while getting the version of an executable.
const LCI_ERROR_PE_PARSING_ERROR: c_int = -3;

/// Some sort of I/O error occurred.
const LCI_ERROR_IO_ERROR: c_int = -4;

/// The library encountered an error that should not have been possible to
/// encounter.
const LCI_ERROR_INTERNAL_LOGIC_ERROR: c_int = -8;

// This constant is not copied from loot-condition-interpreter-ffi, but does not
// conflict with any values defined there.
pub(crate) const LCI_ERROR_UNKNOWN: c_int = c_int::MAX;

#[must_use]
pub(crate) fn map_error(err: &Error) -> c_int {
    match err {
        Error::ParsingIncomplete(_) | Error::UnconsumedInput(_) | Error::ParsingError(_, _) => {
            LCI_ERROR_PARSING_ERROR
        }
        Error::PeParsingError(_, _) => LCI_ERROR_PE_PARSING_ERROR,
        Error::IoError(_, _) => LCI_ERROR_IO_ERROR,
        _ => LCI_ERROR_INTERNAL_LOGIC_ERROR,
    }
}
