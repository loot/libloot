use std::path::Path;

use delegate::delegate;
use libloot_ffi_errors::UnsupportedEnumValueError;

use crate::{OptionalPlugin, Plugin, VerboseError, database::Database, ffi::GameType};

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

#[derive(Clone, Copy, Debug)]
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
        .map(str::to_owned)
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
        Box::new(self.0.plugin(plugin_name).map(Into::into).into())
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
        self.0
            .load_order()
            .iter()
            .map(ToString::to_string)
            .collect()
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
