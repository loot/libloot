use std::fmt::Display;

use petgraph::graph::NodeIndex;
use saphyr::Marker;

use crate::{
    metadata::{
        MessageContent,
        yaml::{YamlObjectType, to_yaml},
    },
    sorting::vertex::Vertex,
};

#[derive(Debug)]
pub struct CyclicInteractionError {
    pub cycle: Vec<Vertex>,
}

impl CyclicInteractionError {
    pub fn new(cycle: Vec<Vertex>) -> Self {
        Self { cycle }
    }
}

impl Display for CyclicInteractionError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let cycle: String = self
            .cycle
            .iter()
            .map(|v| {
                if let Some(edge_type) = v.out_edge_type() {
                    format!("{} --[{}]-> ", v.name(), edge_type)
                } else {
                    v.name().to_string()
                }
            })
            .chain(self.cycle.first().iter().map(|v| v.name().to_string()))
            .collect();
        write!(f, "Cyclic interaction detected: {}", cycle)
    }
}

impl std::error::Error for CyclicInteractionError {}

#[derive(Debug)]
pub struct FileAccessError {
    pub message: String,
}

impl FileAccessError {
    pub(crate) fn new(message: String) -> Self {
        FileAccessError { message }
    }
}

impl std::fmt::Display for FileAccessError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for FileAccessError {}

#[derive(Debug)]
pub struct UndefinedGroupError {
    pub group_name: String,
}

impl UndefinedGroupError {
    pub fn new(group_name: String) -> Self {
        Self { group_name }
    }
}

impl Display for UndefinedGroupError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "The group \"{}\" does not exist", self.group_name)
    }
}

impl std::error::Error for UndefinedGroupError {}

#[derive(Debug)]
pub struct InvalidArgumentError {
    pub message: String,
}

impl std::fmt::Display for InvalidArgumentError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for InvalidArgumentError {}

#[derive(Debug)]
pub(crate) struct YamlMergeKeyError {
    value: saphyr::MarkedYaml,
}

impl YamlMergeKeyError {
    pub(crate) fn new(value: saphyr::MarkedYaml) -> Self {
        YamlMergeKeyError { value }
    }
}

impl std::fmt::Display for YamlMergeKeyError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut output = String::new();

        let yaml = to_yaml(&self.value);

        if saphyr::YamlEmitter::new(&mut output).dump(&yaml).is_ok() {
            write!(
                f,
                "Invalid YAML merge key value at line {} column {}: {}",
                self.value.span.start.line(),
                self.value.span.start.col(),
                output
            )
        } else {
            write!(
                f,
                "Invalid YAML merge key value at line {} column {}: {:?}",
                self.value.span.start.line(),
                self.value.span.start.col(),
                self.value
            )
        }
    }
}

impl std::error::Error for YamlMergeKeyError {}

#[derive(Debug)]
pub struct YamlParseError {
    marker: saphyr::Marker,
    message: String,
}

impl YamlParseError {
    pub fn new(marker: Marker, message: String) -> Self {
        YamlParseError { marker, message }
    }

    pub fn missing_key(marker: Marker, key: &str, yaml_type: YamlObjectType) -> Self {
        YamlParseError::new(
            marker,
            format!("'{}' key missing from '{}' map object", key, yaml_type),
        )
    }
}

impl std::fmt::Display for YamlParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Encountered a YAML parsing error at line {} column {}: {}",
            self.marker.line(),
            self.marker.col(),
            self.message
        )
    }
}

impl std::error::Error for YamlParseError {}

#[derive(Debug)]
pub struct InvalidMultilingualMessageContents {}

impl std::fmt::Display for InvalidMultilingualMessageContents {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Multilingual messages must contain a content string that uses the {} language code",
            MessageContent::DEFAULT_LANGUAGE
        )
    }
}

impl std::error::Error for InvalidMultilingualMessageContents {}

#[derive(Debug)]
pub struct PoisonedMutexError;

impl std::fmt::Display for PoisonedMutexError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "A mutex is poisoned",)
    }
}

impl std::error::Error for PoisonedMutexError {}

#[derive(Debug)]
pub struct PathfindingError {
    message: String,
}

impl PathfindingError {
    pub fn new(message: String) -> Self {
        Self { message }
    }
}

impl std::fmt::Display for PathfindingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for PathfindingError {}

#[derive(Debug)]
pub struct SortingLogicError {
    message: String,
}

impl SortingLogicError {
    pub fn new(message: String) -> Self {
        Self { message }
    }
}

impl std::fmt::Display for SortingLogicError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for SortingLogicError {}

#[derive(Debug)]
pub struct GeneralError(Box<dyn std::error::Error + Send + Sync + 'static>);

impl std::fmt::Display for GeneralError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

impl std::error::Error for GeneralError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(self.0.as_ref())
    }
}

impl From<std::io::Error> for GeneralError {
    fn from(value: std::io::Error) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<saphyr::ScanError> for GeneralError {
    fn from(value: saphyr::ScanError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<YamlMergeKeyError> for GeneralError {
    fn from(value: YamlMergeKeyError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<FileAccessError> for GeneralError {
    fn from(value: FileAccessError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<YamlParseError> for GeneralError {
    fn from(value: YamlParseError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<loot_condition_interpreter::Error> for GeneralError {
    fn from(value: loot_condition_interpreter::Error) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<std::num::TryFromIntError> for GeneralError {
    fn from(value: std::num::TryFromIntError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<InvalidMultilingualMessageContents> for GeneralError {
    fn from(value: InvalidMultilingualMessageContents) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<InvalidArgumentError> for GeneralError {
    fn from(value: InvalidArgumentError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<loadorder::Error> for GeneralError {
    fn from(value: loadorder::Error) -> Self {
        GeneralError(Box::new(value))
    }
}

impl<T> From<std::sync::PoisonError<T>> for GeneralError {
    fn from(_: std::sync::PoisonError<T>) -> Self {
        GeneralError(Box::new(PoisonedMutexError))
    }
}

impl From<esplugin::Error> for GeneralError {
    fn from(value: esplugin::Error) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<Box<fancy_regex::Error>> for GeneralError {
    fn from(value: Box<fancy_regex::Error>) -> Self {
        GeneralError(value)
    }
}

impl From<UndefinedGroupError> for GeneralError {
    fn from(value: UndefinedGroupError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<CyclicInteractionError> for GeneralError {
    fn from(value: CyclicInteractionError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<PathfindingError> for GeneralError {
    fn from(value: PathfindingError) -> Self {
        GeneralError(Box::new(value))
    }
}

impl From<petgraph::algo::Cycle<NodeIndex>> for GeneralError {
    fn from(_: petgraph::algo::Cycle<NodeIndex>) -> Self {
        GeneralError(Box::new(CyclicInteractionError { cycle: Vec::new() }))
    }
}

impl From<SortingLogicError> for GeneralError {
    fn from(value: SortingLogicError) -> Self {
        GeneralError(Box::new(value))
    }
}
