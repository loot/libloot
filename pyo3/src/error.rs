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
use libloot_ffi_errors::{
    SystemError, UnsupportedEnumValueError, fmt_error_chain, variant_box_from_error,
};
use pyo3::{PyErr, exceptions::PyValueError};

use crate::{CyclicInteractionError, EspluginError, UndefinedGroupError, database::Vertex};

#[derive(Debug)]
pub enum VerboseError {
    CyclicInteractionError(Vec<libloot::Vertex>),
    UndefinedGroupError(String),
    EspluginError(SystemError),
    Other(Box<dyn std::error::Error>),
}

impl std::fmt::Display for VerboseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::CyclicInteractionError(c) => SortPluginsError::CycleFound(c.clone()).fmt(f),
            Self::UndefinedGroupError(g) => SortPluginsError::UndefinedGroup(g.clone()).fmt(f),
            Self::EspluginError(e) => e.message().fmt(f),
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

impl From<GameHandleCreationError> for VerboseError {
    fn from(value: GameHandleCreationError) -> Self {
        match value {
            GameHandleCreationError::LoadOrderError(e) => e.into(),
            GameHandleCreationError::NotADirectory(_) | _ => Self::Other(Box::new(value)),
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
        Self::EspluginError(SystemError::from(value))
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
            VerboseError::EspluginError(e) => {
                PyErr::new::<EspluginError, _>((e.code(), e.message().to_owned()))
            }
            VerboseError::Other(_) => PyValueError::new_err(message),
        }
    }
}
