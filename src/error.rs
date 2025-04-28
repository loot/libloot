//! Holds all error types aside from those related to LOOT metadata.
use std::path::PathBuf;

pub use crate::database::{ConditionEvaluationError, MetadataRetrievalError};
pub use crate::plugin::error::PluginDataError;
use crate::plugin::error::PluginValidationError;
pub use crate::sorting::error::GroupsPathError;

use crate::sorting::error::{
    BuildGroupsGraphError, PluginGraphValidationError, SortingError, display_cycle,
};
use crate::{Vertex, escape_ascii};

/// Represents an error that occurred while trying to create a [Game][crate::Game].
#[derive(Debug)]
#[non_exhaustive]
pub enum GameHandleCreationError {
    NotADirectory(PathBuf),
    LoadOrderError(LoadOrderError),
}

impl std::fmt::Display for GameHandleCreationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NotADirectory(p) => write!(
                f,
                "the path \"{}\" does not resolve to a directory",
                escape_ascii(p)
            ),
            Self::LoadOrderError(_) => {
                write!(f, "failed to initialise the load order game settings")
            }
        }
    }
}

impl std::error::Error for GameHandleCreationError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::NotADirectory(_) => None,
            Self::LoadOrderError(e) => Some(e),
        }
    }
}

impl From<loadorder::Error> for GameHandleCreationError {
    fn from(value: loadorder::Error) -> Self {
        GameHandleCreationError::LoadOrderError(value.into())
    }
}

/// Represents an error that occurred while trying to interact with the load order.
#[derive(Debug)]
pub struct LoadOrderError(Box<loadorder::Error>);

impl std::fmt::Display for LoadOrderError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "load order interaction failed")
    }
}

impl std::error::Error for LoadOrderError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.0)
    }
}

impl From<loadorder::Error> for LoadOrderError {
    fn from(value: loadorder::Error) -> Self {
        LoadOrderError(Box::new(value))
    }
}

/// Indicates that the Database's RwLock wrapper has been poisoned and as such
/// the Database may be in an invalid state.
#[derive(Clone, Copy, Default, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct DatabaseLockPoisonError;

impl std::fmt::Display for DatabaseLockPoisonError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "the database's lock has been poisoned")
    }
}

impl std::error::Error for DatabaseLockPoisonError {}

impl<T> From<std::sync::PoisonError<T>> for DatabaseLockPoisonError {
    fn from(_: std::sync::PoisonError<T>) -> Self {
        DatabaseLockPoisonError
    }
}

/// Represents an error that occurred while loading plugins.
#[derive(Debug)]
#[non_exhaustive]
pub enum LoadPluginsError {
    DatabaseLockPoisoned,
    IoError(Box<std::io::Error>),
    PluginValidationError(Box<dyn std::error::Error + Send + Sync + 'static>),
    PluginDataError(PluginDataError),
}

impl std::fmt::Display for LoadPluginsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::DatabaseLockPoisoned => DatabaseLockPoisonError.fmt(f),
            Self::IoError(_) => write!(f, "an I/O error occurred"),
            Self::PluginValidationError(_) => write!(f, "failed validation of input plugin paths"),
            Self::PluginDataError(_) => write!(f, "failed to read loaded plugin data"),
        }
    }
}

impl std::error::Error for LoadPluginsError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::DatabaseLockPoisoned => None,
            Self::IoError(e) => Some(e),
            Self::PluginValidationError(e) => Some(e.as_ref()),
            Self::PluginDataError(e) => Some(e),
        }
    }
}

impl<T> From<std::sync::PoisonError<T>> for LoadPluginsError {
    fn from(_: std::sync::PoisonError<T>) -> Self {
        LoadPluginsError::DatabaseLockPoisoned
    }
}

impl From<DatabaseLockPoisonError> for LoadPluginsError {
    fn from(_: DatabaseLockPoisonError) -> Self {
        LoadPluginsError::DatabaseLockPoisoned
    }
}

impl From<std::io::Error> for LoadPluginsError {
    fn from(value: std::io::Error) -> Self {
        LoadPluginsError::IoError(Box::new(value))
    }
}

impl From<PluginValidationError> for LoadPluginsError {
    fn from(value: PluginValidationError) -> Self {
        LoadPluginsError::PluginValidationError(Box::new(value))
    }
}

impl From<PluginDataError> for LoadPluginsError {
    fn from(value: PluginDataError) -> Self {
        LoadPluginsError::PluginDataError(value)
    }
}

/// Represents an error that occurred while trying to load the current load
/// order state.
#[derive(Debug)]
#[non_exhaustive]
pub enum LoadOrderStateError {
    DatabaseLockPoisoned,
    LoadOrderError(LoadOrderError),
}

impl std::fmt::Display for LoadOrderStateError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::DatabaseLockPoisoned => DatabaseLockPoisonError.fmt(f),
            Self::LoadOrderError(_) => write!(f, "failed to load the current load order state"),
        }
    }
}

impl std::error::Error for LoadOrderStateError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::DatabaseLockPoisoned => None,
            Self::LoadOrderError(e) => Some(e),
        }
    }
}

impl<T> From<std::sync::PoisonError<T>> for LoadOrderStateError {
    fn from(_: std::sync::PoisonError<T>) -> Self {
        LoadOrderStateError::DatabaseLockPoisoned
    }
}

impl From<loadorder::Error> for LoadOrderStateError {
    fn from(value: loadorder::Error) -> Self {
        LoadOrderStateError::LoadOrderError(value.into())
    }
}

/// Represents an error that occurred during sorting.
#[derive(Debug)]
#[non_exhaustive]
pub enum SortPluginsError {
    DatabaseLockPoisoned,
    PluginNotLoaded(String),
    MetadataRetrievalError(MetadataRetrievalError),
    UndefinedGroup(String),
    CycleFound(Vec<Vertex>),
    CycleFoundInvolving(String),
    PluginDataError(PluginDataError),
    PathfindingError(Box<dyn std::error::Error + Send + Sync + 'static>),
}

impl std::fmt::Display for SortPluginsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::DatabaseLockPoisoned => DatabaseLockPoisonError.fmt(f),
            Self::PluginNotLoaded(n) => write!(f, "the plugin \"{n}\" has not been loaded"),
            Self::UndefinedGroup(g) => write!(f, "the group \"{g}\" does not exist"),
            Self::CycleFound(c) => write!(f, "found a cycle: {}", display_cycle(c)),
            Self::CycleFoundInvolving(n) => write!(f, "found a cycle involving \"{n}\""),
            Self::PluginDataError(_) => write!(f, "failed to read loaded plugin data"),
            Self::MetadataRetrievalError(_) => write!(f, "failed to retrieve plugin metadata"),
            Self::PathfindingError(_) => write!(f, "failed to find a path in the plugins graph"),
        }
    }
}

impl std::error::Error for SortPluginsError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::MetadataRetrievalError(e) => Some(e),
            Self::PluginDataError(e) => Some(e),
            Self::PathfindingError(e) => Some(e.as_ref()),
            _ => None,
        }
    }
}

impl<T> From<std::sync::PoisonError<T>> for SortPluginsError {
    fn from(_: std::sync::PoisonError<T>) -> Self {
        SortPluginsError::DatabaseLockPoisoned
    }
}

impl From<SortingError> for SortPluginsError {
    fn from(value: SortingError) -> Self {
        match value {
            SortingError::ValidationError(e) => match e {
                PluginGraphValidationError::CycleFound(c) => Self::CycleFound(c.into_cycle()),
                PluginGraphValidationError::PluginDataError(e) => Self::PluginDataError(e),
            },
            SortingError::UndefinedGroup(g) => Self::UndefinedGroup(g.into_group_name()),
            SortingError::CycleFound(c) => Self::CycleFound(c.into_cycle()),
            SortingError::CycleInvolving(n) => Self::CycleFoundInvolving(n),
            SortingError::PluginDataError(e) => Self::PluginDataError(e),
            SortingError::PathfindingError(e) => Self::PathfindingError(Box::new(e)),
        }
    }
}

impl From<BuildGroupsGraphError> for SortPluginsError {
    fn from(value: BuildGroupsGraphError) -> Self {
        match value {
            BuildGroupsGraphError::UndefinedGroup(g) => Self::UndefinedGroup(g.into_group_name()),
            BuildGroupsGraphError::CycleFound(c) => Self::CycleFound(c.into_cycle()),
        }
    }
}

impl From<PluginDataError> for SortPluginsError {
    fn from(value: PluginDataError) -> Self {
        SortPluginsError::PluginDataError(value)
    }
}

impl From<MetadataRetrievalError> for SortPluginsError {
    fn from(value: MetadataRetrievalError) -> Self {
        SortPluginsError::MetadataRetrievalError(value)
    }
}

impl From<ConditionEvaluationError> for SortPluginsError {
    fn from(value: ConditionEvaluationError) -> Self {
        SortPluginsError::MetadataRetrievalError(MetadataRetrievalError::ConditionEvaluationError(
            value,
        ))
    }
}
