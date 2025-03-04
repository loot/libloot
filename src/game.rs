use std::{
    collections::{HashMap, HashSet},
    fmt::Display,
    path::{Path, PathBuf},
    sync::{Arc, RwLock},
};

use loadorder::WritableLoadOrder;
use rayon::iter::{IntoParallelRefIterator, ParallelIterator};

use crate::{
    database::Database,
    error::{
        DatabaseLockPoisonError, GameHandleCreationError, LoadOrderError, LoadOrderStateError,
        LoadPluginsError, SortPluginsError,
    },
    metadata::{
        Filename,
        plugin_metadata::{GHOST_FILE_EXTENSION, iends_with_ascii},
    },
    plugin::error::{InvalidFilenameReason, PluginValidationError},
    plugin::{LoadScope, Plugin, plugins_metadata, validate_plugin_path_and_header},
    sorting::{
        groups::build_groups_graph,
        plugins::{PluginSortingData, sort_plugins},
    },
};

/// Codes used to create database handles for specific games.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[non_exhaustive]
pub enum GameType {
    /// The Elder Scrolls IV: Oblivion
    TES4,
    /// The Elder Scrolls V: Skyrim
    TES5,
    /// Fallout 3
    FO3,
    /// Fallout: New Vegas
    FONV,
    /// Fallout 4
    FO4,
    /// The Elder Scrolls V: Skyrim Special Edition
    TES5SE,
    /// Fallout 4 VR
    FO4VR,
    /// Skyrim VR
    TES5VR,
    /// The Elder Scrolls III: Morrowind
    TES3,
    /// Starfield
    Starfield,
    /// OpenMW
    OpenMW,
}

impl Display for GameType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            GameType::TES4 => write!(f, "The Elder Scrolls IV: Oblivion"),
            GameType::TES5 => write!(f, "The Elder Scrolls V: Skyrim"),
            GameType::FO3 => write!(f, "Fallout 3"),
            GameType::FONV => write!(f, "Fallout: New Vegas"),
            GameType::FO4 => write!(f, "Fallout 4"),
            GameType::TES5SE => write!(f, "The Elder Scrolls V: Skyrim Special Edition"),
            GameType::FO4VR => write!(f, "Fallout 4 VR"),
            GameType::TES5VR => write!(f, "The Elder Scrolls V: Skyrim VR"),
            GameType::TES3 => write!(f, "The Elder Scrolls III: Morrowind"),
            GameType::Starfield => write!(f, "Starfield"),
            GameType::OpenMW => write!(f, "OpenMW"),
        }
    }
}

impl From<GameType> for loadorder::GameId {
    fn from(value: GameType) -> Self {
        match value {
            GameType::TES4 => loadorder::GameId::Oblivion,
            GameType::TES5 => loadorder::GameId::Skyrim,
            GameType::FO3 => loadorder::GameId::Fallout3,
            GameType::FONV => loadorder::GameId::FalloutNV,
            GameType::FO4 => loadorder::GameId::Fallout4,
            GameType::TES5SE => loadorder::GameId::SkyrimSE,
            GameType::FO4VR => loadorder::GameId::Fallout4VR,
            GameType::TES5VR => loadorder::GameId::SkyrimVR,
            GameType::TES3 => loadorder::GameId::Morrowind,
            GameType::Starfield => loadorder::GameId::Starfield,
            GameType::OpenMW => loadorder::GameId::OpenMW,
        }
    }
}

impl From<GameType> for loot_condition_interpreter::GameType {
    fn from(value: GameType) -> Self {
        match value {
            GameType::TES4 => loot_condition_interpreter::GameType::Oblivion,
            GameType::TES5 => loot_condition_interpreter::GameType::Skyrim,
            GameType::FO3 => loot_condition_interpreter::GameType::Fallout3,
            GameType::FONV => loot_condition_interpreter::GameType::FalloutNV,
            GameType::FO4 => loot_condition_interpreter::GameType::Fallout4,
            GameType::TES5SE => loot_condition_interpreter::GameType::SkyrimSE,
            GameType::FO4VR => loot_condition_interpreter::GameType::Fallout4VR,
            GameType::TES5VR => loot_condition_interpreter::GameType::SkyrimVR,
            GameType::TES3 => loot_condition_interpreter::GameType::Morrowind,
            GameType::Starfield => loot_condition_interpreter::GameType::Starfield,
            GameType::OpenMW => loot_condition_interpreter::GameType::OpenMW,
        }
    }
}

impl From<GameType> for esplugin::GameId {
    fn from(value: GameType) -> Self {
        match value {
            GameType::TES4 => esplugin::GameId::Oblivion,
            GameType::TES5 => esplugin::GameId::Skyrim,
            GameType::FO3 => esplugin::GameId::Fallout3,
            GameType::FONV => esplugin::GameId::FalloutNV,
            GameType::FO4 | GameType::FO4VR => esplugin::GameId::Fallout4,
            GameType::TES5SE | GameType::TES5VR => esplugin::GameId::SkyrimSE,
            GameType::TES3 | GameType::OpenMW => esplugin::GameId::Morrowind,
            GameType::Starfield => esplugin::GameId::Starfield,
        }
    }
}

/// The interface through which game-specific functionality is provided.
#[derive(Debug)]
pub struct Game {
    game_type: GameType,
    game_path: PathBuf,
    load_order: Box<(dyn WritableLoadOrder + Send + Sync + 'static)>,
    // Stored in an Arc<RwLock<_>> to support loading metadata in parallel with
    // loading plugins.
    database: Arc<RwLock<Database>>,
    cache: GameCache,
}

impl Game {
    /// Initialise a new game handle, which is then used by all game-specific
    /// functions.
    ///
    /// - `game_type` is a value representing which game to create the handle
    ///   for,
    /// - `game_path` is the relative or absolute path to the directory
    ///   containing the game's executable.
    ///
    /// This function will attempt to look up the game's local data path, which
    /// may fail in some situations (e.g. when running libloot natively on Linux
    /// for a game other than Morrowind or OpenMW). [Game::with_local_path]
    /// can be used to provide the local path instead.
    pub fn new(game_type: GameType, game_path: &Path) -> Result<Self, GameHandleCreationError> {
        log::info!(
            "Attempting to create a game handle for game type {} with game path {:?}",
            game_type,
            game_path
        );

        let resolved_game_path = resolve_path(game_path);
        if !resolved_game_path.is_dir() {
            return Err(GameHandleCreationError::NotADirectory(game_path.into()));
        }

        let load_order =
            loadorder::GameSettings::new(game_type.into(), &resolved_game_path)?.into_load_order();

        let condition_evaluator_state =
            new_condition_evaluator_state(game_type, &resolved_game_path, load_order.as_ref());

        Ok(Game {
            game_type,
            game_path: resolved_game_path,
            load_order,
            database: Arc::new(RwLock::new(Database::new(condition_evaluator_state))),
            cache: GameCache::default(),
        })
    }

    /// Initialise a new game handle, which is then used by all game-specific
    /// functions.
    ///
    /// - `game_type` is a value representing which game to create the handle
    ///   for,
    /// - `game_path` is the relative or absolute path to the directory
    ///   containing the game's executable.
    /// - `game_local_path` is the relative or absolute path to the game's local
    ///   data path.  The local data folder is usually in `%%LOCALAPPDATA%`, but
    ///   Morrowind has no local data folder and OpenMW's is in the user's My
    ///   Games folder on Windows and in `$HOME/.config` on Linux.
    pub fn with_local_path(
        game_type: GameType,
        game_path: &Path,
        game_local_path: &Path,
    ) -> Result<Self, GameHandleCreationError> {
        log::info!(
            "Attempting to create a game handle for game type {} with game path {:?} and game local path {:?}",
            game_type,
            game_path,
            game_local_path
        );

        let resolved_game_path = resolve_path(game_path);
        if !resolved_game_path.is_dir() {
            return Err(GameHandleCreationError::NotADirectory(game_path.into()));
        }

        let resolved_game_local_path = resolve_path(game_local_path);
        if resolved_game_local_path.exists() && !resolved_game_local_path.is_dir() {
            return Err(GameHandleCreationError::NotADirectory(
                game_local_path.into(),
            ));
        }

        let load_order = loadorder::GameSettings::with_local_path(
            game_type.into(),
            &resolved_game_path,
            &resolved_game_local_path,
        )?
        .into_load_order();

        let condition_evaluator_state =
            new_condition_evaluator_state(game_type, &resolved_game_path, load_order.as_ref());

        Ok(Game {
            game_type,
            game_path: resolved_game_path,
            load_order,
            database: Arc::new(RwLock::new(Database::new(condition_evaluator_state))),
            cache: GameCache::default(),
        })
    }

    /// Get the game's type.
    pub fn game_type(&self) -> GameType {
        self.game_type
    }

    /// Gets the currently-set additional data paths.
    ///
    /// The following games are configured with additional data paths by
    /// default:
    ///
    /// - Fallout 4, when installed from the Microsoft Store
    /// - Starfield
    /// - OpenMW
    pub fn additional_data_paths(&self) -> &[PathBuf] {
        self.load_order
            .game_settings()
            .additional_plugins_directories()
    }

    /// Set additional data paths.
    ///
    /// The additional data paths are used when interacting with the load order,
    /// evaluating conditions and scanning for archives (BSA/BA2 depending on
    /// the game). Additional data paths are used in the order they are given
    /// (except with OpenMW, which checks them in reverse order), and take
    /// precedence over the game's main data path.
    ///
    /// Setting additional data paths clears the condition cache in this game's
    /// database object.
    pub fn set_additional_data_paths(
        &mut self,
        additional_data_paths: &[&Path],
    ) -> Result<(), DatabaseLockPoisonError> {
        let paths: Vec<_> = additional_data_paths
            .iter()
            .map(|p| p.to_path_buf())
            .collect();

        let mut database = self.database.write()?;
        database.clear_condition_cache();

        self.load_order
            .game_settings_mut()
            .set_additional_plugins_directories(paths.clone());

        database
            .condition_evaluator_state_mut()
            .set_additional_data_paths(paths);

        Ok(())
    }

    /// Get the object used for accessing metadata-related functionality.
    pub fn database(&self) -> Arc<RwLock<Database>> {
        Arc::clone(&self.database)
    }

    /// Check if a file is a valid plugin.
    ///
    /// The validity check is not exhaustive: it generally checks that the
    /// file is a valid plugin file extension for the game and that its header
    /// (if applicable) can be parsed.
    ///
    /// `plugin_path` can be absolute or relative: relative paths are resolved
    /// relative to the game's plugins directory, while absolute paths are used
    /// as given.
    pub fn is_valid_plugin(&self, plugin_path: &Path) -> bool {
        validate_plugin_path_and_header(self.game_type, plugin_path).is_ok()
    }

    /// Fully parses plugins and loads their data.
    ///
    /// If a given plugin filename (or one that is case-insensitively equal) has
    /// already been loaded, its previously-loaded data data is discarded.
    ///
    /// If the game is Morrowind, OpenMW or Starfield, it's only valid to fully
    /// load a plugin if its masters are already loaded or included in the same
    /// input slice.
    ///
    /// Relative paths in `plugin_paths` are resolved relative to the game's
    /// plugins directory, while absolute paths are used as given. Each plugin
    /// filename must be unique within the vector.
    ///
    /// Loading plugins clears the condition cache in this game's database
    /// object.
    pub fn load_plugins(&mut self, plugin_paths: &[&Path]) -> Result<(), LoadPluginsError> {
        let mut plugins = self.load_plugins_common(plugin_paths, LoadScope::WholePlugin)?;

        if matches!(
            self.game_type,
            GameType::TES3 | GameType::OpenMW | GameType::Starfield
        ) {
            let plugins_metadata = plugins_metadata(&plugins)?;

            for plugin in &mut plugins {
                plugin.resolve_record_ids(&plugins_metadata)?;
            }
        }

        self.store_plugins(plugins)?;

        Ok(())
    }

    /// Parses plugin headers and loads their data.
    ///
    /// If a given plugin filename (or one that is case-insensitively equal) has
    /// already been loaded, its previously-loaded data data is discarded.
    ///
    /// Relative paths in `plugin_paths` are resolved relative to the game's
    /// plugins directory, while absolute paths are used as given. Each plugin
    /// filename must be unique within the vector.
    ///
    /// Loading plugins clears the condition cache in this game's database
    /// object.
    pub fn load_plugin_headers(&mut self, plugin_paths: &[&Path]) -> Result<(), LoadPluginsError> {
        let plugins = self.load_plugins_common(plugin_paths, LoadScope::HeaderOnly)?;

        self.store_plugins(plugins)?;

        Ok(())
    }

    fn load_plugins_common(
        &mut self,
        plugin_paths: &[&Path],
        load_scope: LoadScope,
    ) -> Result<Vec<Plugin>, LoadPluginsError> {
        validate_plugin_paths(self.game_type, plugin_paths)?;

        let data_path = data_path(self.game_type, &self.game_path);

        let archive_paths =
            find_archives(self.game_type, self.additional_data_paths(), &data_path)?;

        self.cache.set_archive_paths(archive_paths);

        log::trace!("Starting loading {}s.", load_scope);

        let plugins: Vec<_> = plugin_paths
            .par_iter()
            .filter_map(|path| {
                try_load_plugin(&data_path, path, self.game_type, &self.cache, load_scope)
            })
            .collect();

        Ok(plugins)
    }

    fn store_plugins(&mut self, plugins: Vec<Plugin>) -> Result<(), DatabaseLockPoisonError> {
        self.cache.insert_plugins(plugins);

        let mut database = self.database.write()?;
        update_loaded_plugin_state(
            database.condition_evaluator_state_mut(),
            self.cache.plugins(),
        );

        Ok(())
    }

    /// Clears the plugins loaded by previous calls to [Game::load_plugins] or
    /// [Game::load_plugin_headers].
    pub fn clear_loaded_plugins(&mut self) {
        self.cache.clear_plugins();
    }

    /// Get data for a loaded plugin.
    pub fn plugin(&self, plugin_name: &str) -> Option<&Plugin> {
        self.cache.plugin(plugin_name)
    }

    /// Get data for all loaded plugins.
    pub fn loaded_plugins(&self) -> Vec<&Plugin> {
        self.cache.plugins().collect()
    }

    /// Calculates a new load order for the game's installed plugins (including
    /// inactive plugins) and returns the sorted order.
    ///
    /// This pulls metadata from the masterlist and userlist if they are loaded,
    /// and uses the loaded data of each plugin. No changes are applied to the
    /// load order used by the game. This function does not load or evaluate the
    /// masterlist or userlist.
    ///
    /// The order in which plugins are listed in `plugin_filenames` is used as
    /// their current load order. All given plugins must have been already been
    /// loaded using [Game::load_plugins] or [Game::load_plugin_headers].
    pub fn sort_plugins(&self, plugin_names: &[&str]) -> Result<Vec<String>, SortPluginsError> {
        let plugins = plugin_names
            .iter()
            .map(|n| {
                self.plugin(n)
                    .ok_or_else(|| SortPluginsError::PluginNotLoaded(n.to_string()))
            })
            .collect::<Result<Vec<_>, _>>()?;

        let database = self.database.read()?;

        let plugins_sorting_data = plugins
            .into_iter()
            .enumerate()
            .map(|(i, p)| {
                let masterlist_metadata = database.plugin_metadata(p.name(), false, true)?;
                let user_metadata = database.plugin_user_metadata(p.name(), true)?;
                let plugin = PluginSortingData::new(
                    p,
                    masterlist_metadata.as_ref(),
                    user_metadata.as_ref(),
                    i,
                )?;
                Ok::<_, SortPluginsError>(plugin)
            })
            .collect::<Result<Vec<_>, _>>()?;

        if log::log_enabled!(log::Level::Debug) {
            log::debug!("Current load order:");
            for plugin_name in plugin_names {
                log::debug!("\t{}", plugin_name);
            }
        }

        let groups_graph = build_groups_graph(&database.groups(false), database.user_groups())?;

        let new_load_order = sort_plugins(
            plugins_sorting_data,
            &groups_graph,
            self.load_order.game_settings().early_loading_plugins(),
        )?;

        if log::log_enabled!(log::Level::Debug) {
            log::debug!("Sorted load order:");
            for plugin_name in &new_load_order {
                log::debug!("\t{}", plugin_name);
            }
        }

        Ok(new_load_order)
    }

    /// Load the current load order state, discarding any previously held state.
    ///
    /// This function should be called whenever the load order or active state
    /// of plugins "on disk" changes, so that the cached state is updated to
    /// reflect the changes.
    ///
    /// Loading the current load order state clears the condition cache in this
    /// game's database object.
    pub fn load_current_load_order_state(&mut self) -> Result<(), LoadOrderStateError> {
        self.load_order.load()?;

        let mut database = self.database.write()?;
        database.clear_condition_cache();
        database
            .condition_evaluator_state_mut()
            .set_active_plugins(&self.load_order.active_plugin_names());
        Ok(())
    }

    /// Check if the load order is ambiguous.
    ///
    /// This checks that all plugins in the current load order state have a
    /// well-defined position in the "on disk" state, and that all data sources
    /// are consistent. If the load order is ambiguous, different applications
    /// may read different load orders from the same source data.
    pub fn is_load_order_ambiguous(&self) -> Result<bool, LoadOrderError> {
        Ok(self.load_order.is_ambiguous()?)
    }

    /// Gets the path to the file that holds the list of active plugins.
    /// The active plugins file path is often within the game's local path, but
    /// its name and location varies by game and game configuration, so this
    /// function exposes the path that libloot uses.
    pub fn active_plugins_file_path(&self) -> &PathBuf {
        self.load_order.game_settings().active_plugins_file()
    }

    /// Check if the given plugin is active.
    pub fn is_plugin_active(&self, plugin_name: &str) -> bool {
        self.load_order.is_active(plugin_name)
    }

    /// Get the current load order.
    pub fn load_order(&self) -> Vec<&str> {
        self.load_order.plugin_names()
    }

    /// Set the game's load order.
    ///
    /// There is no way to persist the load order of inactive OpenMW plugins, so
    /// setting an OpenMW load order will have no effect if the relative order
    /// of active plugins is unchanged.
    pub fn set_load_order(&mut self, load_order: &[&str]) -> Result<(), LoadOrderError> {
        self.load_order.set_load_order(load_order)?;
        Ok(())
    }
}

fn resolve_path(path: &Path) -> PathBuf {
    if path.is_symlink() {
        path.read_link().unwrap_or_else(|_| path.to_path_buf())
    } else {
        path.to_path_buf()
    }
}

fn data_path(game_type: GameType, game_path: &Path) -> PathBuf {
    match game_type {
        GameType::TES3 => game_path.join("Data Files"),
        GameType::OpenMW => game_path.join("resources/vfs"),
        _ => game_path.join("Data"),
    }
}

fn new_condition_evaluator_state(
    game_type: GameType,
    game_path: &Path,
    load_order: &(dyn WritableLoadOrder + Send + Sync + 'static),
) -> loot_condition_interpreter::State {
    let data_path = data_path(game_type, game_path);

    let mut condition_evaluator_state =
        loot_condition_interpreter::State::new(game_type.into(), data_path);
    condition_evaluator_state.set_additional_data_paths(
        load_order
            .game_settings()
            .additional_plugins_directories()
            .to_vec(),
    );

    condition_evaluator_state
}

fn validate_plugin_paths(
    game_type: GameType,
    plugin_paths: &[&Path],
) -> Result<(), PluginValidationError> {
    // Check that all plugin filenames are unique.
    let mut set = HashSet::new();
    for path in plugin_paths {
        let filename = match path.file_name() {
            Some(f) => f.to_string_lossy(),
            None => {
                return Err(PluginValidationError::invalid(
                    path.into(),
                    InvalidFilenameReason::Empty,
                ));
            }
        };
        if !set.insert(Filename::new(filename.to_string())) {
            return Err(PluginValidationError::invalid(
                path.into(),
                InvalidFilenameReason::NonUnique,
            ));
        }
    }

    plugin_paths
        .par_iter()
        .map(|path| validate_plugin_path_and_header(game_type, path))
        .collect::<Result<(), PluginValidationError>>()
}

fn find_archives(
    game_type: GameType,
    additional_data_paths: &[PathBuf],
    data_path: &Path,
) -> std::io::Result<Vec<PathBuf>> {
    let extension = archive_file_extension(game_type);

    let mut archive_paths = Vec::new();
    for path in additional_data_paths {
        let paths = find_archives_in_path(path, extension)?;
        archive_paths.extend(paths);
    }

    let paths = find_archives_in_path(data_path, extension)?;
    archive_paths.extend(paths);

    Ok(archive_paths)
}

fn archive_file_extension(game_type: GameType) -> &'static str {
    match game_type {
        GameType::FO4 | GameType::FO4VR | GameType::Starfield => ".ba2",
        _ => ".bsa",
    }
}

fn find_archives_in_path(
    parent_path: &Path,
    archive_file_extension: &str,
) -> std::io::Result<Vec<PathBuf>> {
    if !parent_path.exists() {
        return Ok(Vec::new());
    }

    let paths = std::fs::read_dir(parent_path)?
        .filter_map(|e| e.ok())
        .filter(|e| {
            e.file_type().map(|f| f.is_file()).unwrap_or(false)
                && iends_with_ascii(&e.file_name().to_string_lossy(), archive_file_extension)
        })
        .map(|e| e.path())
        .collect();

    Ok(paths)
}

fn try_load_plugin(
    data_path: &Path,
    plugin_path: &Path,
    game_type: GameType,
    game_cache: &GameCache,
    load_scope: LoadScope,
) -> Option<Plugin> {
    let resolved_path = resolve_plugin_path(game_type, data_path, plugin_path);

    match Plugin::new(game_type, game_cache, &resolved_path, load_scope) {
        Ok(p) => Some(p),
        Err(e) => {
            log::error!(
                "Caught error while trying to load \"{}\": {}",
                plugin_path.display(),
                e
            );
            None
        }
    }
}

fn resolve_plugin_path(game_type: GameType, data_path: &Path, plugin_path: &Path) -> PathBuf {
    let plugin_path = data_path.join(plugin_path);

    if game_type != GameType::OpenMW && !plugin_path.exists() {
        if let Some(filename) = plugin_path.file_name() {
            log::debug!(
                "Could not find plugin at {}, adding {} file extension",
                plugin_path.display(),
                GHOST_FILE_EXTENSION
            );
            let mut filename = filename.to_os_string();
            filename.push(GHOST_FILE_EXTENSION);
            plugin_path.with_file_name(filename)
        } else {
            plugin_path
        }
    } else {
        plugin_path
    }
}

fn update_loaded_plugin_state<'a>(
    state: &mut loot_condition_interpreter::State,
    plugins: impl Iterator<Item = &'a Plugin>,
) {
    let mut plugin_versions = Vec::new();
    let mut plugin_crcs = Vec::new();

    for plugin in plugins {
        if let Some(version) = plugin.version() {
            plugin_versions.push((plugin.name(), version));
        }

        if let Some(crc) = plugin.crc() {
            plugin_crcs.push((plugin.name(), crc));
        }
    }

    if let Err(e) = state.clear_condition_cache() {
        log::error!("The condition cache's lock is poisoned, assigning a new cache");
        *e.into_inner() = HashMap::new();
    }

    state.set_plugin_versions(&plugin_versions);

    if let Err(e) = state.set_cached_crcs(&plugin_crcs) {
        log::error!(
            "The condition interpreter's CRC cache's lock is poisoned, clearing the cache and assigning a new value"
        );
        let mut cache = e.into_inner();
        cache.clear();
        *cache = plugin_crcs
            .into_iter()
            .map(|(n, c)| (n.to_lowercase(), c))
            .collect();
    }
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub(crate) struct GameCache {
    plugins: HashMap<Filename, Plugin>,
    archive_paths: HashSet<PathBuf>,
}

impl GameCache {
    fn set_archive_paths(&mut self, archive_paths: Vec<PathBuf>) {
        self.archive_paths.clear();
        self.archive_paths.extend(archive_paths);
    }

    fn insert_plugins(&mut self, plugins: Vec<Plugin>) {
        for plugin in plugins {
            self.plugins
                .insert(Filename::new(plugin.name().to_string()), plugin);
        }
    }

    fn clear_plugins(&mut self) {
        self.plugins.clear();
    }

    fn plugins(&self) -> impl Iterator<Item = &Plugin> {
        self.plugins.values()
    }

    fn plugin(&self, plugin_name: &str) -> Option<&Plugin> {
        self.plugins.get(&Filename::new(plugin_name.to_string()))
    }

    pub fn archives(&self) -> impl Iterator<Item = &PathBuf> {
        self.archive_paths.iter()
    }
}
