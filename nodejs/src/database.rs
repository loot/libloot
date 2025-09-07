use std::{
    path::Path,
    sync::{Arc, RwLock},
};

use libloot::{WriteMode, error::DatabaseLockPoisonError};
use libloot_ffi_errors::UnsupportedEnumValueError;
use napi_derive::napi;

use crate::{
    error::VerboseError,
    metadata::{Group, Message, PluginMetadata},
};

#[napi]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum EvalMode {
    DoNotEvaluate,
    Evaluate,
}

impl From<EvalMode> for libloot::EvalMode {
    fn from(value: EvalMode) -> Self {
        match value {
            EvalMode::DoNotEvaluate => libloot::EvalMode::DoNotEvaluate,
            EvalMode::Evaluate => libloot::EvalMode::Evaluate,
        }
    }
}

#[napi]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum MergeMode {
    WithoutUserMetadata,
    WithUserMetadata,
}

impl From<MergeMode> for libloot::MergeMode {
    fn from(value: MergeMode) -> Self {
        match value {
            MergeMode::WithoutUserMetadata => libloot::MergeMode::WithoutUserMetadata,
            MergeMode::WithUserMetadata => libloot::MergeMode::WithUserMetadata,
        }
    }
}

#[napi]
#[derive(Clone, Debug)]
pub struct Database(Arc<RwLock<libloot::Database>>);

#[napi]
impl Database {
    #[napi]
    pub fn load_masterlist(&self, path: String) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_masterlist(Path::new(&path))
            .map_err(Into::into)
    }

    #[napi]
    pub fn load_masterlist_with_prelude(
        &self,
        masterlist_path: String,
        prelude_path: String,
    ) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_masterlist_with_prelude(Path::new(&masterlist_path), Path::new(&prelude_path))
            .map_err(Into::into)
    }

    #[napi]
    pub fn load_userlist(&self, path: String) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_userlist(Path::new(&path))
            .map_err(Into::into)
    }

    #[napi]
    pub fn write_user_metadata(
        &self,
        output_path: String,
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
            .write_user_metadata(Path::new(&output_path), write_mode)
            .map_err(Into::into)
    }

    #[napi]
    pub fn write_minimal_list(
        &self,
        output_path: String,
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
            .write_minimal_list(Path::new(&output_path), write_mode)
            .map_err(Into::into)
    }

    #[napi]
    pub fn evaluate(&self, condition: String) -> Result<bool, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .evaluate(&condition)
            .map_err(Into::into)
    }

    #[napi]
    pub fn clear_condition_cache(&self) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .clear_condition_cache();

        Ok(())
    }

    #[napi]
    pub fn known_bash_tags(&self) -> Result<Vec<String>, VerboseError> {
        Ok(self
            .0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .known_bash_tags())
    }

    #[napi]
    pub fn general_messages(
        &self,
        evaluate_conditions: EvalMode,
    ) -> Result<Vec<Message>, VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .general_messages(evaluate_conditions.into())
            .map(|v| v.into_iter().map(Into::into).collect())
            .map_err(Into::into)
    }

    #[napi]
    pub fn groups(&self, include_user_metadata: MergeMode) -> Result<Vec<Group>, VerboseError> {
        Ok(self
            .0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .groups(include_user_metadata.into())
            .into_iter()
            .map(Into::into)
            .collect())
    }

    #[napi]
    pub fn user_groups(&self) -> Result<Vec<Group>, VerboseError> {
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

    #[napi]
    pub fn set_user_groups(&self, groups: Vec<&Group>) -> Result<(), VerboseError> {
        let groups = groups.into_iter().cloned().map(Into::into).collect();
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .set_user_groups(groups);
        Ok(())
    }

    #[napi]
    pub fn groups_path(
        &self,
        from_group_name: String,
        to_group_name: String,
    ) -> Result<Vec<Vertex>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .groups_path(&from_group_name, &to_group_name)
            .map(|v| v.into_iter().map(Into::into).collect())
            .map_err(Into::into)
    }

    #[napi]
    pub fn plugin_metadata(
        &self,
        plugin_name: String,
        include_user_metadata: MergeMode,
        evaluate_conditions: EvalMode,
    ) -> Result<Option<PluginMetadata>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .plugin_metadata(
                &plugin_name,
                include_user_metadata.into(),
                evaluate_conditions.into(),
            )
            .map(|p| p.map(Into::into))
            .map_err(Into::into)
    }

    #[napi]
    pub fn plugin_user_metadata(
        &self,
        plugin_name: String,
        evaluate_conditions: EvalMode,
    ) -> Result<Option<PluginMetadata>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .plugin_user_metadata(&plugin_name, evaluate_conditions.into())
            .map(|p| p.map(Into::into))
            .map_err(Into::into)
    }

    #[napi]
    pub fn set_plugin_user_metadata(
        &mut self,
        plugin_metadata: &PluginMetadata,
    ) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .set_plugin_user_metadata(plugin_metadata.clone().into());
        Ok(())
    }

    #[napi]
    pub fn discard_plugin_user_metadata(&self, plugin: String) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .discard_plugin_user_metadata(&plugin);
        Ok(())
    }

    #[napi]
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

#[napi]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct Vertex(libloot::Vertex);

#[napi]
impl Vertex {
    #[napi(constructor)]
    pub fn new(name: String, out_edge_type: Option<EdgeType>) -> Self {
        let mut vertex = libloot::Vertex::new(name);

        if let Some(out_edge_type) = out_edge_type {
            vertex = vertex.with_out_edge_type(out_edge_type.into());
        }

        Self(vertex)
    }

    #[napi(getter)]
    pub fn name(&self) -> &str {
        self.0.name()
    }

    #[napi(getter)]
    pub fn out_edge_type(&self) -> Result<Option<EdgeType>, VerboseError> {
        self.0
            .out_edge_type()
            .map(|e| e.try_into().map_err(Into::into))
            .transpose()
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

#[napi]
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

impl From<EdgeType> for libloot::EdgeType {
    fn from(value: EdgeType) -> Self {
        match value {
            EdgeType::Hardcoded => libloot::EdgeType::Hardcoded,
            EdgeType::MasterFlag => libloot::EdgeType::MasterFlag,
            EdgeType::Master => libloot::EdgeType::Master,
            EdgeType::MasterlistRequirement => libloot::EdgeType::MasterlistRequirement,
            EdgeType::UserRequirement => libloot::EdgeType::UserRequirement,
            EdgeType::MasterlistLoadAfter => libloot::EdgeType::MasterlistLoadAfter,
            EdgeType::UserLoadAfter => libloot::EdgeType::UserLoadAfter,
            EdgeType::MasterlistGroup => libloot::EdgeType::MasterlistGroup,
            EdgeType::UserGroup => libloot::EdgeType::UserGroup,
            EdgeType::RecordOverlap => libloot::EdgeType::RecordOverlap,
            EdgeType::AssetOverlap => libloot::EdgeType::AssetOverlap,
            EdgeType::TieBreak => libloot::EdgeType::TieBreak,
            EdgeType::BlueprintMaster => libloot::EdgeType::BlueprintMaster,
        }
    }
}
