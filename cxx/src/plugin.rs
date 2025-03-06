use delegate::delegate;

use crate::{EmptyOptionalError, OptionalRef, VerboseError};

#[derive(Debug)]
#[repr(transparent)]
pub struct PluginRef<'a>(&'a libloot::Plugin);

impl<'a> PluginRef<'a> {
    pub fn new(plugin: &'a libloot::Plugin) -> Self {
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

    pub fn crc(&self) -> i64 {
        self.0.crc().map(Into::into).unwrap_or(-1)
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
        self.0.do_records_overlap(plugin.0).map_err(Into::into)
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

impl<'a> From<&'a libloot::Plugin> for PluginRef<'a> {
    fn from(value: &'a libloot::Plugin) -> Self {
        PluginRef(value)
    }
}

pub type OptionalPluginRef = OptionalRef<libloot::Plugin>;

impl OptionalRef<libloot::Plugin> {
    /// # Safety
    ///
    /// This is safe as long as the pointer in the OptionalRef is still valid.
    pub unsafe fn as_ref(&self) -> Result<Box<PluginRef<'_>>, EmptyOptionalError> {
        if self.0.is_null() {
            Err(EmptyOptionalError)
        } else {
            unsafe { Ok(Box::new(PluginRef::new(&*self.0))) }
        }
    }
}
