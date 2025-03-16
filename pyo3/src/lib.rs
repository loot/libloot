mod database;
mod game;
mod metadata;
mod plugin;

use database::{Database, EdgeType, Vertex};
use game::{Game, GameType};
use metadata::{
    File, Filename, Group, Location, Message, MessageContent, MessageType, PluginMetadata, Tag,
    TagSuggestion, select_message_content,
};
use plugin::Plugin;
use pyo3::{exceptions::PyValueError, prelude::*};

#[pyfunction]
fn is_compatible(major: u32, minor: u32, patch: u32) -> bool {
    libloot::is_compatible(major, minor, patch)
}

#[pyfunction]
fn libloot_revision() -> String {
    libloot::libloot_revision()
}

#[pyfunction]
fn libloot_version() -> String {
    libloot::libloot_version()
}

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
pub struct VerboseError(Box<dyn std::error::Error>);

impl std::fmt::Display for VerboseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let e = &self.0;
        write!(f, "{}", e)?;
        let mut error = e.as_ref();
        while let Some(source) = error.source() {
            write!(f, ": {}", source)?;
            error = source;
        }
        Ok(())
    }
}

// impl std::error::Error for VerboseError {}

impl<T: std::error::Error + 'static> From<T> for VerboseError {
    fn from(value: T) -> Self {
        VerboseError(Box::new(value))
    }
}

impl From<VerboseError> for PyErr {
    fn from(value: VerboseError) -> Self {
        PyValueError::new_err(value.to_string())
    }
}

/// A Python module implemented in Rust.
#[pymodule(name = "loot")]
fn libloot_pyo3(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add("LIBLOOT_VERSION_MAJOR", libloot::LIBLOOT_VERSION_MAJOR)?;
    m.add("LIBLOOT_VERSION_MINOR", libloot::LIBLOOT_VERSION_MINOR)?;
    m.add("LIBLOOT_VERSION_PATCH", libloot::LIBLOOT_VERSION_PATCH)?;

    m.add_function(wrap_pyfunction!(is_compatible, m)?)?;
    m.add_function(wrap_pyfunction!(libloot_revision, m)?)?;
    m.add_function(wrap_pyfunction!(libloot_version, m)?)?;

    m.add_function(wrap_pyfunction!(select_message_content, m)?)?;

    m.add_class::<Vertex>()?;
    m.add_class::<EdgeType>()?;
    m.add_class::<GameType>()?;
    m.add_class::<Game>()?;
    m.add_class::<Database>()?;
    m.add_class::<Plugin>()?;
    m.add_class::<Group>()?;
    m.add_class::<MessageContent>()?;
    m.add_class::<MessageType>()?;
    m.add_class::<Message>()?;
    m.add_class::<File>()?;
    m.add_class::<Filename>()?;
    m.add_class::<Tag>()?;
    m.add_class::<TagSuggestion>()?;
    m.add_class::<Location>()?;
    m.add_class::<PluginMetadata>()?;

    Ok(())
}
