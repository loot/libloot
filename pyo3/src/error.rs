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
use libloot_ffi_errors::SystemError;
use pyo3::{PyErr, exceptions::PyValueError};

use crate::{CyclicInteractionError, EspluginError, UndefinedGroupError, database::Vertex};

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct UnsupportedEnumValueError;

impl std::fmt::Display for UnsupportedEnumValueError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Enum value is unsupported")
    }
}

impl std::error::Error for UnsupportedEnumValueError {}

impl From<UnsupportedEnumValueError> for PyErr {
    fn from(value: UnsupportedEnumValueError) -> Self {
        PyValueError::new_err(value.to_string())
    }
}

#[derive(Debug)]
pub enum VerboseError {
    CyclicInteractionError(Vec<libloot::Vertex>),
    UndefinedGroupError(String),
    EspluginError(i32, String),
    Other(Box<dyn std::error::Error>),
}

impl std::fmt::Display for VerboseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::CyclicInteractionError(c) => {
                let cycle = display_cycle(c);
                write!(f, "cyclic interaction detected: {}", cycle)
            }
            Self::UndefinedGroupError(g) => {
                write!(f, "the group \"{}\" does not exist", g)
            }
            Self::EspluginError(_, s) => s.fmt(f),
            Self::Other(e) => {
                write!(f, "{}", e)?;
                let mut error = e.as_ref();
                while let Some(source) = error.source() {
                    write!(f, ": {}", source)?;
                    error = source;
                }
                Ok(())
            }
        }
    }
}

impl From<GameHandleCreationError> for VerboseError {
    fn from(value: GameHandleCreationError) -> Self {
        match value {
            GameHandleCreationError::LoadOrderError(e) => e.into(),
            _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<UnsupportedEnumValueError> for VerboseError {
    fn from(value: UnsupportedEnumValueError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<DatabaseLockPoisonError> for VerboseError {
    fn from(value: DatabaseLockPoisonError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<LoadPluginsError> for VerboseError {
    fn from(value: LoadPluginsError) -> Self {
        match value {
            LoadPluginsError::PluginDataError(e) => e.into(),
            _ => Self::Other(Box::new(value)),
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
            _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<LoadOrderStateError> for VerboseError {
    fn from(value: LoadOrderStateError) -> Self {
        match value {
            LoadOrderStateError::LoadOrderError(e) => e.into(),
            _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<LoadOrderError> for VerboseError {
    fn from(value: LoadOrderError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<LoadMetadataError> for VerboseError {
    fn from(value: LoadMetadataError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<WriteMetadataError> for VerboseError {
    fn from(value: WriteMetadataError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<ConditionEvaluationError> for VerboseError {
    fn from(value: ConditionEvaluationError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<GroupsPathError> for VerboseError {
    fn from(value: GroupsPathError) -> Self {
        match value {
            GroupsPathError::UndefinedGroup(g) => Self::UndefinedGroupError(g),
            GroupsPathError::CycleFound(cycle) => Self::CyclicInteractionError(cycle),
            _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<MetadataRetrievalError> for VerboseError {
    fn from(value: MetadataRetrievalError) -> Self {
        match value {
            MetadataRetrievalError::ConditionEvaluationError(e) => e.into(),
            _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<PluginDataError> for VerboseError {
    fn from(value: PluginDataError) -> Self {
        let error = SystemError::from(value);
        Self::EspluginError(error.code(), error.message().to_string())
    }
}

impl From<MultilingualMessageContentsError> for VerboseError {
    fn from(value: MultilingualMessageContentsError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<RegexError> for VerboseError {
    fn from(value: RegexError) -> Self {
        Self::Other(Box::new(value))
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
            VerboseError::EspluginError(i, s) => PyErr::new::<EspluginError, _>((i, s)),
            VerboseError::Other(_) => PyValueError::new_err(message),
        }
    }
}

fn display_cycle(cycle: &[libloot::Vertex]) -> String {
    cycle
        .iter()
        .map(|v| {
            if let Some(edge_type) = v.out_edge_type() {
                format!("{} --[{}]-> ", v.name(), edge_type)
            } else {
                v.name().to_string()
            }
        })
        .chain(cycle.first().iter().map(|v| v.name().to_string()))
        .collect()
}
