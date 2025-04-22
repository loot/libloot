use std::{
    error::Error,
    ffi::{c_int, c_uchar},
};

use libloot::error::{ConditionEvaluationError, LoadOrderError, PluginDataError};

pub mod esplugin;
pub mod lci;
pub mod libloadorder;

/// An error from esplugin.
#[unsafe(no_mangle)]
pub static LIBLOOT_SYSTEM_ERROR_CATEGORY_ESPLUGIN: c_uchar = 1;

/// An error from libloadorder.
#[unsafe(no_mangle)]
pub static LIBLOOT_SYSTEM_ERROR_CATEGORY_LIBLOADORDER: c_uchar = 2;

/// An error from loot-condition-interpreter.
#[unsafe(no_mangle)]
pub static LIBLOOT_SYSTEM_ERROR_CATEGORY_LCI: c_uchar = 3;

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(u8)]
pub enum SystemErrorCategory {
    Esplugin = LIBLOOT_SYSTEM_ERROR_CATEGORY_ESPLUGIN,
    Libloadorder = LIBLOOT_SYSTEM_ERROR_CATEGORY_LIBLOADORDER,
    LootConditionInterpreter = LIBLOOT_SYSTEM_ERROR_CATEGORY_LCI,
}

impl std::fmt::Display for SystemErrorCategory {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            SystemErrorCategory::Esplugin => write!(f, "esplugin"),
            SystemErrorCategory::Libloadorder => write!(f, "libloadorder"),
            SystemErrorCategory::LootConditionInterpreter => {
                write!(f, "loot-condition-interpreter")
            }
        }
    }
}

#[derive(Debug)]
pub struct SystemError {
    code: c_int,
    category: SystemErrorCategory,
    message: String,
}

impl SystemError {
    pub fn code(&self) -> c_int {
        self.code
    }

    pub fn category(&self) -> SystemErrorCategory {
        self.category
    }

    pub fn message(&self) -> &str {
        &self.message
    }
}

impl std::fmt::Display for SystemError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} error, code {}: {}",
            self.category, self.code, self.message
        )
    }
}

impl std::error::Error for SystemError {}

impl From<PluginDataError> for SystemError {
    fn from(value: PluginDataError) -> Self {
        let error = value
            .source()
            .expect("LoadOrderError has source")
            .downcast_ref::<::esplugin::Error>()
            .expect("LoadOrderError source is an esplugin::Error");
        let error_code = crate::esplugin::map_error(error);
        SystemError {
            code: error_code,
            category: SystemErrorCategory::Esplugin,
            message: error.to_string(),
        }
    }
}

impl From<LoadOrderError> for SystemError {
    fn from(value: LoadOrderError) -> Self {
        let error = value
            .source()
            .expect("LoadOrderError has source")
            .downcast_ref::<loadorder::Error>()
            .expect("LoadOrderError source is a loadorder::Error");
        let error_code = crate::libloadorder::map_error(error);
        SystemError {
            code: error_code,
            category: SystemErrorCategory::Libloadorder,
            message: error.to_string(),
        }
    }
}

impl From<ConditionEvaluationError> for SystemError {
    fn from(value: ConditionEvaluationError) -> Self {
        let error = value
            .source()
            .expect("ConditionEvaluationError has source")
            .downcast_ref::<loot_condition_interpreter::Error>()
            .expect("ConditionEvaluationError source is a loot_condition_interpreter::Error");
        let error_code = crate::lci::map_error(error);
        SystemError {
            code: error_code,
            category: SystemErrorCategory::LootConditionInterpreter,
            message: error.to_string(),
        }
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct UnsupportedEnumValueError;

impl std::fmt::Display for UnsupportedEnumValueError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Enum value is unsupported")
    }
}

impl std::error::Error for UnsupportedEnumValueError {}

pub fn fmt_error_chain(
    mut error: &dyn std::error::Error,
    f: &mut std::fmt::Formatter<'_>,
) -> std::fmt::Result {
    write!(f, "{}", error)?;
    while let Some(source) = error.source() {
        write!(f, ": {}", source)?;
        error = source;
    }
    Ok(())
}

#[macro_export]
macro_rules! variant_box_from_error {
    ( $from_type:ident, $to_type:ident::$to_variant:ident ) => {
        impl From<$from_type> for $to_type {
            fn from(value: $from_type) -> Self {
                Self::$to_variant(Box::new(value))
            }
        }
    };
}
