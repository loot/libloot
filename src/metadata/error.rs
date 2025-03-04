//! Holds all error types related to LOOT metadata.
use std::path::PathBuf;

use saphyr::Marker;

use crate::metadata::MessageContent;

use super::yaml::{YamlObjectType, to_yaml};

/// Represents an error that occurred when validating a collection of
/// [MessageContent] objects.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct MultilingualMessageContentsError;

impl std::fmt::Display for MultilingualMessageContentsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "multilingual messages must contain a content string that uses the {} language code",
            MessageContent::DEFAULT_LANGUAGE
        )
    }
}

impl std::error::Error for MultilingualMessageContentsError {}

/// Represents an error that occurred while parsing metadata.
#[derive(Debug)]
pub struct ParseMetadataError {
    marker: saphyr::Marker,
    reason: MetadataParsingErrorReason,
}

impl ParseMetadataError {
    pub(super) fn new(marker: Marker, reason: MetadataParsingErrorReason) -> Self {
        Self { marker, reason }
    }

    pub(super) fn invalid_condition(
        marker: Marker,
        condition: String,
        cause: loot_condition_interpreter::Error,
    ) -> Self {
        Self {
            marker,
            reason: MetadataParsingErrorReason::InvalidCondition(Box::new((condition, cause))),
        }
    }

    pub(super) fn missing_key(
        marker: Marker,
        key: &'static str,
        yaml_type: YamlObjectType,
    ) -> Self {
        Self {
            marker,
            reason: MetadataParsingErrorReason::MissingKey(key, yaml_type),
        }
    }

    pub(super) fn duplicate_entry(marker: Marker, id: String, yaml_type: YamlObjectType) -> Self {
        Self {
            marker,
            reason: MetadataParsingErrorReason::DuplicateEntry(id, yaml_type),
        }
    }

    pub(super) fn unexpected_type(
        marker: Marker,
        yaml_type: YamlObjectType,
        expected_type: ExpectedType,
    ) -> Self {
        Self {
            marker,
            reason: MetadataParsingErrorReason::UnexpectedType(expected_type, yaml_type),
        }
    }

    pub(super) fn unexpected_value_type(
        marker: Marker,
        key: &'static str,
        yaml_type: YamlObjectType,
        expected_type: ExpectedType,
    ) -> Self {
        Self {
            marker,
            reason: MetadataParsingErrorReason::UnexpectedValueType(key, expected_type, yaml_type),
        }
    }
}

impl std::fmt::Display for ParseMetadataError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "encountered a YAML parsing error at line {} column {}: {}",
            self.marker.line(),
            self.marker.col(),
            self.reason
        )
    }
}

impl std::error::Error for ParseMetadataError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match &self.reason {
            MetadataParsingErrorReason::InvalidCondition(b) => Some(&b.1),
            MetadataParsingErrorReason::InvalidRegex(b) => Some(b),
            _ => None,
        }
    }
}

impl From<saphyr::ScanError> for ParseMetadataError {
    fn from(value: saphyr::ScanError) -> Self {
        Self {
            marker: *value.marker(),
            reason: MetadataParsingErrorReason::Other(Box::new(value)),
        }
    }
}

#[derive(Debug)]
pub(super) enum MetadataParsingErrorReason {
    InvalidCondition(Box<(String, loot_condition_interpreter::Error)>),
    MissingKey(&'static str, YamlObjectType),
    InvalidRegex(Box<fancy_regex::Error>),
    InvalidMultilingualMessageContents,
    UnexpectedType(ExpectedType, YamlObjectType),
    UnexpectedValueType(&'static str, ExpectedType, YamlObjectType),
    MissingPlaceholder(String, usize),
    NonU32Number(i64),
    DuplicateEntry(String, YamlObjectType),
    Other(Box<saphyr::ScanError>),
}

impl std::fmt::Display for MetadataParsingErrorReason {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidCondition(b) => {
                write!(f, "the condition string \"{}\" is invalid", b.0)
            }
            Self::MissingKey(key, yaml_object_type) => write!(
                f,
                "\"{}\" key in \"{}\" map is missing",
                key, yaml_object_type
            ),
            Self::InvalidRegex(_) => {
                write!(f, "invalid regex in \"name\" key")
            }
            Self::InvalidMultilingualMessageContents => MultilingualMessageContentsError.fmt(f),
            Self::UnexpectedType(expected_type, yaml_object_type) => write!(
                f,
                "\"{}\" object must be {}",
                yaml_object_type, expected_type
            ),
            Self::UnexpectedValueType(key, expected_type, yaml_object_type) => write!(
                f,
                "\"{}\" key in \"{}\" map must be {}",
                key, yaml_object_type, expected_type
            ),
            Self::MissingPlaceholder(sub, placeholder_index) => write!(
                f,
                "failed to substitute \"{}\" into message, no placeholder {{{}}} was found",
                sub, placeholder_index
            ),
            Self::NonU32Number(i) => {
                write!(f, "{} is not valid as a 32-bit unsigned integer", i)
            }
            Self::DuplicateEntry(id, yaml_object_type) => write!(
                f,
                "more than one entry exists for {} \"{}\"",
                yaml_object_type, id
            ),
            Self::Other(m) => m.fmt(f),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub(super) enum ExpectedType {
    String,
    Number,
    Array,
    Map,
    MapOrString,
    ArrayOrString,
}

impl std::fmt::Display for ExpectedType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ExpectedType::String => write!(f, "a string"),
            ExpectedType::Number => write!(f, "a number"),
            ExpectedType::Array => write!(f, "an array"),
            ExpectedType::Map => write!(f, "a map"),
            ExpectedType::MapOrString => write!(f, "a map or string"),
            ExpectedType::ArrayOrString => write!(f, "an array or string"),
        }
    }
}

/// Represents an error encountered while parsing and compiling a regex plugin
/// name.
#[derive(Clone, Debug)]
pub struct RegexError(Box<fancy_regex::Error>);

impl std::fmt::Display for RegexError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "encountered a regex error: {}", self.0)
    }
}

impl std::error::Error for RegexError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.0)
    }
}

impl From<Box<fancy_regex::Error>> for RegexError {
    fn from(value: Box<fancy_regex::Error>) -> Self {
        Self(value)
    }
}

/// Represents an error encountered while loading metadata from a file.
#[derive(Debug)]
pub struct LoadMetadataError {
    path: PathBuf,
    reason: MetadataDocumentParsingError,
}

impl LoadMetadataError {
    pub(super) fn new(path: PathBuf, reason: MetadataDocumentParsingError) -> Self {
        Self {
            path: path.to_path_buf(),
            reason,
        }
    }

    pub(super) fn from_io_error(path: PathBuf, error: std::io::Error) -> Self {
        Self {
            path: path.to_path_buf(),
            reason: MetadataDocumentParsingError::IoError(error),
        }
    }
}

impl std::fmt::Display for LoadMetadataError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "failed to parse the file at \"{}\"", self.path.display())
    }
}

impl std::error::Error for LoadMetadataError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.reason)
    }
}

#[derive(Debug)]
#[non_exhaustive]
pub(super) enum MetadataDocumentParsingError {
    PathNotFound,
    NoDocuments,
    MoreThanOneDocument(usize),
    IoError(std::io::Error),
    MetadataParsingError(ParseMetadataError),
    YamlMergeKeyError(YamlMergeKeyError),
}

impl std::fmt::Display for MetadataDocumentParsingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::PathNotFound => write!(f, "path not found"),
            Self::NoDocuments => write!(f, "no YAML document found"),
            Self::MoreThanOneDocument(n) => write!(f, "expected 1 YAML document, found {}", n),
            Self::IoError(_) => write!(f, "an I/O error occurred"),
            Self::MetadataParsingError(_) => write!(f, "a metadata parsing error occurred"),
            Self::YamlMergeKeyError(_) => {
                write!(f, "an error occurred while resolving YAML merge keys",)
            }
        }
    }
}

impl std::error::Error for MetadataDocumentParsingError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::PathNotFound => None,
            Self::NoDocuments => None,
            Self::MoreThanOneDocument(_) => None,
            Self::IoError(e) => Some(e),
            Self::MetadataParsingError(e) => Some(e),
            Self::YamlMergeKeyError(e) => Some(e),
        }
    }
}

impl From<std::io::Error> for MetadataDocumentParsingError {
    fn from(value: std::io::Error) -> Self {
        MetadataDocumentParsingError::IoError(value)
    }
}

impl From<ParseMetadataError> for MetadataDocumentParsingError {
    fn from(value: ParseMetadataError) -> Self {
        MetadataDocumentParsingError::MetadataParsingError(value)
    }
}

impl From<YamlMergeKeyError> for MetadataDocumentParsingError {
    fn from(value: YamlMergeKeyError) -> Self {
        MetadataDocumentParsingError::YamlMergeKeyError(value)
    }
}

impl From<saphyr::ScanError> for MetadataDocumentParsingError {
    fn from(value: saphyr::ScanError) -> Self {
        Self::MetadataParsingError(value.into())
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Hash)]
pub(super) struct YamlMergeKeyError {
    value: Box<saphyr::MarkedYaml>,
}

impl YamlMergeKeyError {
    pub(super) fn new(value: saphyr::MarkedYaml) -> Self {
        YamlMergeKeyError {
            value: Box::new(value),
        }
    }
}

impl std::fmt::Display for YamlMergeKeyError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut output = String::new();

        let yaml = to_yaml(&self.value);

        if saphyr::YamlEmitter::new(&mut output).dump(&yaml).is_ok() {
            write!(
                f,
                "invalid YAML merge key value at line {} column {}: {}",
                self.value.span.start.line(),
                self.value.span.start.col(),
                output
            )
        } else {
            write!(
                f,
                "invalid YAML merge key value at line {} column {}: {:?}",
                self.value.span.start.line(),
                self.value.span.start.col(),
                self.value
            )
        }
    }
}

impl std::error::Error for YamlMergeKeyError {}

/// Represents an error that occurred while trying to write metadata to a file.
#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct WriteMetadataError {
    path: PathBuf,
    reason: WriteMetadataErrorReason,
}

impl WriteMetadataError {
    pub(crate) fn new(path: PathBuf, reason: WriteMetadataErrorReason) -> Self {
        Self { path, reason }
    }
}

impl std::fmt::Display for WriteMetadataError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self.reason {
            WriteMetadataErrorReason::ParentDirectoryNotFound => write!(
                f,
                "the parent directory of the path \"{}\" was not found",
                self.path.display()
            ),
            WriteMetadataErrorReason::PathAlreadyExists => {
                write!(f, "the path \"{}\" already exists", self.path.display())
            }
        }
    }
}

impl std::error::Error for WriteMetadataError {}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub(crate) enum WriteMetadataErrorReason {
    ParentDirectoryNotFound,
    PathAlreadyExists,
}
