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
use libloot_ffi_errors::{fmt_error_chain, SystemError, UnsupportedEnumValueError};

#[derive(Debug)]
pub struct VerboseError(Box<dyn std::error::Error>);

impl std::fmt::Display for VerboseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        fmt_error_chain(self.0.as_ref(), f)
    }
}

macro_rules! box_from_error {
    ( $from_type:ident, $to_type:ident ) => {
        impl From<$from_type> for $to_type {
            fn from(value: $from_type) -> Self {
                Self(Box::new(value))
            }
        }
    };
}

box_from_error!(GameHandleCreationError, VerboseError);
box_from_error!(UnsupportedEnumValueError, VerboseError);
box_from_error!(DatabaseLockPoisonError, VerboseError);
box_from_error!(LoadPluginsError, VerboseError);
box_from_error!(SortPluginsError, VerboseError);
box_from_error!(LoadOrderStateError, VerboseError);
box_from_error!(LoadOrderError, VerboseError);
box_from_error!(LoadMetadataError, VerboseError);
box_from_error!(WriteMetadataError, VerboseError);
box_from_error!(ConditionEvaluationError, VerboseError);
box_from_error!(GroupsPathError, VerboseError);
box_from_error!(MetadataRetrievalError, VerboseError);
box_from_error!(MultilingualMessageContentsError, VerboseError);
box_from_error!(RegexError, VerboseError);

impl From<PluginDataError> for VerboseError {
    fn from(value: PluginDataError) -> Self {
        Self(Box::new(SystemError::from(value)))
    }
}

impl From<VerboseError> for napi::Error {
    fn from(value: VerboseError) -> Self {
        napi::Error::from_reason(value.to_string())
    }
}

impl From<VerboseError> for napi::JsError {
    fn from(value: VerboseError) -> Self {
        napi::JsError::from(napi::Error::from(value))
    }
}
