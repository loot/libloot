use crate::game::NotValidUtf8;
use libloot_ffi_errors::SystemError;

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
    EspluginError(i32, String),
    LibloadorderError(i32, String),
    LciError(i32, String),
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
                    let name = vertex.name().replace("\\", "\\\\").replace("-", "\\-");
                    match vertex.out_edge_type() {
                        Some(e) => write!(f, "{}--{}--", name, e)?,
                        None => write!(f, "{}", name)?,
                    }
                }
                Ok(())
            }
            Self::UndefinedGroupError(group) => {
                write!(f, "UndefinedGroupError: {}", group)
            }
            Self::EspluginError(c, s) => {
                write!(f, "EspluginError: {}: {}", c, s)
            }
            Self::LibloadorderError(c, s) => {
                write!(f, "LibloadorderError: {}: {}", c, s)
            }
            Self::LciError(c, s) => {
                write!(f, "LciError: {}: {}", c, s)
            }
            Self::FileAccessError(s) => write!(f, "FileAccessError: {}", s),
            Self::InvalidArgument(s) => write!(f, "InvalidArgument: {}", s),
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
            GameHandleCreationError::NotADirectory(_) => Self::InvalidArgument(value.to_string()),
            _ => Self::Other(Box::new(value)),
        }
    }
}

impl From<UnsupportedEnumValueError> for VerboseError {
    fn from(value: UnsupportedEnumValueError) -> Self {
        Self::Other(Box::new(value))
    }
}

impl From<NotValidUtf8> for VerboseError {
    fn from(value: NotValidUtf8) -> Self {
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
            LoadPluginsError::PluginValidationError(_) => Self::InvalidArgument(value.to_string()),
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
        let error = SystemError::from(value);
        Self::LibloadorderError(error.code(), error.message().to_string())
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
        let error = SystemError::from(value);
        Self::LciError(error.code(), error.message().to_string())
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

#[derive(Debug)]
pub struct EmptyOptionalError;

impl std::fmt::Display for EmptyOptionalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "optional is empty")
    }
}

impl std::error::Error for EmptyOptionalError {}

#[derive(Debug)]
pub struct UnsupportedEnumValueError;

impl std::fmt::Display for UnsupportedEnumValueError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Enum value is unsupported")
    }
}

impl std::error::Error for UnsupportedEnumValueError {}
