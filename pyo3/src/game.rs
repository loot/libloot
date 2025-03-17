use std::path::{Path, PathBuf};

use pyo3::{pyclass, pymethods};

use crate::{
    database::Database,
    error::{UnsupportedEnumValueError, VerboseError},
    plugin::Plugin,
};

#[allow(non_camel_case_types)]
#[pyclass(eq, frozen, hash, ord)]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum GameType {
    tes4,
    tes5,
    fo3,
    fonv,
    fo4,
    tes5se,
    fo4vr,
    tes5vr,
    tes3,
    starfield,
    openmw,
}

impl TryFrom<libloot::GameType> for GameType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: libloot::GameType) -> Result<Self, Self::Error> {
        match value {
            libloot::GameType::TES4 => Ok(GameType::tes4),
            libloot::GameType::TES5 => Ok(GameType::tes5),
            libloot::GameType::FO3 => Ok(GameType::fo3),
            libloot::GameType::FONV => Ok(GameType::fonv),
            libloot::GameType::FO4 => Ok(GameType::fo4),
            libloot::GameType::TES5SE => Ok(GameType::tes5se),
            libloot::GameType::FO4VR => Ok(GameType::fo4vr),
            libloot::GameType::TES5VR => Ok(GameType::tes5vr),
            libloot::GameType::TES3 => Ok(GameType::tes3),
            libloot::GameType::Starfield => Ok(GameType::starfield),
            libloot::GameType::OpenMW => Ok(GameType::openmw),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

impl TryFrom<GameType> for libloot::GameType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: GameType) -> Result<Self, Self::Error> {
        match value {
            GameType::tes4 => Ok(libloot::GameType::TES4),
            GameType::tes5 => Ok(libloot::GameType::TES5),
            GameType::fo3 => Ok(libloot::GameType::FO3),
            GameType::fonv => Ok(libloot::GameType::FONV),
            GameType::fo4 => Ok(libloot::GameType::FO4),
            GameType::tes5se => Ok(libloot::GameType::TES5SE),
            GameType::fo4vr => Ok(libloot::GameType::FO4VR),
            GameType::tes5vr => Ok(libloot::GameType::TES5VR),
            GameType::tes3 => Ok(libloot::GameType::TES3),
            GameType::starfield => Ok(libloot::GameType::Starfield),
            GameType::openmw => Ok(libloot::GameType::OpenMW),
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

    fn set_additional_data_paths(&mut self, paths: Vec<PathBuf>) -> Result<(), VerboseError> {
        self.0.set_additional_data_paths(&as_paths(&paths))?;
        Ok(())
    }

    fn database(&self) -> Database {
        self.0.database().into()
    }

    fn is_valid_plugin(&self, plugin_path: PathBuf) -> bool {
        self.0.is_valid_plugin(&plugin_path)
    }

    fn load_plugins(&mut self, plugin_paths: Vec<PathBuf>) -> Result<(), VerboseError> {
        self.0.load_plugins(&as_paths(&plugin_paths))?;
        Ok(())
    }

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
