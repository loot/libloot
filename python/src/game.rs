use std::path::{Path, PathBuf};

use libloot_ffi_errors::UnsupportedEnumValueError;
use pyo3::{pyclass, pymethods};

use crate::{database::Database, error::VerboseError, plugin::Plugin};

#[pyclass(eq, frozen, hash, ord)]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
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
    OblivionRemastered,
}

impl TryFrom<libloot::GameType> for GameType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: libloot::GameType) -> Result<Self, Self::Error> {
        match value {
            libloot::GameType::Oblivion => Ok(GameType::Oblivion),
            libloot::GameType::Skyrim => Ok(GameType::Skyrim),
            libloot::GameType::Fallout3 => Ok(GameType::Fallout3),
            libloot::GameType::FalloutNV => Ok(GameType::FalloutNV),
            libloot::GameType::Fallout4 => Ok(GameType::Fallout4),
            libloot::GameType::SkyrimSE => Ok(GameType::SkyrimSE),
            libloot::GameType::Fallout4VR => Ok(GameType::Fallout4VR),
            libloot::GameType::SkyrimVR => Ok(GameType::SkyrimVR),
            libloot::GameType::Morrowind => Ok(GameType::Morrowind),
            libloot::GameType::Starfield => Ok(GameType::Starfield),
            libloot::GameType::OpenMW => Ok(GameType::OpenMW),
            libloot::GameType::OblivionRemastered => Ok(GameType::OblivionRemastered),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

impl TryFrom<GameType> for libloot::GameType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: GameType) -> Result<Self, Self::Error> {
        match value {
            GameType::Oblivion => Ok(libloot::GameType::Oblivion),
            GameType::Skyrim => Ok(libloot::GameType::Skyrim),
            GameType::Fallout3 => Ok(libloot::GameType::Fallout3),
            GameType::FalloutNV => Ok(libloot::GameType::FalloutNV),
            GameType::Fallout4 => Ok(libloot::GameType::Fallout4),
            GameType::SkyrimSE => Ok(libloot::GameType::SkyrimSE),
            GameType::Fallout4VR => Ok(libloot::GameType::Fallout4VR),
            GameType::SkyrimVR => Ok(libloot::GameType::SkyrimVR),
            GameType::Morrowind => Ok(libloot::GameType::Morrowind),
            GameType::Starfield => Ok(libloot::GameType::Starfield),
            GameType::OpenMW => Ok(libloot::GameType::OpenMW),
            GameType::OblivionRemastered => Ok(libloot::GameType::OblivionRemastered),
        }
    }
}

#[pyclass]
#[derive(Debug)]
pub struct Game(libloot::Game);

#[pymethods]
impl Game {
    #[new]
    #[pyo3(signature = (game_type, game_path, local_path = None))]
    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    fn new(
        game_type: GameType,
        game_path: PathBuf,
        local_path: Option<PathBuf>,
    ) -> Result<Self, VerboseError> {
        match local_path {
            Some(local_path) => Ok(Game(libloot::Game::with_local_path(
                game_type.try_into()?,
                &game_path,
                &local_path,
            )?)),
            None => Ok(Game(libloot::Game::new(game_type.try_into()?, &game_path)?)),
        }
    }

    fn game_type(&self) -> Result<GameType, VerboseError> {
        self.0.game_type().try_into().map_err(Into::into)
    }

    fn additional_data_paths(&self) -> &[PathBuf] {
        self.0.additional_data_paths()
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    fn set_additional_data_paths(&mut self, paths: Vec<PathBuf>) -> Result<(), VerboseError> {
        self.0.set_additional_data_paths(&as_paths(&paths))?;
        Ok(())
    }

    fn database(&self) -> Database {
        self.0.database().into()
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    fn is_valid_plugin(&self, plugin_path: PathBuf) -> bool {
        self.0.is_valid_plugin(&plugin_path)
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    fn load_plugins(&mut self, plugin_paths: Vec<PathBuf>) -> Result<(), VerboseError> {
        self.0.load_plugins(&as_paths(&plugin_paths))?;
        Ok(())
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    fn load_plugin_headers(&mut self, plugin_paths: Vec<PathBuf>) -> Result<(), VerboseError> {
        self.0.load_plugin_headers(&as_paths(&plugin_paths))?;
        Ok(())
    }

    fn clear_loaded_plugins(&mut self) {
        self.0.clear_loaded_plugins();
    }

    fn plugin(&self, plugin_name: &str) -> Option<Plugin> {
        self.0.plugin(plugin_name).map(Into::into)
    }

    fn loaded_plugins(&self) -> Vec<Plugin> {
        self.0
            .loaded_plugins()
            .into_iter()
            .map(Into::into)
            .collect()
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    fn sort_plugins(&self, plugin_names: Vec<String>) -> Result<Vec<String>, VerboseError> {
        Ok(self.0.sort_plugins(&as_strs(&plugin_names))?)
    }

    fn load_current_load_order_state(&mut self) -> Result<(), VerboseError> {
        self.0.load_current_load_order_state()?;
        Ok(())
    }

    fn is_load_order_ambiguous(&self) -> Result<bool, VerboseError> {
        Ok(self.0.is_load_order_ambiguous()?)
    }

    fn active_plugins_file_path(&self) -> &PathBuf {
        self.0.active_plugins_file_path()
    }

    fn is_plugin_active(&self, plugin_name: &str) -> bool {
        self.0.is_plugin_active(plugin_name)
    }

    fn load_order(&self) -> Vec<&str> {
        self.0.load_order()
    }

    #[expect(clippy::needless_pass_by_value, reason = "Required by PyO3")]
    fn set_load_order(&mut self, load_order: Vec<String>) -> Result<(), VerboseError> {
        self.0.set_load_order(&as_strs(&load_order))?;
        Ok(())
    }
}

fn as_paths(pathbufs: &[PathBuf]) -> Vec<&Path> {
    pathbufs.iter().map(PathBuf::as_ref).collect()
}

fn as_strs(strings: &[String]) -> Vec<&str> {
    strings.iter().map(String::as_ref).collect()
}
