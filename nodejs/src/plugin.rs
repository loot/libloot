use std::sync::Arc;

use napi_derive::napi;

use crate::error::VerboseError;

#[napi]
#[derive(Clone, Debug, Eq, PartialEq)]
#[repr(transparent)]
pub struct Plugin(Arc<libloot::Plugin>);

#[napi]
impl Plugin {
    #[napi]
    pub fn name(&self) -> &str {
        self.0.name()
    }

    #[napi]
    pub fn header_version(&self) -> Option<f32> {
        self.0.header_version()
    }

    #[napi]
    pub fn version(&self) -> Option<&str> {
        self.0.version()
    }

    #[napi]
    pub fn masters(&self) -> Result<Vec<String>, VerboseError> {
        Ok(self.0.masters()?)
    }

    #[napi]
    pub fn bash_tags(&self) -> Vec<String> {
        self.0.bash_tags().to_vec()
    }

    #[napi]
    pub fn crc(&self) -> Option<u32> {
        self.0.crc()
    }

    #[napi]
    pub fn is_master(&self) -> bool {
        self.0.is_master()
    }

    #[napi]
    pub fn is_light_plugin(&self) -> bool {
        self.0.is_light_plugin()
    }

    #[napi]
    pub fn is_medium_plugin(&self) -> bool {
        self.0.is_medium_plugin()
    }

    #[napi]
    pub fn is_update_plugin(&self) -> bool {
        self.0.is_update_plugin()
    }

    #[napi]
    pub fn is_blueprint_plugin(&self) -> bool {
        self.0.is_blueprint_plugin()
    }

    #[napi]
    pub fn is_valid_as_light_plugin(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_valid_as_light_plugin()?)
    }

    #[napi]
    pub fn is_valid_as_medium_plugin(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_valid_as_medium_plugin()?)
    }

    #[napi]
    pub fn is_valid_as_update_plugin(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_valid_as_update_plugin()?)
    }

    #[napi]
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    #[napi]
    pub fn loads_archive(&self) -> bool {
        self.0.loads_archive()
    }

    #[napi]
    pub fn do_records_overlap(&self, plugin: &Plugin) -> Result<bool, VerboseError> {
        Ok(self.0.do_records_overlap(&plugin.0)?)
    }
}

impl From<Arc<libloot::Plugin>> for Plugin {
    fn from(value: Arc<libloot::Plugin>) -> Self {
        Self(value)
    }
}
