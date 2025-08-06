use crate::game::NotValidUtf8;
use libloot_ffi_errors::{UnsupportedEnumValueError, fmt_error_chain, variant_box_from_error};

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
    PluginNotLoadedError(String),
    InvalidArgument(Box<dyn std::error::Error>),
    Other(Box<dyn std::error::Error>),
}

impl std::fmt::Display for VerboseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::CyclicInteractionError(cycle) => {
                write!(f, "CyclicInteractionError: ")?;
                for vertex in cycle {
                    let name = vertex.name().replace('\\', "\\\\").replace('>', "\\>");
                    match vertex.out_edge_type() {
                        Some(e) => write!(f, "{name} > {e} > ")?,
                        None => write!(f, "{name}")?,
                    }
                }
                Ok(())
            }
            Self::UndefinedGroupError(group) => {
                write!(f, "UndefinedGroupError: {group}",)
            }
            Self::PluginNotLoadedError(plugin) => write!(f, "PluginNotLoadedError: {plugin}"),
            Self::InvalidArgument(e) => {
                write!(f, "InvalidArgument: ")?;
                fmt_error_chain(e.as_ref(), f)
            }
            Self::Other(e) => fmt_error_chain(e.as_ref(), f),
        }
    }
}

variant_box_from_error!(UnsupportedEnumValueError, VerboseError::Other);
variant_box_from_error!(NotValidUtf8, VerboseError::Other);
variant_box_from_error!(DatabaseLockPoisonError, VerboseError::Other);
variant_box_from_error!(MultilingualMessageContentsError, VerboseError::Other);
variant_box_from_error!(RegexError, VerboseError::Other);
variant_box_from_error!(LoadMetadataError, VerboseError::Other);
variant_box_from_error!(WriteMetadataError, VerboseError::Other);
variant_box_from_error!(ConditionEvaluationError, VerboseError::Other);
variant_box_from_error!(MetadataRetrievalError, VerboseError::Other);
variant_box_from_error!(LoadOrderError, VerboseError::Other);
variant_box_from_error!(LoadOrderStateError, VerboseError::Other);
variant_box_from_error!(PluginDataError, VerboseError::Other);

impl From<GameHandleCreationError> for VerboseError {
    fn from(value: GameHandleCreationError) -> Self {
        match value {
            GameHandleCreationError::NotADirectory(_) => Self::InvalidArgument(Box::new(value)),
            GameHandleCreationError::LoadOrderError(_) | _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<LoadPluginsError> for VerboseError {
    fn from(value: LoadPluginsError) -> Self {
        match value {
            LoadPluginsError::PluginNotLoaded(p) => Self::PluginNotLoadedError(p),
            LoadPluginsError::PluginValidationError(_) => Self::InvalidArgument(Box::new(value)),
            LoadPluginsError::DatabaseLockPoisoned
            | LoadPluginsError::IoError(_)
            | LoadPluginsError::PluginDataError(_)
            | _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<SortPluginsError> for VerboseError {
    fn from(value: SortPluginsError) -> Self {
        match value {
            SortPluginsError::UndefinedGroup(g) => Self::UndefinedGroupError(g),
            SortPluginsError::CycleFound(cycle) => Self::CyclicInteractionError(cycle),
            SortPluginsError::PluginNotLoaded(p) => Self::PluginNotLoadedError(p),
            SortPluginsError::DatabaseLockPoisoned
            | SortPluginsError::CycleFoundInvolving(_)
            | SortPluginsError::PathfindingError(_)
            | SortPluginsError::PluginDataError(_)
            | _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<GroupsPathError> for VerboseError {
    fn from(value: GroupsPathError) -> Self {
        match value {
            GroupsPathError::UndefinedGroup(g) => Self::UndefinedGroupError(g),
            GroupsPathError::CycleFound(cycle) => Self::CyclicInteractionError(cycle),
            GroupsPathError::PathfindingError(_) | _ => Self::Other(Box::new(value)),
        }
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
