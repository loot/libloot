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

#[unsafe(no_mangle)]
pub static LCI_ERROR_UNKNOWN: c_int = c_int::MAX;

#[must_use]
pub fn map_error(err: &Error) -> c_int {
    match err {
        Error::ParsingIncomplete(_) | Error::UnconsumedInput(_) | Error::ParsingError(_, _) => {
            LCI_ERROR_PARSING_ERROR
        }
        Error::PeParsingError(_, _) => LCI_ERROR_PE_PARSING_ERROR,
        Error::IoError(_, _) => LCI_ERROR_IO_ERROR,
        _ => LCI_ERROR_INTERNAL_LOGIC_ERROR,
    }
}
