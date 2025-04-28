use std::path::PathBuf;

use fancy_regex::Error as RegexImplError;

use crate::escape_ascii;

/// Represents an error that occurred while reading a parsed plugin's data.
#[derive(Debug)]
pub struct PluginDataError(esplugin::Error);

impl std::fmt::Display for PluginDataError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "failed to read plugin data")
    }
}

impl std::error::Error for PluginDataError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.0)
    }
}

impl From<esplugin::Error> for PluginDataError {
    fn from(value: esplugin::Error) -> Self {
        PluginDataError(value)
    }
}

#[derive(Debug)]
#[non_exhaustive]
pub(crate) enum LoadPluginError {
    InvalidFilename(InvalidFilenameReason),
    IoError(std::io::Error),
    ParsingError(esplugin::Error),
    RegexError(Box<RegexImplError>),
}

impl std::fmt::Display for LoadPluginError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidFilename(i) => i.fmt(f),
            Self::IoError(_) => write!(f, "an I/O error occurred"),
            Self::ParsingError(_) => write!(f, "failed to parse plugin data"),
            Self::RegexError(_) => write!(f, "failed while using a regex"),
        }
    }
}

impl std::error::Error for LoadPluginError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::InvalidFilename(_) => None,
            Self::IoError(e) => Some(e),
            Self::ParsingError(e) => Some(e),
            Self::RegexError(e) => Some(e),
        }
    }
}

impl From<std::io::Error> for LoadPluginError {
    fn from(value: std::io::Error) -> Self {
        LoadPluginError::IoError(value)
    }
}

impl From<esplugin::Error> for LoadPluginError {
    fn from(value: esplugin::Error) -> Self {
        LoadPluginError::ParsingError(value)
    }
}

impl From<Box<RegexImplError>> for LoadPluginError {
    fn from(value: Box<RegexImplError>) -> Self {
        LoadPluginError::RegexError(value)
    }
}

#[derive(Debug)]
#[non_exhaustive]
pub(crate) enum InvalidFilenameReason {
    Empty,
    NonUnicode,
    NonUnique,
    UnsupportedFileExtension,
}

impl std::fmt::Display for InvalidFilenameReason {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Empty => write!(f, "is empty"),
            Self::NonUnicode => write!(f, "cannot be represented in UTF-8"),
            Self::NonUnique => write!(f, "is not unique"),
            Self::UnsupportedFileExtension => {
                write!(f, "does not have a supported plugin file extension")
            }
        }
    }
}

/// Represents an error that occurred when validating plugins before loading them.
#[derive(Debug)]
pub(crate) struct PluginValidationError {
    path: PathBuf,
    reason: PluginValidationErrorReason,
}

impl PluginValidationError {
    pub(crate) fn new(path: PathBuf, reason: PluginValidationErrorReason) -> Self {
        Self { path, reason }
    }

    pub(crate) fn invalid(path: PathBuf, reason: InvalidFilenameReason) -> Self {
        Self {
            path,
            reason: PluginValidationErrorReason::InvalidFilename(reason),
        }
    }
}

impl std::fmt::Display for PluginValidationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.reason {
            PluginValidationErrorReason::InvalidFilename(i) => write!(
                f,
                "the path \"{}\" has a filename that {}",
                escape_ascii(&self.path),
                i
            ),
            PluginValidationErrorReason::InvalidPluginHeader => write!(
                f,
                "the file at \"{}\" does not have a valid plugin header",
                escape_ascii(&self.path)
            ),
        }
    }
}

impl std::error::Error for PluginValidationError {}

#[derive(Debug)]
pub(crate) enum PluginValidationErrorReason {
    InvalidFilename(InvalidFilenameReason),
    InvalidPluginHeader,
}
