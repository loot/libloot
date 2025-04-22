use crate::game::NotValidUtf8;
use libloot_ffi_errors::{
    SystemError, SystemErrorCategory, UnsupportedEnumValueError, fmt_error_chain,
    variant_box_from_error,
};

use libloot::{
    error::{
        ConditionEvaluationError, DatabaseLockPoisonError, GameHandleCreationError,
        GroupsPathError, LoadOrderError, LoadOrderStateError, LoadPluginsError,
        MetadataRetrievalError, PluginDataError, SortPluginsError,
    },
    metadata::error::{
        LoadMetadataError, MultilingualMessageContentsError, RegexError, WriteMetadataError,
    },
};

#[derive(Debug)]
pub enum VerboseError {
    CyclicInteractionError(Vec<libloot::Vertex>),
    UndefinedGroupError(String),
    SystemError(SystemError),
    FileAccessError(String),
    InvalidArgument(String),
    Other(Box<dyn std::error::Error>),
}

impl std::fmt::Display for VerboseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::CyclicInteractionError(cycle) => {
                write!(f, "CyclicInteractionError: ")?;
                for vertex in cycle {
                    let name = vertex.name().replace('\\', "\\\\").replace('-', "\\-");
                    match vertex.out_edge_type() {
                        Some(e) => write!(f, "{name}--{e}--")?,
                        None => write!(f, "{name}")?,
                    }
                }
                Ok(())
            }
            Self::UndefinedGroupError(group) => {
                write!(f, "UndefinedGroupError: {group}",)
            }
            Self::SystemError(e) => {
                let prefix = match e.category() {
                    SystemErrorCategory::Esplugin => "EspluginError",
                    SystemErrorCategory::Libloadorder => "LibloadorderError",
                    SystemErrorCategory::LootConditionInterpreter => "LciError",
                    _ => "UnknownCategoryError",
                };
                write!(f, "{}: {}: {}", prefix, e.code(), e.message())
            }
            Self::FileAccessError(s) => write!(f, "FileAccessError: {s}"),
            Self::InvalidArgument(s) => write!(f, "InvalidArgument: {s}"),
            Self::Other(e) => fmt_error_chain(e.as_ref(), f),
        }
    }
}

variant_box_from_error!(UnsupportedEnumValueError, VerboseError::Other);
variant_box_from_error!(NotValidUtf8, VerboseError::Other);
variant_box_from_error!(DatabaseLockPoisonError, VerboseError::Other);
variant_box_from_error!(MultilingualMessageContentsError, VerboseError::Other);
variant_box_from_error!(RegexError, VerboseError::Other);

impl From<GameHandleCreationError> for VerboseError {
    fn from(value: GameHandleCreationError) -> Self {
        match value {
            GameHandleCreationError::LoadOrderError(e) => e.into(),
            GameHandleCreationError::NotADirectory(_) => Self::InvalidArgument(value.to_string()),
            _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<LoadPluginsError> for VerboseError {
    fn from(value: LoadPluginsError) -> Self {
        match value {
            LoadPluginsError::PluginDataError(e) => e.into(),
            LoadPluginsError::PluginValidationError(_) => Self::InvalidArgument(value.to_string()),
            LoadPluginsError::DatabaseLockPoisoned | LoadPluginsError::IoError(_) | _ => {
                Self::Other(Box::new(value))
            }
        }
    }
}

impl From<SortPluginsError> for VerboseError {
    fn from(value: SortPluginsError) -> Self {
        match value {
            SortPluginsError::MetadataRetrievalError(e) => e.into(),
            SortPluginsError::UndefinedGroup(g) => Self::UndefinedGroupError(g),
            SortPluginsError::CycleFound(cycle) => Self::CyclicInteractionError(cycle),
            SortPluginsError::PluginDataError(e) => e.into(),
            SortPluginsError::DatabaseLockPoisoned
            | SortPluginsError::PluginNotLoaded(_)
            | SortPluginsError::CycleFoundInvolving(_)
            | SortPluginsError::PathfindingError(_)
            | _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<LoadOrderStateError> for VerboseError {
    fn from(value: LoadOrderStateError) -> Self {
        match value {
            LoadOrderStateError::LoadOrderError(e) => e.into(),
            LoadOrderStateError::DatabaseLockPoisoned | _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<LoadOrderError> for VerboseError {
    fn from(value: LoadOrderError) -> Self {
        Self::SystemError(SystemError::from(value))
    }
}

impl From<LoadMetadataError> for VerboseError {
    fn from(value: LoadMetadataError) -> Self {
        Self::FileAccessError(value.to_string())
    }
}

impl From<WriteMetadataError> for VerboseError {
    fn from(value: WriteMetadataError) -> Self {
        Self::FileAccessError(value.to_string())
    }
}

impl From<ConditionEvaluationError> for VerboseError {
    fn from(value: ConditionEvaluationError) -> Self {
        Self::SystemError(SystemError::from(value))
    }
}

impl From<GroupsPathError> for VerboseError {
    fn from(value: GroupsPathError) -> Self {
        match value {
            GroupsPathError::UndefinedGroup(g) => Self::UndefinedGroupError(g),
            GroupsPathError::CycleFound(cycle) => Self::CyclicInteractionError(cycle),
            GroupsPathError::PathfindingError(_) => Self::Other(Box::new(value)),
        }
    }
}

impl From<MetadataRetrievalError> for VerboseError {
    fn from(value: MetadataRetrievalError) -> Self {
        match value {
            MetadataRetrievalError::ConditionEvaluationError(e) => e.into(),
            MetadataRetrievalError::RegexError(_) => Self::Other(Box::new(value)),
        }
    }
}

impl From<PluginDataError> for VerboseError {
    fn from(value: PluginDataError) -> Self {
        Self::SystemError(SystemError::from(value))
    }
}

#[derive(Clone, Copy, Debug)]
pub struct EmptyOptionalError;

impl std::fmt::Display for EmptyOptionalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "optional is empty")
    }
}

impl std::error::Error for EmptyOptionalError {}
