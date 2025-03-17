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

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
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
