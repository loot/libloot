use std::{
    collections::{HashMap, HashSet},
    fmt::Display,
    path::{Path, PathBuf},
    sync::{Arc, RwLock},
};

use loadorder::WritableLoadOrder;
use rayon::iter::{IntoParallelRefIterator, ParallelIterator};

use crate::{
    LogLevel,
    database::Database,
    error::{
        DatabaseLockPoisonError, GameHandleCreationError, LoadOrderError, LoadOrderStateError,
        LoadPluginsError, SortPluginsError,
    },
    escape_ascii,
    logging::{self, format_details, is_log_enabled},
    metadata::{
        Filename,
        plugin_metadata::{GHOST_FILE_EXTENSION, iends_with_ascii},
    },
    plugin::{
        LoadScope, Plugin,
        error::{InvalidFilenameReason, PluginValidationError},
        plugins_metadata, validate_plugin_path_and_header,
    },
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
    Oblivion,
    /// The Elder Scrolls V: Skyrim
    Skyrim,
    /// Fallout 3
    Fallout3,
    /// Fallout: New Vegas
    FalloutNV,
    /// Fallout 4
    Fallout4,
    /// The Elder Scrolls V: Skyrim Special Edition
    SkyrimSE,
    /// Fallout 4 VR
    Fallout4VR,
    /// Skyrim VR
    SkyrimVR,
    /// The Elder Scrolls III: Morrowind
    Morrowind,
    /// Starfield
    Starfield,
    /// OpenMW
    OpenMW,
    /// The Elder Scrolls IV: Oblivion Remastered
    OblivionRemastered,
}

impl Display for GameType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            GameType::Oblivion => write!(f, "The Elder Scrolls IV: Oblivion"),
            GameType::Skyrim => write!(f, "The Elder Scrolls V: Skyrim"),
            GameType::Fallout3 => write!(f, "Fallout 3"),
            GameType::FalloutNV => write!(f, "Fallout: New Vegas"),
            GameType::Fallout4 => write!(f, "Fallout 4"),
            GameType::SkyrimSE => write!(f, "The Elder Scrolls V: Skyrim Special Edition"),
            GameType::Fallout4VR => write!(f, "Fallout 4 VR"),
            GameType::SkyrimVR => write!(f, "The Elder Scrolls V: Skyrim VR"),
            GameType::Morrowind => write!(f, "The Elder Scrolls III: Morrowind"),
            GameType::Starfield => write!(f, "Starfield"),
            GameType::OpenMW => write!(f, "OpenMW"),
            GameType::OblivionRemastered => write!(f, "The Elder Scrolls IV: Oblivion Remastered"),
        }
    }
}

impl From<GameType> for loadorder::GameId {
    fn from(value: GameType) -> Self {
        match value {
            GameType::Oblivion => loadorder::GameId::Oblivion,
            GameType::Skyrim => loadorder::GameId::Skyrim,
            GameType::Fallout3 => loadorder::GameId::Fallout3,
            GameType::FalloutNV => loadorder::GameId::FalloutNV,
            GameType::Fallout4 => loadorder::GameId::Fallout4,
            GameType::SkyrimSE => loadorder::GameId::SkyrimSE,
            GameType::Fallout4VR => loadorder::GameId::Fallout4VR,
            GameType::SkyrimVR => loadorder::GameId::SkyrimVR,
            GameType::Morrowind => loadorder::GameId::Morrowind,
            GameType::Starfield => loadorder::GameId::Starfield,
            GameType::OpenMW => loadorder::GameId::OpenMW,
            GameType::OblivionRemastered => loadorder::GameId::OblivionRemastered,
        }
    }
}

impl From<GameType> for loot_condition_interpreter::GameType {
    fn from(value: GameType) -> Self {
        match value {
            GameType::Oblivion | GameType::OblivionRemastered => {
                loot_condition_interpreter::GameType::Oblivion
            }
            GameType::Skyrim => loot_condition_interpreter::GameType::Skyrim,
            GameType::Fallout3 => loot_condition_interpreter::GameType::Fallout3,
            GameType::FalloutNV => loot_condition_interpreter::GameType::FalloutNV,
            GameType::Fallout4 => loot_condition_interpreter::GameType::Fallout4,
            GameType::SkyrimSE => loot_condition_interpreter::GameType::SkyrimSE,
            GameType::Fallout4VR => loot_condition_interpreter::GameType::Fallout4VR,
            GameType::SkyrimVR => loot_condition_interpreter::GameType::SkyrimVR,
            GameType::Morrowind => loot_condition_interpreter::GameType::Morrowind,
            GameType::Starfield => loot_condition_interpreter::GameType::Starfield,
            GameType::OpenMW => loot_condition_interpreter::GameType::OpenMW,
        }
    }
}

impl From<GameType> for esplugin::GameId {
    fn from(value: GameType) -> Self {
        match value {
            GameType::Oblivion | GameType::OblivionRemastered => esplugin::GameId::Oblivion,
            GameType::Skyrim => esplugin::GameId::Skyrim,
            GameType::Fallout3 => esplugin::GameId::Fallout3,
            GameType::FalloutNV => esplugin::GameId::FalloutNV,
            GameType::Fallout4 | GameType::Fallout4VR => esplugin::GameId::Fallout4,
            GameType::SkyrimSE | GameType::SkyrimVR => esplugin::GameId::SkyrimSE,
            GameType::Morrowind | GameType::OpenMW => esplugin::GameId::Morrowind,
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
        logging::info!(
            "Attempting to create a game handle for game type \"{}\" with game path \"{}\"",
            game_type,
            escape_ascii(game_path)
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
        logging::info!(
            "Attempting to create a game handle for game type \"{}\" with game path \"{}\" and game local path \"{}\"",
            game_type,
            escape_ascii(game_path),
            escape_ascii(game_local_path)
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
        let resolved_path = resolve_plugin_path(
            self.game_type,
            &data_path(self.game_type, &self.game_path),
            plugin_path,
        );
        validate_plugin_path_and_header(self.game_type, &resolved_path).is_ok()
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
            GameType::Morrowind | GameType::OpenMW | GameType::Starfield
        ) {
            let mut loaded_plugins: HashMap<Filename, &Plugin> = self
                .cache
                .plugins()
                .iter()
                .map(|(k, v)| (k.clone(), v.as_ref()))
                .collect();

            for plugin in &plugins {
                loaded_plugins.insert(Filename::new(plugin.name().to_owned()), plugin);
            }

            let loaded_plugins: Vec<_> = loaded_plugins.into_values().collect();

            let plugins_metadata = plugins_metadata(&loaded_plugins)?;

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
        let data_path = data_path(self.game_type, &self.game_path);

        validate_plugin_paths(self.game_type, &data_path, plugin_paths)?;

        let archive_paths =
            find_archives(self.game_type, self.additional_data_paths(), &data_path)?;

        self.cache.set_archive_paths(archive_paths);

        logging::trace!("Starting loading {load_scope}s.");

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
            self.cache.plugins_iter(),
        );

        Ok(())
    }

    /// Clears the plugins loaded by previous calls to [Game::load_plugins] or
    /// [Game::load_plugin_headers].
    pub fn clear_loaded_plugins(&mut self) {
        self.cache.clear_plugins();
    }

    /// Get data for a loaded plugin.
    pub fn plugin(&self, plugin_name: &str) -> Option<Arc<Plugin>> {
        self.cache.plugin(plugin_name).cloned()
    }

    /// Get data for all loaded plugins.
    pub fn loaded_plugins(&self) -> Vec<Arc<Plugin>> {
        self.cache.plugins_iter().cloned().collect()
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
                self.cache
                    .plugin(n)
                    .ok_or_else(|| SortPluginsError::PluginNotLoaded((*n).to_owned()))
            })
            .collect::<Result<Vec<_>, _>>()?;

        let database = self.database.read()?;

        let plugins_sorting_data = plugins
            .into_iter()
            .enumerate()
            .map(|(i, p)| to_plugin_sorting_data(&database, p, i))
            .collect::<Result<Vec<_>, _>>()?;

        if is_log_enabled(LogLevel::Debug) {
            logging::debug!("Current load order:");
            for plugin_name in plugin_names {
                logging::debug!("\t{plugin_name}");
            }
        }

        let groups_graph = build_groups_graph(&database.groups(false), database.user_groups())?;

        let new_load_order = sort_plugins(
            plugins_sorting_data,
            &groups_graph,
            self.load_order.game_settings().early_loading_plugins(),
        )?;

        if is_log_enabled(LogLevel::Debug) {
            logging::debug!("Sorted load order:");
            for plugin_name in &new_load_order {
                logging::debug!("\t{plugin_name}");
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
        self.load_order.save()?;
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
        GameType::Morrowind => game_path.join("Data Files"),
        GameType::OpenMW => game_path.join("resources/vfs"),
        GameType::OblivionRemastered => {
            game_path.join("OblivionRemastered/Content/Dev/ObvData/Data")
        }
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
    data_path: &Path,
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
        .map(|path| {
            let resolved_path = resolve_plugin_path(game_type, data_path, path);
            validate_plugin_path_and_header(game_type, &resolved_path)
        })
        .collect()
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
        GameType::Fallout4 | GameType::Fallout4VR | GameType::Starfield => ".ba2",
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
        .filter_map(Result::ok)
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
            logging::error!(
                "Caught error while trying to load \"{}\": {}",
                escape_ascii(plugin_path),
                format_details(&e)
            );
            None
        }
    }
}

fn resolve_plugin_path(game_type: GameType, data_path: &Path, plugin_path: &Path) -> PathBuf {
    let plugin_path = data_path.join(plugin_path);

    if game_type != GameType::OpenMW && !plugin_path.exists() {
        if let Some(filename) = plugin_path.file_name() {
            logging::debug!(
                "Could not find plugin at \"{}\", adding {} file extension",
                escape_ascii(&plugin_path),
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
    plugins: impl Iterator<Item = &'a Arc<Plugin>>,
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
        logging::error!("The condition cache's lock is poisoned, assigning a new cache");
        *e.into_inner() = HashMap::new();
    }

    state.set_plugin_versions(&plugin_versions);

    if let Err(e) = state.set_cached_crcs(&plugin_crcs) {
        logging::error!(
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

fn to_plugin_sorting_data<'a>(
    database: &Database,
    plugin: &'a Arc<Plugin>,
    load_order_index: usize,
) -> Result<PluginSortingData<'a, Plugin>, SortPluginsError> {
    let masterlist_metadata = database
        .plugin_metadata(plugin.name(), false, true)?
        .map(|m| m.filter_by_constraints(database))
        .transpose()?;

    let user_metadata = database
        .plugin_user_metadata(plugin.name(), true)?
        .map(|m| m.filter_by_constraints(database))
        .transpose()?;

    PluginSortingData::new(
        plugin.as_ref(),
        masterlist_metadata.as_ref(),
        user_metadata.as_ref(),
        load_order_index,
    )
    .map_err(Into::into)
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub(crate) struct GameCache {
    plugins: HashMap<Filename, Arc<Plugin>>,
    archive_paths: HashSet<PathBuf>,
}

impl GameCache {
    pub fn set_archive_paths(&mut self, archive_paths: Vec<PathBuf>) {
        self.archive_paths.clear();
        self.archive_paths.extend(archive_paths);
    }

    fn insert_plugins(&mut self, plugins: Vec<Plugin>) {
        for plugin in plugins {
            self.plugins
                .insert(Filename::new(plugin.name().to_owned()), Arc::new(plugin));
        }
    }

    fn clear_plugins(&mut self) {
        self.plugins.clear();
    }

    fn plugins(&self) -> &HashMap<Filename, Arc<Plugin>> {
        &self.plugins
    }

    fn plugins_iter(&self) -> impl Iterator<Item = &Arc<Plugin>> {
        self.plugins.values()
    }

    fn plugin(&self, plugin_name: &str) -> Option<&Arc<Plugin>> {
        self.plugins.get(&Filename::new(plugin_name.to_owned()))
    }

    pub fn archives_iter(&self) -> impl Iterator<Item = &PathBuf> {
        self.archive_paths.iter()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use parameterized_test::parameterized_test;

    use crate::{
        metadata::{File, PluginMetadata},
        tests::{
            ALL_GAME_TYPES, BLANK_DIFFERENT_ESM, BLANK_DIFFERENT_ESP, BLANK_ESM, BLANK_ESP,
            BLANK_MASTER_DEPENDENT_ESM, Fixture,
        },
    };

    mod game {
        use std::path::Component;

        use crate::tests::BLANK_ESM;

        use super::*;

        #[cfg(windows)]
        fn symlink_dir(original: &Path, link: &Path) {
            std::os::windows::fs::symlink_dir(original, link).unwrap();
        }

        #[cfg(unix)]
        fn symlink_dir(original: &Path, link: &Path) {
            std::os::unix::fs::symlink(original, link).unwrap();
        }

        #[cfg(windows)]
        fn junction_link(original: &Path, link: &Path) {
            use std::ffi::{OsStr, OsString};

            // The paths may contain forward slashes, which cmd doesn't accept,
            // so replace them.
            let original = OsString::from(original.to_str().unwrap().replace('/', "\\"));
            let link = OsString::from(link.to_str().unwrap().replace('/', "\\"));

            let status = std::process::Command::new("cmd")
                .args([
                    OsStr::new("/C"),
                    OsStr::new("mklink"),
                    OsStr::new("/J"),
                    link.as_os_str(),
                    original.as_os_str(),
                ])
                .status()
                .unwrap();

            assert!(status.success());
        }

        fn make_relative(path: &Path) -> PathBuf {
            if path.is_relative() {
                return path.to_path_buf();
            }

            let base = std::env::current_dir().unwrap();
            assert!(base.is_absolute());

            let mut path_iter = path.components();
            let mut base_iter = base.components();

            let mut relative_components = Vec::new();
            loop {
                match (path_iter.next(), base_iter.next()) {
                    (None, None) => break,
                    (None, _) => relative_components.push(Component::ParentDir),
                    (Some(p), None) => {
                        relative_components.push(p);
                        relative_components.extend(path_iter);
                        break;
                    }
                    (Some(p), Some(b)) => {
                        if relative_components.is_empty() && p == b {
                            continue;
                        }

                        relative_components.push(Component::ParentDir);
                        for _ in base_iter {
                            relative_components.push(Component::ParentDir);
                        }

                        relative_components.push(p);
                        relative_components.extend(path_iter);
                        break;
                    }
                }
            }

            relative_components
                .into_iter()
                .map(Component::as_os_str)
                .collect()
        }

        mod new {
            use super::*;

            #[cfg(windows)]
            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_succeed_if_given_valid_game_path(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                assert!(Game::new(fixture.game_type, &fixture.game_path).is_ok());
            }

            #[cfg(not(windows))]
            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_succeed_for_morrowind_if_given_valid_game_path(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                if matches!(
                    game_type,
                    GameType::Morrowind | GameType::OpenMW | GameType::OblivionRemastered
                ) {
                    assert!(Game::new(fixture.game_type, &fixture.game_path).is_ok());
                } else {
                    assert!(Game::new(fixture.game_type, &fixture.game_path).is_err());
                }
            }

            #[test]
            fn should_succeed_if_given_a_relative_game_path() {
                let fixture = Fixture::in_path(GameType::Morrowind, Path::new("target"));

                let game_path = make_relative(&fixture.game_path);
                assert!(game_path.is_relative());

                assert!(Game::new(fixture.game_type, &game_path).is_ok());
            }

            #[test]
            fn should_succeed_if_given_an_absolute_game_path() {
                let fixture = Fixture::new(GameType::Morrowind);

                assert!(fixture.game_path.is_absolute());
                assert!(Game::new(fixture.game_type, &fixture.game_path).is_ok());
            }

            #[test]
            fn should_succeed_if_given_a_symlink_path() {
                let fixture = Fixture::new(GameType::Morrowind);

                let game_path = fixture.game_path.with_extension("symlink");
                symlink_dir(&fixture.game_path, &game_path);
                assert!(game_path.is_symlink());

                assert!(Game::new(fixture.game_type, &game_path).is_ok());
            }

            #[cfg(windows)]
            #[test]
            fn should_succeed_if_given_a_junction_link_path() {
                let fixture = Fixture::new(GameType::Morrowind);

                let game_path = fixture.game_path.with_extension("junction");
                junction_link(&fixture.game_path, &game_path);

                assert!(Game::new(fixture.game_type, &game_path).is_ok());
            }

            #[test]
            fn should_error_if_given_a_game_path_that_does_not_exist() {
                let game_path = Path::new("missing");
                match Game::new(GameType::Morrowind, game_path) {
                    Err(GameHandleCreationError::NotADirectory(p)) => assert_eq!(game_path, p),
                    _ => panic!("Expected a not-a-directory error"),
                }
            }
        }

        mod with_local_path {
            use super::*;

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_succeed_if_given_valid_paths(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                );

                assert!(game.is_ok());
            }

            #[test]
            fn should_succeed_if_given_relative_paths() {
                let fixture = Fixture::in_path(GameType::Morrowind, Path::new("target"));

                let game_path = make_relative(&fixture.game_path);
                assert!(game_path.is_relative());

                let local_path = make_relative(&fixture.local_path);
                assert!(local_path.is_relative());

                let game = Game::with_local_path(fixture.game_type, &game_path, &local_path);

                assert!(game.is_ok());
            }

            #[test]
            fn should_succeed_if_given_absolute_paths() {
                let fixture = Fixture::new(GameType::Oblivion);

                assert!(fixture.game_path.is_absolute());
                assert!(fixture.local_path.is_absolute());

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                );

                assert!(game.is_ok());
            }

            #[test]
            fn should_succeed_if_given_symlink_paths() {
                let fixture = Fixture::new(GameType::Oblivion);

                let game_path = fixture.game_path.with_extension("symlink");
                symlink_dir(&fixture.game_path, &game_path);
                assert!(game_path.is_symlink());

                let local_path = fixture.local_path.with_extension("symlink");
                symlink_dir(&fixture.local_path, &local_path);
                assert!(local_path.is_symlink());

                let game = Game::with_local_path(fixture.game_type, &game_path, &local_path);

                assert!(game.is_ok());
            }

            #[cfg(windows)]
            #[test]
            fn should_succeed_if_given_junction_link_paths() {
                let fixture = Fixture::new(GameType::Oblivion);

                let game_path = fixture.game_path.with_extension("junction");
                junction_link(&fixture.game_path, &game_path);

                let local_path = fixture.local_path.with_extension("junction");
                junction_link(&fixture.local_path, &local_path);

                let game = Game::with_local_path(fixture.game_type, &game_path, &local_path);

                assert!(game.is_ok());
            }

            #[test]
            fn should_error_if_given_a_game_path_that_does_not_exist() {
                let fixture = Fixture::new(GameType::Oblivion);

                let game_path = Path::new("missing");
                let game = Game::with_local_path(fixture.game_type, game_path, &fixture.local_path);

                match game {
                    Err(GameHandleCreationError::NotADirectory(p)) => assert_eq!(game_path, p),
                    _ => panic!("Expected a not-a-directory error"),
                }
            }

            #[test]
            fn should_succeed_if_given_a_local_path_that_does_not_exist() {
                let fixture = Fixture::new(GameType::Oblivion);

                let local_path = Path::new("missing");
                let game = Game::with_local_path(fixture.game_type, &fixture.game_path, local_path);

                assert!(game.is_ok());
            }

            #[test]
            fn should_error_if_given_a_local_path_that_is_not_a_directory() {
                let fixture = Fixture::new(GameType::Oblivion);

                let local_path = Path::new("README.md");
                assert!(local_path.exists());

                let game = Game::with_local_path(fixture.game_type, &fixture.game_path, local_path);

                match game {
                    Err(GameHandleCreationError::NotADirectory(p)) => assert_eq!(local_path, p),
                    _ => panic!("Expected a not-a-directory error"),
                }
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_set_default_additional_data_paths(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                match game_type {
                    GameType::Fallout4 => {
                        std::fs::File::create(fixture.game_path.join("appxmanifest.xml")).unwrap();
                    }
                    GameType::OpenMW => {
                        let contents = format!(
                            "data-local=\"{}\"\nconfig=\"{}\"",
                            fixture.local_path.join("data").display(),
                            fixture.local_path.display()
                        );
                        std::fs::write(fixture.game_path.join("openmw.cfg"), contents).unwrap();
                    }
                    _ => {}
                }

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                match game_type {
                    GameType::Fallout4 => {
                        let base_path = fixture.game_path.join("../..");
                        assert_eq!(
                            &[
                                base_path
                                    .join("Fallout 4- Automatron (PC)")
                                    .join("Content")
                                    .join("Data"),
                                base_path
                                    .join("Fallout 4- Nuka-World (PC)")
                                    .join("Content")
                                    .join("Data"),
                                base_path
                                    .join("Fallout 4- Wasteland Workshop (PC)")
                                    .join("Content")
                                    .join("Data"),
                                base_path
                                    .join("Fallout 4- High Resolution Texture Pack")
                                    .join("Content")
                                    .join("Data"),
                                base_path
                                    .join("Fallout 4- Vault-Tec Workshop (PC)")
                                    .join("Content")
                                    .join("Data"),
                                base_path
                                    .join("Fallout 4- Far Harbor (PC)")
                                    .join("Content")
                                    .join("Data"),
                                base_path
                                    .join("Fallout 4- Contraptions Workshop (PC)")
                                    .join("Content")
                                    .join("Data")
                            ],
                            game.additional_data_paths()
                        );
                    }
                    GameType::Starfield => {
                        assert_eq!(1, game.additional_data_paths().len());

                        let expected_suffix = Path::new("Documents")
                            .join("My Games")
                            .join("Starfield")
                            .join("Data");

                        assert!(game.additional_data_paths()[0].ends_with(expected_suffix));
                    }
                    GameType::OpenMW => {
                        assert_eq!(
                            &[fixture.local_path.join("data")],
                            game.additional_data_paths()
                        );
                    }
                    _ => assert!(game.additional_data_paths().is_empty()),
                }
            }

            mod set_additional_data_paths {
                use std::time::{Duration, SystemTime};

                use super::*;

                #[test]
                fn should_clear_the_condition_cache() {
                    let fixture = Fixture::new(GameType::Oblivion);

                    let mut game = Game::with_local_path(
                        fixture.game_type,
                        &fixture.game_path,
                        &fixture.local_path,
                    )
                    .unwrap();

                    let mut metadata = PluginMetadata::new(BLANK_ESM).unwrap();
                    metadata.set_load_after_files(vec![
                        File::new("plugin.esp".into())
                            .with_condition("file(\"plugin.esp\")".into()),
                    ]);
                    game.database()
                        .write()
                        .unwrap()
                        .set_plugin_user_metadata(metadata);

                    let evaluated_metadata = game
                        .database()
                        .read()
                        .unwrap()
                        .plugin_user_metadata(BLANK_ESM, true)
                        .unwrap();
                    assert!(evaluated_metadata.is_none());

                    std::fs::File::create(fixture.data_path().join("plugin.esp")).unwrap();

                    game.set_additional_data_paths(&[Path::new("")]).unwrap();

                    let evaluated_metadata = game
                        .database()
                        .read()
                        .unwrap()
                        .plugin_user_metadata(BLANK_ESM, true)
                        .unwrap()
                        .unwrap();
                    assert!(!evaluated_metadata.load_after_files().is_empty());
                }

                #[test]
                fn should_update_where_load_order_plugins_are_found() {
                    let fixture = Fixture::new(GameType::Oblivion);

                    let mut game = Game::with_local_path(
                        fixture.game_type,
                        &fixture.game_path,
                        &fixture.local_path,
                    )
                    .unwrap();

                    game.load_current_load_order_state().unwrap();

                    let mut load_order: Vec<_> =
                        game.load_order().iter().map(ToString::to_string).collect();

                    let filename = "plugin.esp";
                    let data_file_path = fixture
                        .game_path
                        .parent()
                        .unwrap()
                        .join("Data")
                        .join(filename);

                    std::fs::create_dir_all(data_file_path.parent().unwrap()).unwrap();
                    std::fs::copy(fixture.data_path().join(BLANK_ESP), &data_file_path).unwrap();

                    std::fs::File::options()
                        .write(true)
                        .open(&data_file_path)
                        .unwrap()
                        .set_modified(SystemTime::now() + Duration::from_secs(3600))
                        .unwrap();

                    game.set_additional_data_paths(&[data_file_path.parent().unwrap()])
                        .unwrap();
                    game.load_current_load_order_state().unwrap();

                    load_order.push(filename.to_owned());

                    assert_eq!(load_order, game.load_order());
                }
            }
        }

        mod is_valid_plugin {
            use super::*;

            use crate::tests::{NON_ASCII_ESM, NON_PLUGIN_FILE};

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_return_true_for_a_valid_non_ascii_plugin(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                std::fs::copy(
                    fixture.data_path().join(BLANK_ESM),
                    fixture.data_path().join(NON_ASCII_ESM),
                )
                .unwrap();

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                assert!(game.is_valid_plugin(Path::new(NON_ASCII_ESM)));
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_return_true_for_an_omwscripts_plugin(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let plugin = fixture.data_path().join("empty.omwscripts");
                std::fs::File::create(&plugin).unwrap();

                if game_type == GameType::OpenMW {
                    assert!(game.is_valid_plugin(&plugin));
                } else {
                    assert!(!game.is_valid_plugin(&plugin));
                }
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_return_false_for_a_non_plugin_file(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                assert!(!game.is_valid_plugin(Path::new(NON_PLUGIN_FILE)));
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_return_false_for_an_empty_file(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let empty_file_path = fixture.data_path().join("empty.esp");
                std::fs::File::create(&empty_file_path).unwrap();

                assert!(!game.is_valid_plugin(&empty_file_path));
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_try_ghosted_path_if_given_plugin_does_not_exist_unless_game_is_openmw(
                game_type: GameType,
            ) {
                let fixture = Fixture::new(game_type);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let path = fixture.data_path().join(BLANK_MASTER_DEPENDENT_ESM);

                if game_type == GameType::OpenMW {
                    std::fs::rename(
                        &path,
                        path.with_file_name(format!("{BLANK_MASTER_DEPENDENT_ESM}.ghost")),
                    )
                    .unwrap();

                    assert!(!game.is_valid_plugin(&path));
                } else {
                    assert!(!path.exists());

                    assert!(game.is_valid_plugin(&path));
                }
            }

            #[test]
            fn should_resolve_relative_paths_relative_to_the_data_path() {
                let fixture = Fixture::new(GameType::Oblivion);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let path = Path::new("..")
                    .join(fixture.data_path().file_name().unwrap())
                    .join(BLANK_ESM);

                assert!(game.is_valid_plugin(&path));
            }

            #[test]
            fn should_use_absolute_paths_as_given() {
                let fixture = Fixture::new(GameType::Oblivion);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let path = fixture.data_path().join(BLANK_ESM);

                assert!(game.is_valid_plugin(&path));
            }
        }

        mod load_plugin_headers {
            use crate::tests::NON_PLUGIN_FILE;

            use super::*;

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_load_the_headers_of_the_given_plugins(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                assert!(game.plugin(BLANK_ESM).is_none());
                assert!(game.plugin(BLANK_DIFFERENT_ESM).is_none());
                assert!(game.plugin(BLANK_ESP).is_none());

                game.load_plugin_headers(&[Path::new(BLANK_ESM), Path::new(BLANK_ESP)])
                    .unwrap();

                let plugin = game.plugin(BLANK_ESM).unwrap();
                assert_eq!("5.0", plugin.version().unwrap());
                assert!(plugin.crc().is_none());

                assert!(game.plugin(BLANK_DIFFERENT_ESM).is_none());
                assert!(game.plugin(BLANK_ESP).is_some());
            }

            #[test]
            fn should_not_modify_loaded_plugins_storage_if_given_a_non_plugin() {
                let fixture = Fixture::new(GameType::Morrowind);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                game.load_plugin_headers(&[Path::new(BLANK_ESM)]).unwrap();
                assert!(game.plugin(BLANK_ESM).is_some());

                assert!(
                    game.load_plugin_headers(&[Path::new(NON_PLUGIN_FILE)])
                        .is_err()
                );

                assert!(game.plugin(BLANK_ESM).is_some());
                assert!(game.plugin(NON_PLUGIN_FILE).is_none());
            }

            #[test]
            fn should_not_clear_the_plugins_cache() {
                let fixture = Fixture::new(GameType::Morrowind);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                game.load_plugin_headers(&[Path::new(BLANK_ESM)]).unwrap();
                assert!(game.plugin(BLANK_ESM).is_some());

                game.load_plugin_headers(&[Path::new(BLANK_ESP)]).unwrap();

                assert!(game.plugin(BLANK_ESM).is_some());
                assert!(game.plugin(BLANK_ESP).is_some());
            }

            #[test]
            fn should_replace_an_existing_cache_entry_for_the_same_plugin() {
                let fixture = Fixture::new(GameType::Morrowind);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                game.load_plugin_headers(&[Path::new(BLANK_ESM)]).unwrap();
                let plugin1: *const str = game.plugin(BLANK_ESM).unwrap().name();
                let plugin2: *const str = game.plugin(BLANK_ESM).unwrap().name();

                assert_eq!(plugin1, plugin2);

                game.load_plugin_headers(&[Path::new(BLANK_ESM)]).unwrap();

                let plugin3: *const str = game.plugin(BLANK_ESM).unwrap().name();

                assert_ne!(plugin2, plugin3);
            }
        }

        mod load_plugins {
            use std::error::Error;

            use crate::tests::BLANK_FULL_ESM;

            use super::*;

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_fully_load_the_given_plugins(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                assert!(game.plugin(BLANK_ESM).is_none());
                assert!(game.plugin(BLANK_DIFFERENT_ESM).is_none());
                assert!(game.plugin(BLANK_ESP).is_none());

                game.load_plugins(&[Path::new(BLANK_ESM), Path::new(BLANK_ESP)])
                    .unwrap();

                let plugin = game.plugin(BLANK_ESM).unwrap();
                assert_eq!("5.0", plugin.version().unwrap());
                assert!(plugin.crc().is_some());

                assert!(game.plugin(BLANK_DIFFERENT_ESM).is_none());
                assert!(game.plugin(BLANK_ESP).is_some());
            }

            #[test]
            fn should_not_clear_the_plugins_cache() {
                let fixture = Fixture::new(GameType::Morrowind);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                game.load_plugin_headers(&[Path::new(BLANK_ESM)]).unwrap();
                assert!(game.plugin(BLANK_ESM).is_some());

                game.load_plugin_headers(&[Path::new(BLANK_ESP)]).unwrap();

                assert!(game.plugin(BLANK_ESM).is_some());
                assert!(game.plugin(BLANK_ESP).is_some());
            }

            #[test]
            fn should_replace_an_existing_cache_entry_for_the_same_plugin() {
                let fixture = Fixture::new(GameType::Morrowind);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                game.load_plugins(&[Path::new(BLANK_ESM)]).unwrap();
                let plugin1: *const str = game.plugin(BLANK_ESM).unwrap().name();
                let plugin2: *const str = game.plugin(BLANK_ESM).unwrap().name();

                assert_eq!(plugin1, plugin2);

                game.load_plugins(&[Path::new(BLANK_ESM)]).unwrap();

                let plugin3: *const str = game.plugin(BLANK_ESM).unwrap().name();

                assert_ne!(plugin2, plugin3);
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_error_if_loading_a_plugin_with_a_master_that_is_not_loaded_if_game_is_morrowind_or_starfield(
                game_type: GameType,
            ) {
                let fixture = Fixture::new(game_type);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let paths = &[Path::new(BLANK_MASTER_DEPENDENT_ESM)];

                if matches!(
                    game_type,
                    GameType::Morrowind | GameType::OpenMW | GameType::Starfield
                ) {
                    match game.load_plugins(paths) {
                        Err(LoadPluginsError::PluginDataError(e)) => {
                            let source = e.source().unwrap();
                            match source.downcast_ref::<esplugin::Error>().unwrap() {
                                esplugin::Error::PluginMetadataNotFound(p) => {
                                    if game_type == GameType::Starfield {
                                        assert_eq!(BLANK_FULL_ESM, p);
                                    } else {
                                        assert_eq!(BLANK_ESM, p);
                                    }
                                }
                                _ => panic!("Unexpected esplugin error: {e}"),
                            }
                        }
                        _ => panic!("Expected an error due to esplugin metadata not found"),
                    }
                } else {
                    game.load_plugins(paths).unwrap();

                    assert!(game.plugin(BLANK_MASTER_DEPENDENT_ESM).is_some());
                }
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_not_error_if_loading_a_plugin_with_a_master_that_is_also_being_loaded_if_game_is_morrowind_or_starfield(
                game_type: GameType,
            ) {
                let fixture = Fixture::new(game_type);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let paths: &[&Path] = if game_type == GameType::Starfield {
                    &[
                        Path::new(BLANK_MASTER_DEPENDENT_ESM),
                        Path::new(BLANK_FULL_ESM),
                    ]
                } else {
                    &[Path::new(BLANK_MASTER_DEPENDENT_ESM), Path::new(BLANK_ESM)]
                };

                game.load_plugins(paths).unwrap();

                assert!(game.plugin(BLANK_MASTER_DEPENDENT_ESM).is_some());
            }

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_not_error_if_loading_a_plugin_with_a_master_that_is_already_loaded_if_game_is_morrowind_or_starfield(
                game_type: GameType,
            ) {
                let fixture = Fixture::new(game_type);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let master = if game_type == GameType::Starfield {
                    BLANK_FULL_ESM
                } else {
                    BLANK_ESM
                };

                game.load_plugins(&[Path::new(master)]).unwrap();

                game.load_plugins(&[Path::new(BLANK_MASTER_DEPENDENT_ESM)])
                    .unwrap();

                assert!(game.plugin(BLANK_MASTER_DEPENDENT_ESM).is_some());
            }
        }

        mod load_plugins_common {
            use super::*;

            #[parameterized_test(ALL_GAME_TYPES)]
            fn should_find_archives_in_additional_data_paths(game_type: GameType) {
                let fixture = Fixture::new(game_type);

                let extension = if matches!(
                    game_type,
                    GameType::Fallout4 | GameType::Fallout4VR | GameType::Starfield
                ) {
                    ".ba2"
                } else {
                    ".bsa"
                };

                let path1 = fixture
                    .game_path
                    .join("sub1")
                    .join("archive")
                    .with_extension(extension);
                let path2 = fixture
                    .game_path
                    .join("sub2")
                    .join("archive")
                    .with_extension(extension);
                std::fs::create_dir_all(path1.parent().unwrap()).unwrap();
                std::fs::create_dir_all(path2.parent().unwrap()).unwrap();
                std::fs::File::create(&path1).unwrap();
                std::fs::File::create(&path2).unwrap();

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                game.set_additional_data_paths(&[path1.parent().unwrap(), path2.parent().unwrap()])
                    .unwrap();

                game.load_plugins_common(&[], LoadScope::HeaderOnly)
                    .unwrap();

                assert_eq!(HashSet::from([path1, path2]), game.cache.archive_paths);
            }

            #[test]
            fn should_clear_the_archive_cache_before_finding_archives() {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                std::fs::File::create(fixture.data_path().join("Blank.bsa")).unwrap();

                game.load_plugins_common(&[], LoadScope::HeaderOnly)
                    .unwrap();
                game.load_plugins_common(&[], LoadScope::HeaderOnly)
                    .unwrap();

                assert_eq!(1, game.cache.archive_paths.len());
            }

            #[test]
            fn should_not_error_if_an_installed_filename_has_non_windows_1252_encodable_characters()
            {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let filename =
                    "\u{2551}\u{00BB}\u{00C1}\u{2510}\u{2557}\u{00FE}\u{00C3}\u{00CE}.txt";
                std::fs::File::create(fixture.data_path().join(filename)).unwrap();

                assert!(game.load_plugins_common(&[], LoadScope::HeaderOnly).is_ok());
            }

            #[test]
            fn should_error_given_duplicate_filenames() {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let paths = &[
                    Path::new("a").join(BLANK_ESM),
                    Path::new("b").join(BLANK_ESM),
                ];

                let expected_path = if cfg!(windows) {
                    "b\\\\Blank.esm"
                } else {
                    "b/Blank.esm"
                };
                match game.load_plugins_common(&[&paths[0], &paths[1]], LoadScope::HeaderOnly) {
                    Err(LoadPluginsError::PluginValidationError(e)) => {
                        assert_eq!(
                            format!(
                                "the path \"{expected_path}\" has a filename that is not unique"
                            ),
                            e.to_string()
                        );
                    }
                    _ => panic!("Expected an error due to duplicate filenames"),
                }
            }

            #[test]
            fn should_resolve_relative_paths_relative_to_the_data_path() {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let path = Path::new("..")
                    .join(fixture.data_path().file_name().unwrap())
                    .join(BLANK_ESM);

                let plugins = game
                    .load_plugins_common(&[&path], LoadScope::HeaderOnly)
                    .unwrap();

                assert_eq!(1, plugins.len());
                assert_eq!(BLANK_ESM, plugins[0].name());
            }

            #[test]
            fn should_use_absolute_paths_as_given() {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let path = fixture.data_path().join(BLANK_ESM);

                let plugins = game
                    .load_plugins_common(&[&path], LoadScope::HeaderOnly)
                    .unwrap();

                assert_eq!(1, plugins.len());
                assert_eq!(BLANK_ESM, plugins[0].name());
            }

            #[test]
            fn should_trim_ghost_extensions_from_loaded_plugin_names() {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                let path = fixture
                    .data_path()
                    .join(format!("{BLANK_MASTER_DEPENDENT_ESM}.ghost"));

                let plugins = game
                    .load_plugins_common(&[&path], LoadScope::HeaderOnly)
                    .unwrap();

                assert_eq!(1, plugins.len());
                assert_eq!(BLANK_MASTER_DEPENDENT_ESM, plugins[0].name());
            }
        }

        #[test]
        fn clear_loaded_plugins_should_clear_the_plugins_cache() {
            let fixture = Fixture::new(GameType::Oblivion);

            let mut game =
                Game::with_local_path(fixture.game_type, &fixture.game_path, &fixture.local_path)
                    .unwrap();

            game.load_plugin_headers(&[Path::new(BLANK_ESM)]).unwrap();

            assert!(!game.cache.plugins.is_empty());

            game.clear_loaded_plugins();

            assert!(game.cache.plugins.is_empty());
        }

        mod sort_plugins {
            use crate::tests::initial_load_order;

            use super::*;

            fn load_all_installed_plugins(game: &mut Game, fixture: &Fixture) {
                let load_order = initial_load_order(fixture.game_type);

                let plugins: Vec<_> = load_order.iter().map(|(n, _)| Path::new(n)).collect();

                game.load_current_load_order_state().unwrap();
                game.load_plugins(&plugins).unwrap();
            }

            #[test]
            fn should_return_an_empty_list_if_given_an_empty_list() {
                let fixture = Fixture::new(GameType::Oblivion);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                assert!(game.sort_plugins(&[]).unwrap().is_empty());
            }

            #[test]
            fn should_only_sort_the_given_plugins() {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                load_all_installed_plugins(&mut game, &fixture);

                let input = &[BLANK_ESP, BLANK_DIFFERENT_ESP];
                let sorted = game.sort_plugins(input).unwrap();

                assert_eq!(input, sorted.as_slice());
            }

            #[test]
            fn should_error_if_a_given_plugin_is_not_loaded() {
                let fixture = Fixture::new(GameType::Oblivion);

                let game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                assert!(game.sort_plugins(&[BLANK_ESP]).is_err());
            }
        }

        mod is_plugin_active {
            use super::*;

            #[test]
            fn should_be_independent_of_plugins_being_loaded() {
                let fixture = Fixture::new(GameType::Oblivion);

                let mut game = Game::with_local_path(
                    fixture.game_type,
                    &fixture.game_path,
                    &fixture.local_path,
                )
                .unwrap();

                game.load_current_load_order_state().unwrap();

                assert!(game.is_plugin_active(BLANK_ESM));
                assert!(!game.is_plugin_active(BLANK_ESP));

                let paths = &[Path::new(BLANK_ESM), Path::new(BLANK_ESP)];
                game.load_plugin_headers(paths).unwrap();

                assert!(game.is_plugin_active(BLANK_ESM));
                assert!(!game.is_plugin_active(BLANK_ESP));

                game.load_plugins(paths).unwrap();

                assert!(game.is_plugin_active(BLANK_ESM));
                assert!(!game.is_plugin_active(BLANK_ESP));
            }
        }

        #[test]
        fn set_load_order_should_persist_the_given_load_order() {
            let fixture = Fixture::new(GameType::Oblivion);

            let mut game =
                Game::with_local_path(fixture.game_type, &fixture.game_path, &fixture.local_path)
                    .unwrap();

            game.load_current_load_order_state().unwrap();

            let mut load_order: Vec<_> =
                game.load_order().iter().map(ToString::to_string).collect();
            load_order.swap(7, 10);
            let load_order: Vec<_> = load_order.iter().map(String::as_str).collect();

            game.set_load_order(&load_order).unwrap();

            let mut game =
                Game::with_local_path(fixture.game_type, &fixture.game_path, &fixture.local_path)
                    .unwrap();

            game.load_current_load_order_state().unwrap();

            assert_eq!(load_order, game.load_order());
        }

        #[test]
        fn should_support_loading_plugins_and_metadata_in_parallel() {
            let fixture = Fixture::new(GameType::Morrowind);

            let mut game =
                Game::with_local_path(fixture.game_type, &fixture.game_path, &fixture.local_path)
                    .unwrap();

            let masterlist_path = fixture.local_path.join("masterlist.yaml");
            std::fs::write(&masterlist_path, "bash_tags: [Relev]").unwrap();

            std::thread::scope(|s| {
                let database = game.database();
                s.spawn(move || {
                    if let Ok(mut database) = database.write() {
                        database.load_masterlist(&masterlist_path).unwrap();
                    }
                });
                s.spawn(|| {
                    game.load_plugins(&[]).unwrap();
                });
            });
        }
    }

    #[test]
    fn to_plugin_sorting_data_should_filter_out_files_with_false_constraints() {
        let game_type = GameType::Oblivion;
        let true_constraint = "file(\"Blank.esm\")";
        let false_constraint = "file(\"missing.esm\")";

        let fixture = Fixture::new(game_type);

        let plugin = Arc::new(
            Plugin::new(
                game_type,
                &GameCache::default(),
                &fixture.data_path().join(BLANK_ESP),
                LoadScope::HeaderOnly,
            )
            .unwrap(),
        );

        let mut database = Database::new(loot_condition_interpreter::State::new(
            game_type.into(),
            fixture.data_path(),
        ));

        let masterlist_path = fixture.local_path.join("masterlist.yaml");
        let masterlist = format!(
            "{{plugins: [{{name: Blank.esp, after: [{{name: A.esp, constraint: '{true_constraint}'}}, {{name: B.esp, constraint: '{false_constraint}'}}], req: [{{name: C.esp, constraint: '{true_constraint}'}}, {{name: D.esp, constraint: '{false_constraint}'}}]}}]}}"
        );
        std::fs::write(&masterlist_path, masterlist).unwrap();

        database.load_masterlist(&masterlist_path).unwrap();

        let mut user_metadata = PluginMetadata::new(BLANK_ESP).unwrap();
        user_metadata.set_load_after_files(vec![
            File::new(BLANK_ESM.to_owned()).with_constraint(true_constraint.to_owned()),
            File::new(BLANK_DIFFERENT_ESM.to_owned()).with_constraint(false_constraint.to_owned()),
        ]);
        user_metadata.set_requirements(vec![
            File::new(BLANK_DIFFERENT_ESP.to_owned()).with_constraint(true_constraint.to_owned()),
            File::new(BLANK_MASTER_DEPENDENT_ESM.to_owned())
                .with_constraint(false_constraint.to_owned()),
        ]);

        database.set_plugin_user_metadata(user_metadata);

        let data = to_plugin_sorting_data(&database, &plugin, 0).unwrap();

        assert_eq!(["A.esp".to_owned()], *data.masterlist_load_after);
        assert_eq!(["C.esp".to_owned()], *data.masterlist_req);
        assert_eq!([BLANK_ESM.to_owned()], *data.user_load_after);
        assert_eq!([BLANK_DIFFERENT_ESP.to_owned()], *data.user_req);
    }

    mod game_cache {
        use super::*;

        use crate::tests::{BLANK_ESM, source_plugins_path};

        mod insert_plugins {

            use super::*;

            #[test]
            fn should_add_plugins_not_already_cached() {
                let mut cache = GameCache::default();

                cache.insert_plugins(vec![
                    Plugin::new(
                        GameType::Oblivion,
                        &cache,
                        &source_plugins_path(GameType::Oblivion).join(BLANK_ESM),
                        LoadScope::HeaderOnly,
                    )
                    .unwrap(),
                ]);

                assert_eq!(BLANK_ESM, cache.plugin(BLANK_ESM).unwrap().name());
            }

            #[test]
            fn should_replace_plugins_that_are_already_cached() {
                let mut cache = GameCache::default();

                cache.insert_plugins(vec![
                    Plugin::new(
                        GameType::Oblivion,
                        &cache,
                        &source_plugins_path(GameType::Oblivion).join(BLANK_ESM),
                        LoadScope::HeaderOnly,
                    )
                    .unwrap(),
                ]);

                assert!(cache.plugin(BLANK_ESM).unwrap().crc().is_none());

                cache.insert_plugins(vec![
                    Plugin::new(
                        GameType::Oblivion,
                        &cache,
                        &source_plugins_path(GameType::Oblivion).join(BLANK_ESM),
                        LoadScope::WholePlugin,
                    )
                    .unwrap(),
                ]);

                assert!(cache.plugin(BLANK_ESM).unwrap().crc().is_some());
            }
        }

        mod plugin {
            use super::*;

            #[test]
            fn should_be_case_insensitive() {
                let mut cache = GameCache::default();

                cache.insert_plugins(vec![
                    Plugin::new(
                        GameType::Oblivion,
                        &cache,
                        &source_plugins_path(GameType::Oblivion).join(BLANK_ESM),
                        LoadScope::HeaderOnly,
                    )
                    .unwrap(),
                ]);

                assert_eq!("Blank.esm", cache.plugin("blank.esm").unwrap().name());
            }

            #[test]
            fn should_return_none_if_the_plugin_is_not_cached() {
                let cache = GameCache::default();

                assert!(cache.plugin(BLANK_ESM).is_none());
            }
        }

        mod clear_plugins {
            use super::*;

            #[test]
            fn should_clear_any_cached_plugins() {
                let mut cache = GameCache::default();

                cache.insert_plugins(vec![
                    Plugin::new(
                        GameType::Oblivion,
                        &cache,
                        &source_plugins_path(GameType::Oblivion).join(BLANK_ESM),
                        LoadScope::HeaderOnly,
                    )
                    .unwrap(),
                ]);

                assert!(!cache.plugins.is_empty());

                cache.clear_plugins();

                assert!(cache.plugins.is_empty());
            }
        }
    }
}
