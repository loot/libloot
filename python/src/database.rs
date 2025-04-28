use std::{
    hash::{DefaultHasher, Hash, Hasher},
    path::PathBuf,
    sync::{Arc, RwLock},
};

use libloot::{WriteMode, error::DatabaseLockPoisonError};
use libloot_ffi_errors::UnsupportedEnumValueError;
use pyo3::{
    Bound, PyResult, pyclass, pymethods,
    types::{PyAnyMethods, PyTypeMethods},
};

use crate::{
    error::VerboseError,
    metadata::{Group, Message, NONE_REPR, PluginMetadata},
};

#[pyclass]
#[derive(Clone, Debug)]
pub struct Database(Arc<RwLock<libloot::Database>>);

#[pymethods]
impl Database {
    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    pub fn load_masterlist(&self, path: PathBuf) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_masterlist(&path)
            .map_err(Into::into)
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    pub fn load_masterlist_with_prelude(
        &self,
        masterlist_path: PathBuf,
        prelude_path: PathBuf,
    ) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_masterlist_with_prelude(&masterlist_path, &prelude_path)
            .map_err(Into::into)
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    pub fn load_userlist(&self, path: PathBuf) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_userlist(&path)
            .map_err(Into::into)
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    pub fn write_user_metadata(
        &self,
        output_path: PathBuf,
        overwrite: bool,
    ) -> Result<(), VerboseError> {
        let write_mode = if overwrite {
            WriteMode::CreateOrTruncate
        } else {
            WriteMode::Create
        };

        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .write_user_metadata(&output_path, write_mode)
            .map_err(Into::into)
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    pub fn write_minimal_list(
        &self,
        output_path: PathBuf,
        overwrite: bool,
    ) -> Result<(), VerboseError> {
        let write_mode = if overwrite {
            WriteMode::CreateOrTruncate
        } else {
            WriteMode::Create
        };

        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .write_minimal_list(&output_path, write_mode)
            .map_err(Into::into)
    }

    pub fn evaluate(&self, condition: &str) -> Result<bool, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .evaluate(condition)
            .map_err(Into::into)
    }

    pub fn known_bash_tags(&self) -> Result<Vec<String>, VerboseError> {
        Ok(self
            .0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .known_bash_tags())
    }

    pub fn general_messages(
        &self,
        evaluate_conditions: bool,
    ) -> Result<Vec<Message>, VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .general_messages(evaluate_conditions)
            .map(|v| v.into_iter().map(Into::into).collect())
            .map_err(Into::into)
    }

    pub fn groups(&self, include_user_metadata: bool) -> Result<Vec<Group>, VerboseError> {
        Ok(self
            .0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .groups(include_user_metadata)
            .into_iter()
            .map(Into::into)
            .collect())
    }

    fn user_groups(&self) -> Result<Vec<Group>, VerboseError> {
        Ok(self
            .0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .user_groups()
            .iter()
            .cloned()
            .map(Into::into)
            .collect())
    }

    pub fn set_user_groups(&self, groups: Vec<Group>) -> Result<(), VerboseError> {
        let groups = groups.into_iter().map(Into::into).collect();
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .set_user_groups(groups);
        Ok(())
    }

    pub fn groups_path(
        &self,
        from_group_name: &str,
        to_group_name: &str,
    ) -> Result<Vec<Vertex>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .groups_path(from_group_name, to_group_name)
            .map(|v| v.into_iter().map(Into::into).collect())
            .map_err(Into::into)
    }

    pub fn plugin_metadata(
        &self,
        plugin_name: &str,
        include_user_metadata: bool,
        evaluate_conditions: bool,
    ) -> Result<Option<PluginMetadata>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .plugin_metadata(plugin_name, include_user_metadata, evaluate_conditions)
            .map(|p| p.map(Into::into))
            .map_err(Into::into)
    }

    pub fn plugin_user_metadata(
        &self,
        plugin_name: &str,
        evaluate_conditions: bool,
    ) -> Result<Option<PluginMetadata>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .plugin_user_metadata(plugin_name, evaluate_conditions)
            .map(|p| p.map(Into::into))
            .map_err(Into::into)
    }

    pub fn set_plugin_user_metadata(
        &mut self,
        plugin_metadata: PluginMetadata,
    ) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .set_plugin_user_metadata(plugin_metadata.into());
        Ok(())
    }

    pub fn discard_plugin_user_metadata(&self, plugin: &str) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .discard_plugin_user_metadata(plugin);
        Ok(())
    }

    pub fn discard_all_user_metadata(&self) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .discard_all_user_metadata();
        Ok(())
    }
}

impl From<Arc<RwLock<libloot::Database>>> for Database {
    fn from(value: Arc<RwLock<libloot::Database>>) -> Self {
        Self(value)
    }
}

#[pyclass(eq, ord, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct Vertex(libloot::Vertex);

#[pymethods]
impl Vertex {
    #[new]
    fn new(name: String) -> Self {
        Self(libloot::Vertex::new(name))
    }

    #[getter]
    fn name(&self) -> &str {
        self.0.name()
    }

    #[getter]
    fn out_edge_type(&self) -> Result<Option<EdgeType>, VerboseError> {
        self.0
            .out_edge_type()
            .map(|e| e.try_into().map_err(Into::into))
            .transpose()
    }

    #[setter]
    fn set_out_edge_type(&mut self, out_edge_type: EdgeType) -> Result<(), VerboseError> {
        let out_edge_type = out_edge_type.try_into()?;
        self.0.set_out_edge_type(out_edge_type);
        Ok(())
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!(
            "{}({}, {})",
            class_name,
            inner.name(),
            inner.out_edge_type().map_or(NONE_REPR, repr_edge_type),
        ))
    }

    fn __hash__(&self) -> u64 {
        let mut hasher = DefaultHasher::new();
        self.0.hash(&mut hasher);
        hasher.finish()
    }
}

impl From<libloot::Vertex> for Vertex {
    fn from(value: libloot::Vertex) -> Self {
        Self(value)
    }
}

impl From<Vertex> for libloot::Vertex {
    fn from(value: Vertex) -> Self {
        value.0
    }
}

#[pyclass(eq, frozen, hash, ord)]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum EdgeType {
    Hardcoded,
    MasterFlag,
    Master,
    MasterlistRequirement,
    UserRequirement,
    MasterlistLoadAfter,
    UserLoadAfter,
    MasterlistGroup,
    UserGroup,
    RecordOverlap,
    AssetOverlap,
    TieBreak,
    BlueprintMaster,
}

impl TryFrom<libloot::EdgeType> for EdgeType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: libloot::EdgeType) -> Result<Self, Self::Error> {
        match value {
            libloot::EdgeType::Hardcoded => Ok(EdgeType::Hardcoded),
            libloot::EdgeType::MasterFlag => Ok(EdgeType::MasterFlag),
            libloot::EdgeType::Master => Ok(EdgeType::Master),
            libloot::EdgeType::MasterlistRequirement => Ok(EdgeType::MasterlistRequirement),
            libloot::EdgeType::UserRequirement => Ok(EdgeType::UserRequirement),
            libloot::EdgeType::MasterlistLoadAfter => Ok(EdgeType::MasterlistLoadAfter),
            libloot::EdgeType::UserLoadAfter => Ok(EdgeType::UserLoadAfter),
            libloot::EdgeType::MasterlistGroup => Ok(EdgeType::MasterlistGroup),
            libloot::EdgeType::UserGroup => Ok(EdgeType::UserGroup),
            libloot::EdgeType::RecordOverlap => Ok(EdgeType::RecordOverlap),
            libloot::EdgeType::AssetOverlap => Ok(EdgeType::AssetOverlap),
            libloot::EdgeType::TieBreak => Ok(EdgeType::TieBreak),
            libloot::EdgeType::BlueprintMaster => Ok(EdgeType::BlueprintMaster),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

impl TryFrom<EdgeType> for libloot::EdgeType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: EdgeType) -> Result<Self, Self::Error> {
        match value {
            EdgeType::Hardcoded => Ok(libloot::EdgeType::Hardcoded),
            EdgeType::MasterFlag => Ok(libloot::EdgeType::MasterFlag),
            EdgeType::Master => Ok(libloot::EdgeType::Master),
            EdgeType::MasterlistRequirement => Ok(libloot::EdgeType::MasterlistRequirement),
            EdgeType::UserRequirement => Ok(libloot::EdgeType::UserRequirement),
            EdgeType::MasterlistLoadAfter => Ok(libloot::EdgeType::MasterlistLoadAfter),
            EdgeType::UserLoadAfter => Ok(libloot::EdgeType::UserLoadAfter),
            EdgeType::MasterlistGroup => Ok(libloot::EdgeType::MasterlistGroup),
            EdgeType::UserGroup => Ok(libloot::EdgeType::UserGroup),
            EdgeType::RecordOverlap => Ok(libloot::EdgeType::RecordOverlap),
            EdgeType::AssetOverlap => Ok(libloot::EdgeType::AssetOverlap),
            EdgeType::TieBreak => Ok(libloot::EdgeType::TieBreak),
            EdgeType::BlueprintMaster => Ok(libloot::EdgeType::BlueprintMaster),
        }
    }
}

fn repr_edge_type(value: libloot::EdgeType) -> &'static str {
    match value {
        libloot::EdgeType::Hardcoded => "EdgeType.Hardcoded",
        libloot::EdgeType::MasterFlag => "EdgeType.MasterFlag",
        libloot::EdgeType::Master => "EdgeType.Master",
        libloot::EdgeType::MasterlistRequirement => "EdgeType.MasterlistRequirement",
        libloot::EdgeType::UserRequirement => "EdgeType.UserRequirement",
        libloot::EdgeType::MasterlistLoadAfter => "EdgeType.MasterlistLoadAfter",
        libloot::EdgeType::UserLoadAfter => "EdgeType.UserLoadAfter",
        libloot::EdgeType::MasterlistGroup => "EdgeType.MasterlistGroup",
        libloot::EdgeType::UserGroup => "EdgeType.UserGroup",
        libloot::EdgeType::RecordOverlap => "EdgeType.RecordOverlap",
        libloot::EdgeType::AssetOverlap => "EdgeType.AssetOverlap",
        libloot::EdgeType::TieBreak => "EdgeType.TieBreak",
        libloot::EdgeType::BlueprintMaster => "EdgeType.BlueprintMaster",
        _ => "<unknown EdgeType>",
    }
}
