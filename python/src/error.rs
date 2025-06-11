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
use libloot_ffi_errors::{UnsupportedEnumValueError, fmt_error_chain, variant_box_from_error};
use pyo3::{PyErr, exceptions::PyValueError};

use crate::{CyclicInteractionError, PluginNotLoadedError, UndefinedGroupError, database::Vertex};

#[derive(Debug)]
pub enum VerboseError {
    CyclicInteractionError(Vec<libloot::Vertex>),
    UndefinedGroupError(String),
    PluginNotLoadedError(String),
    Other(Box<dyn std::error::Error>),
}

impl std::fmt::Display for VerboseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::CyclicInteractionError(c) => SortPluginsError::CycleFound(c.clone()).fmt(f),
            Self::UndefinedGroupError(g) => SortPluginsError::UndefinedGroup(g.clone()).fmt(f),
            Self::PluginNotLoadedError(p) => SortPluginsError::PluginNotLoaded(p.clone()).fmt(f),
            Self::Other(e) => fmt_error_chain(e.as_ref(), f),
        }
    }
}

variant_box_from_error!(UnsupportedEnumValueError, VerboseError::Other);
variant_box_from_error!(DatabaseLockPoisonError, VerboseError::Other);
variant_box_from_error!(LoadPluginsError, VerboseError::Other);
variant_box_from_error!(LoadOrderError, VerboseError::Other);
variant_box_from_error!(LoadMetadataError, VerboseError::Other);
variant_box_from_error!(WriteMetadataError, VerboseError::Other);
variant_box_from_error!(ConditionEvaluationError, VerboseError::Other);
variant_box_from_error!(MultilingualMessageContentsError, VerboseError::Other);
variant_box_from_error!(RegexError, VerboseError::Other);
variant_box_from_error!(GameHandleCreationError, VerboseError::Other);
variant_box_from_error!(LoadOrderStateError, VerboseError::Other);
variant_box_from_error!(MetadataRetrievalError, VerboseError::Other);
variant_box_from_error!(PluginDataError, VerboseError::Other);

impl From<SortPluginsError> for VerboseError {
    fn from(value: SortPluginsError) -> Self {
        match value {
            SortPluginsError::UndefinedGroup(g) => Self::UndefinedGroupError(g),
            SortPluginsError::CycleFound(cycle) => Self::CyclicInteractionError(cycle),
            SortPluginsError::PluginNotLoaded(n) => Self::PluginNotLoadedError(n),
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

impl From<VerboseError> for PyErr {
    fn from(value: VerboseError) -> Self {
        let message = value.to_string();

        match value {
            VerboseError::CyclicInteractionError(c) => PyErr::new::<CyclicInteractionError, _>((
                c.into_iter().map(Vertex::from).collect::<Vec<_>>(),
                message,
            )),
            VerboseError::UndefinedGroupError(g) => {
                PyErr::new::<UndefinedGroupError, _>((g, message))
            }
            VerboseError::PluginNotLoadedError(p) => {
                PyErr::new::<PluginNotLoadedError, _>((p, message))
            }
            VerboseError::Other(_) => PyValueError::new_err(message),
        }
    }
}
