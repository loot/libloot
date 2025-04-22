use std::path::Path;

use delegate::delegate;
use libloot_ffi_errors::UnsupportedEnumValueError;

use crate::{Plugin, VerboseError, database::Database, ffi::GameType, plugin::OptionalPlugin};

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
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

impl From<Game> for libloot::Game {
    fn from(value: Game) -> Self {
        value.0
    }
}

impl From<libloot::Game> for Game {
    fn from(value: libloot::Game) -> Self {
        Game(value)
    }
}

#[derive(Debug)]
pub struct NotValidUtf8;

impl std::fmt::Display for NotValidUtf8 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Value was not valid UTF-8")
    }
}

impl std::error::Error for NotValidUtf8 {}

#[derive(Debug)]
#[repr(transparent)]
pub struct Game(libloot::Game);

// CXX doesn't support &Path so use &str instead.
pub fn new_game(game_type: GameType, game_path: &str) -> Result<Box<Game>, VerboseError> {
    libloot::Game::new(game_type.try_into()?, Path::new(game_path))
        .map(|g| Box::new(g.into()))
        .map_err(Into::into)
}

pub fn new_game_with_local_path(
    game_type: GameType,
    game_path: &str,
    game_local_path: &str,
) -> Result<Box<Game>, VerboseError> {
    libloot::Game::with_local_path(
        game_type.try_into()?,
        Path::new(game_path),
        Path::new(game_local_path),
    )
    .map(|g| Box::new(g.into()))
    .map_err(Into::into)
}

fn path_to_string(path: &Path) -> Result<String, VerboseError> {
    path.to_str()
        .map(|s| s.to_owned())
        .ok_or(NotValidUtf8)
        .map_err(Into::into)
}

fn strings_to_paths<'a>(paths: &'a [&'a str]) -> Vec<&'a Path> {
    paths.iter().map(Path::new).collect()
}

impl Game {
    pub fn game_type(&self) -> Result<GameType, VerboseError> {
        self.0.game_type().try_into().map_err(Into::into)
    }

    pub fn additional_data_paths(&self) -> Result<Vec<String>, VerboseError> {
        self.0
            .additional_data_paths()
            .iter()
            .map(|p| path_to_string(p))
            .collect()
    }

    pub fn set_additional_data_paths(
        &mut self,
        additional_data_paths: &[&str],
    ) -> Result<(), VerboseError> {
        self.0
            .set_additional_data_paths(&strings_to_paths(additional_data_paths))
            .map_err(Into::into)
    }

    pub fn database(&self) -> Box<Database> {
        Box::new(Database::new(self.0.database()))
    }

    pub fn is_valid_plugin(&self, plugin_path: &str) -> bool {
        self.0.is_valid_plugin(Path::new(plugin_path))
    }

    pub fn load_plugins(&mut self, plugin_paths: &[&str]) -> Result<(), VerboseError> {
        self.0
            .load_plugins(&strings_to_paths(plugin_paths))
            .map_err(Into::into)
    }

    pub fn load_plugin_headers(&mut self, plugin_paths: &[&str]) -> Result<(), VerboseError> {
        self.0
            .load_plugin_headers(&strings_to_paths(plugin_paths))
            .map_err(Into::into)
    }

    pub fn plugin(&self, plugin_name: &str) -> Box<OptionalPlugin> {
        Box::new(self.0.plugin(plugin_name).into())
    }

    pub fn loaded_plugins(&self) -> Vec<Plugin> {
        self.0
            .loaded_plugins()
            .into_iter()
            .map(Into::into)
            .collect()
    }

    pub fn sort_plugins(&self, plugin_names: &[&str]) -> Result<Vec<String>, VerboseError> {
        self.0.sort_plugins(plugin_names).map_err(Into::into)
    }

    pub fn load_current_load_order_state(&mut self) -> Result<(), VerboseError> {
        self.0.load_current_load_order_state().map_err(Into::into)
    }

    pub fn is_load_order_ambiguous(&self) -> Result<bool, VerboseError> {
        self.0.is_load_order_ambiguous().map_err(Into::into)
    }

    pub fn active_plugins_file_path(&self) -> Result<String, VerboseError> {
        path_to_string(self.0.active_plugins_file_path())
    }

    pub fn load_order(&self) -> Vec<String> {
        self.0.load_order().iter().map(|s| s.to_string()).collect()
    }

    pub fn set_load_order(&mut self, load_order: &[&str]) -> Result<(), VerboseError> {
        self.0.set_load_order(load_order).map_err(Into::into)
    }

    delegate! {
        to self.0 {
            pub fn clear_loaded_plugins(&mut self);

            pub fn is_plugin_active(&self, plugin_name: &str) -> bool;
        }
    }
}
