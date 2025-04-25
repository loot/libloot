use std::{
    path::Path,
    sync::{Arc, RwLock},
};

use delegate::delegate;
use libloot::{WriteMode, error::DatabaseLockPoisonError};
use libloot_ffi_errors::UnsupportedEnumValueError;

use crate::{
    OptionalPluginMetadata, VerboseError,
    ffi::EdgeType,
    metadata::{Group, Message, PluginMetadata, to_vec_of_unwrapped},
};

#[derive(Debug)]
#[repr(transparent)]
pub struct Database(Arc<RwLock<libloot::Database>>);

impl Database {
    pub fn new(db: Arc<RwLock<libloot::Database>>) -> Self {
        Self(db)
    }

    pub fn load_masterlist(&self, path: &str) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_masterlist(Path::new(path))
            .map_err(Into::into)
    }

    pub fn load_masterlist_with_prelude(
        &self,
        masterlist_path: &str,
        prelude_path: &str,
    ) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_masterlist_with_prelude(Path::new(masterlist_path), Path::new(prelude_path))
            .map_err(Into::into)
    }

    pub fn load_userlist(&self, path: &str) -> Result<(), VerboseError> {
        self.0
            .write()
            .map_err(DatabaseLockPoisonError::from)?
            .load_userlist(Path::new(path))
            .map_err(Into::into)
    }

    pub fn write_user_metadata(
        &self,
        output_path: &str,
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
            .write_user_metadata(Path::new(output_path), write_mode)
            .map_err(Into::into)
    }

    pub fn write_minimal_list(
        &self,
        output_path: &str,
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
            .write_minimal_list(Path::new(output_path), write_mode)
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

    // I tried returning a GroupRef<'_> here, but it borrows from the DB read guard, which the borrow checker sees as an owned value within this scope, so won't allow me to return a reference to it. This is the only place that uses a group reference, so it's probably not worth figuring out a workaround, and cloning the groups is fine.
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

    // This is ugly, but Group can't be held as a value in C++ and CXX doesn't support Vec<Box<Group>> as a parameter, so this can't take ownership of the input groups.
    pub fn set_user_groups(&self, groups: &[Box<Group>]) -> Result<(), VerboseError> {
        let groups = to_vec_of_unwrapped(groups);
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
    ) -> Result<Box<OptionalPluginMetadata>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .plugin_metadata(plugin_name, include_user_metadata, evaluate_conditions)
            .map(|p| Box::new(p.map(Into::into).into()))
            .map_err(Into::into)
    }

    pub fn plugin_user_metadata(
        &self,
        plugin_name: &str,
        evaluate_conditions: bool,
    ) -> Result<Box<OptionalPluginMetadata>, VerboseError> {
        self.0
            .read()
            .map_err(DatabaseLockPoisonError::from)?
            .plugin_user_metadata(plugin_name, evaluate_conditions)
            .map(|p| Box::new(p.map(Into::into).into()))
            .map_err(Into::into)
    }

    pub fn set_plugin_user_metadata(
        &mut self,
        plugin_metadata: Box<PluginMetadata>,
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

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Vertex(libloot::Vertex);

pub fn new_vertex(name: String) -> Box<Vertex> {
    Box::new(Vertex(libloot::Vertex::new(name)))
}

impl Vertex {
    // A value of 255 is used to indicate that there is no out edge.
    pub fn out_edge_type(&self) -> Result<u8, VerboseError> {
        match self.0.out_edge_type() {
            Some(e) => Ok(EdgeType::try_from(e)?.repr),
            None => Ok(u8::MAX),
        }
    }

    pub fn set_out_edge_type(&mut self, out_edge_type: EdgeType) -> Result<(), VerboseError> {
        self.0.set_out_edge_type(out_edge_type.try_into()?);
        Ok(())
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn name(&self) -> &str;
        }
    }
}

impl From<libloot::Vertex> for Vertex {
    fn from(value: libloot::Vertex) -> Self {
        Self(value)
    }
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
            _ => Err(UnsupportedEnumValueError),
        }
    }
}
