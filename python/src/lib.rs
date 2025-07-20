mod database;
mod error;
mod game;
mod metadata;
mod plugin;

use database::{Database, EdgeType, Vertex};
use game::{Game, GameType};
use metadata::{
    File, Filename, Group, Location, Message, MessageContent, MessageType, PluginCleaningData,
    PluginMetadata, Tag, TagSuggestion, select_message_content,
};
use plugin::Plugin;
use pyo3::{create_exception, exceptions::PyException, prelude::*};

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

create_exception!(loot, CyclicInteractionError, PyException);
create_exception!(loot, UndefinedGroupError, PyException);
create_exception!(loot, PluginNotLoadedError, PyException);

/// A Python module implemented in Rust.
#[pymodule(name = "loot")]
fn libloot_pyo3(py: Python<'_>, m: &Bound<'_, PyModule>) -> PyResult<()> {
    pyo3_log::init();

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
    m.add_class::<PluginCleaningData>()?;
    m.add_class::<Tag>()?;
    m.add_class::<TagSuggestion>()?;
    m.add_class::<Location>()?;
    m.add_class::<PluginMetadata>()?;

    m.add(
        "CyclicInteractionError",
        py.get_type::<CyclicInteractionError>(),
    )?;
    m.add("UndefinedGroupError", py.get_type::<UndefinedGroupError>())?;
    m.add(
        "PluginNotLoadedError",
        py.get_type::<PluginNotLoadedError>(),
    )?;

    Ok(())
}
