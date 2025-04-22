use std::path::Path;

use libloot_ffi_errors::UnsupportedEnumValueError;
use napi_derive::napi;

use crate::{database::Database, error::VerboseError, plugin::Plugin};

#[napi]
#[derive(Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum GameType {
    Oblivion,
    Skyrim,
    Fallout3,
    FalloutNV,
    Fallout4,
    SkyrimSE,
    Fallout4VR,
    SkyrimVR,
    Morrowind,
    Starfield,
    OpenMW,
}

impl TryFrom<libloot::GameType> for GameType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: libloot::GameType) -> Result<Self, Self::Error> {
        match value {
            libloot::GameType::TES4 => Ok(GameType::Oblivion),
            libloot::GameType::TES5 => Ok(GameType::Skyrim),
            libloot::GameType::FO3 => Ok(GameType::Fallout3),
            libloot::GameType::FONV => Ok(GameType::FalloutNV),
            libloot::GameType::FO4 => Ok(GameType::Fallout4),
            libloot::GameType::TES5SE => Ok(GameType::SkyrimSE),
            libloot::GameType::FO4VR => Ok(GameType::Fallout4VR),
            libloot::GameType::TES5VR => Ok(GameType::SkyrimVR),
            libloot::GameType::TES3 => Ok(GameType::Morrowind),
            libloot::GameType::Starfield => Ok(GameType::Starfield),
            libloot::GameType::OpenMW => Ok(GameType::OpenMW),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

impl From<GameType> for libloot::GameType {
    fn from(value: GameType) -> Self {
        match value {
            GameType::Oblivion => libloot::GameType::TES4,
            GameType::Skyrim => libloot::GameType::TES5,
            GameType::Fallout3 => libloot::GameType::FO3,
            GameType::FalloutNV => libloot::GameType::FONV,
            GameType::Fallout4 => libloot::GameType::FO4,
            GameType::SkyrimSE => libloot::GameType::TES5SE,
            GameType::Fallout4VR => libloot::GameType::FO4VR,
            GameType::SkyrimVR => libloot::GameType::TES5VR,
            GameType::Morrowind => libloot::GameType::TES3,
            GameType::Starfield => libloot::GameType::Starfield,
            GameType::OpenMW => libloot::GameType::OpenMW,
        }
    }
}

#[napi]
#[derive(Debug)]
pub struct Game(libloot::Game);

#[napi]
impl Game {
    #[napi(constructor)]
    pub fn new(
        game_type: GameType,
        game_path: String,
        local_path: Option<String>,
    ) -> Result<Self, VerboseError> {
        match local_path {
            Some(local_path) => Ok(Game(libloot::Game::with_local_path(
                game_type.into(),
                Path::new(&game_path),
                Path::new(&local_path),
            )?)),
            None => Ok(Game(libloot::Game::new(
                game_type.into(),
                Path::new(&game_path),
            )?)),
        }
    }

    #[napi]
    pub fn game_type(&self) -> Result<GameType, VerboseError> {
        self.0.game_type().try_into().map_err(Into::into)
    }

    #[napi]
    pub fn additional_data_paths(&self) -> Vec<String> {
        self.0
            .additional_data_paths()
            .iter()
            .map(|p| p.to_string_lossy().to_string())
            .collect()
    }

    #[napi]
    pub fn set_additional_data_paths(&mut self, paths: Vec<String>) -> Result<(), VerboseError> {
        self.0.set_additional_data_paths(&as_paths(&paths))?;
        Ok(())
    }

    #[napi]
    pub fn database(&self) -> Database {
        self.0.database().into()
    }

    #[napi]
    pub fn is_valid_plugin(&self, plugin_path: String) -> bool {
        self.0.is_valid_plugin(Path::new(&plugin_path))
    }

    #[napi]
    pub fn load_plugins(&mut self, plugin_paths: Vec<String>) -> Result<(), VerboseError> {
        self.0.load_plugins(&as_paths(&plugin_paths))?;
        Ok(())
    }

    #[napi]
    pub fn load_plugin_headers(&mut self, plugin_paths: Vec<String>) -> Result<(), VerboseError> {
        self.0.load_plugin_headers(&as_paths(&plugin_paths))?;
        Ok(())
    }

    #[napi]
    pub fn clear_loaded_plugins(&mut self) {
        self.0.clear_loaded_plugins();
    }

    #[napi]
    pub fn plugin(&self, plugin_name: String) -> Option<Plugin> {
        self.0.plugin(&plugin_name).map(Into::into)
    }

    #[napi]
    pub fn loaded_plugins(&self) -> Vec<Plugin> {
        self.0
            .loaded_plugins()
            .into_iter()
            .map(Into::into)
            .collect()
    }

    #[napi]
    pub fn sort_plugins(&self, plugin_names: Vec<String>) -> Result<Vec<String>, VerboseError> {
        Ok(self.0.sort_plugins(&as_strs(&plugin_names))?)
    }

    #[napi]
    pub fn load_current_load_order_state(&mut self) -> Result<(), VerboseError> {
        self.0.load_current_load_order_state()?;
        Ok(())
    }

    #[napi]
    pub fn is_load_order_ambiguous(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_load_order_ambiguous()?)
    }

    #[napi]
    pub fn active_plugins_file_path(&self) -> String {
        self.0
            .active_plugins_file_path()
            .to_string_lossy()
            .to_string()
    }

    #[napi]
    pub fn is_plugin_active(&self, plugin_name: String) -> bool {
        self.0.is_plugin_active(&plugin_name)
    }

    #[napi]
    pub fn load_order(&self) -> Vec<&str> {
        self.0.load_order()
    }

    #[napi]
    pub fn set_load_order(&mut self, load_order: Vec<String>) -> Result<(), VerboseError> {
        self.0.set_load_order(&as_strs(&load_order))?;
        Ok(())
    }
}

fn as_paths(pathbufs: &[String]) -> Vec<&Path> {
    pathbufs.iter().map(Path::new).collect()
}

fn as_strs(strings: &[String]) -> Vec<&str> {
    strings.iter().map(String::as_ref).collect()
}
