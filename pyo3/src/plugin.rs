use pyo3::{pyclass, pymethods};

use crate::VerboseError;

#[pyclass(eq, frozen)]
#[derive(Clone, Debug, Eq, PartialEq)]
#[repr(transparent)]
pub struct Plugin(libloot::Plugin);

#[pymethods]
impl Plugin {
    fn name(&self) -> &str {
        self.0.name()
    }

    fn header_version(&self) -> Option<f32> {
        self.0.header_version()
    }

    fn version(&self) -> Option<&str> {
        self.0.version()
    }

    fn masters(&self) -> Result<Vec<String>, VerboseError> {
        Ok(self.0.masters()?)
    }

    fn bash_tags(&self) -> &[String] {
        self.0.bash_tags()
    }

    fn crc(&self) -> Option<u32> {
        self.0.crc()
    }

    fn is_master(&self) -> bool {
        self.0.is_master()
    }

    fn is_light_plugin(&self) -> bool {
        self.0.is_light_plugin()
    }

    fn is_medium_plugin(&self) -> bool {
        self.0.is_medium_plugin()
    }

    fn is_update_plugin(&self) -> bool {
        self.0.is_update_plugin()
    }

    fn is_blueprint_plugin(&self) -> bool {
        self.0.is_blueprint_plugin()
    }

    fn is_valid_as_light_plugin(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_valid_as_light_plugin()?)
    }

    fn is_valid_as_medium_plugin(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_valid_as_medium_plugin()?)
    }

    fn is_valid_as_update_plugin(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_valid_as_update_plugin()?)
    }

    fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    fn loads_archive(&self) -> bool {
        self.0.loads_archive()
    }

    fn do_records_overlap(&self, plugin: &Self) -> Result<bool, VerboseError> {
        Ok(self.0.do_records_overlap(&plugin.0)?)
    }
}

impl Plugin {
    pub fn wrap(plugin: &libloot::Plugin) -> &Plugin {
        let p = plugin as *const libloot::Plugin;
        // SAFETY: Plugin is a transparent wrapper, so reinterpreting the pointer is safe.
        unsafe {
            let p = p as *const Plugin;
            &*p
        }
    }
}

pub fn wrap_plugins(mut vec: Vec<&libloot::Plugin>) -> Vec<&Plugin> {
    // SAFETY: This is safe because Plugin is a transparent wrapper around the libloot type.
    unsafe { Vec::from_raw_parts(vec.as_mut_ptr().cast(), vec.len(), vec.capacity()) }
}
