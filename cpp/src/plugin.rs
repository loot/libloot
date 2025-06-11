use std::sync::Arc;

use delegate::delegate;

use crate::{OptionalCrc, VerboseError};

#[derive(Debug)]
#[repr(transparent)]
pub struct Plugin(Arc<libloot::Plugin>);

impl Plugin {
    pub fn new(plugin: Arc<libloot::Plugin>) -> Self {
        Self(plugin)
    }

    pub fn header_version(&self) -> f32 {
        self.0.header_version().unwrap_or(f32::NAN)
    }

    pub fn version(&self) -> &str {
        self.0.version().unwrap_or("")
    }

    pub fn masters(&self) -> Result<Vec<String>, VerboseError> {
        self.0.masters().map_err(Into::into)
    }

    pub fn crc(&self) -> Box<OptionalCrc> {
        Box::new(self.0.crc().into())
    }

    pub fn is_valid_as_light_plugin(&self) -> Result<bool, VerboseError> {
        self.0.is_valid_as_light_plugin().map_err(Into::into)
    }

    pub fn is_valid_as_medium_plugin(&self) -> Result<bool, VerboseError> {
        self.0.is_valid_as_medium_plugin().map_err(Into::into)
    }

    pub fn is_valid_as_update_plugin(&self) -> Result<bool, VerboseError> {
        self.0.is_valid_as_update_plugin().map_err(Into::into)
    }

    pub fn do_records_overlap(&self, plugin: &Self) -> Result<bool, VerboseError> {
        self.0.do_records_overlap(&plugin.0).map_err(Into::into)
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(Arc::clone(&self.0)))
    }

    delegate! {
        to self.0 {
            pub fn name(&self) -> &str;

            pub fn bash_tags(&self) -> &[String];

            pub fn is_master(&self) -> bool;

            pub fn is_light_plugin(&self) -> bool;

            pub fn is_medium_plugin(&self) -> bool;

            pub fn is_update_plugin(&self) -> bool;

            pub fn is_blueprint_plugin(&self) -> bool;

            pub fn is_empty(&self) -> bool;

            pub fn loads_archive(&self) -> bool;
        }
    }
}

impl From<Arc<libloot::Plugin>> for Plugin {
    fn from(value: Arc<libloot::Plugin>) -> Self {
        Plugin(value)
    }
}
