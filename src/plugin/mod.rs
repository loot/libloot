pub mod error;

use std::{
    collections::{BTreeMap, BTreeSet},
    fs::File,
    hash::Hasher,
    io::{BufRead, BufReader},
    path::{Path, PathBuf},
    sync::LazyLock,
};

use esplugin::ParseOptions;
use fancy_regex::Regex;

use crate::{
    GameType,
    archive::{assets_in_archives, find_associated_archives},
    game::GameCache,
    logging,
    metadata::plugin_metadata::trim_dot_ghost,
    regex,
};
use error::{
    InvalidFilenameReason, LoadPluginError, PluginDataError, PluginValidationError,
    PluginValidationErrorReason,
};

static VERSION_REGEXES: LazyLock<Box<[Regex]>> = LazyLock::new(|| {
    /* The string below matches the range of version strings supported by
    Pseudosem v1.0.1, excluding space separators, as they make version
    extraction from inside sentences very tricky and have not been
    seen "in the wild". */
    let pseudosem_regex_str = r"(\d+(?:\.\d+)+(?:[-._:]?[A-Za-z0-9]+)*)(?!,)";

    Box::new([
        /* The string below matches timestamps that use forwardslashes for date
        separators. However, Pseudosem v1.0.1 will only compare the first
        two digits as it does not recognise forwardslashes as separators. */
        regex(r"(\d{1,2}/\d{1,2}/\d{1,4} \d{1,2}:\d{1,2}:\d{1,2})")
            .expect("Hardcoded version timestamp regex should be valid"),
        regex(&(String::from(r"version:?\s") + pseudosem_regex_str))
            .expect("Hardcoded version-prefixed pseudosem version regex should be valid"),
        regex(&(String::from(r"(?:^|v|\s)") + pseudosem_regex_str))
            .expect("Hardcoded pseudosem version regex should be valid"),
        /* The string below matches a number containing one or more
        digits found at the start of the search string or preceded by
        'v' or 'version:. */
        regex(r"(?:^|v|version:\s*)(\d+)")
            .expect("Hardcoded prefixed version number regex should be valid"),
    ])
});

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub(crate) enum LoadScope {
    HeaderOnly,
    WholePlugin,
}

impl std::fmt::Display for LoadScope {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            LoadScope::HeaderOnly => write!(f, "plugin header"),
            LoadScope::WholePlugin => write!(f, "whole plugin"),
        }
    }
}

/// Represents a plugin file that has been loaded.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Plugin {
    name: String,
    plugin: Option<esplugin::Plugin>,
    game_type: GameType,
    crc: Option<u32>,
    version: Option<String>,
    tags: Vec<String>,
    archive_paths: Vec<PathBuf>,
    archive_assets: BTreeMap<u64, BTreeSet<u64>>,
}

impl Plugin {
    pub(crate) fn new(
        game_type: GameType,
        game_cache: &GameCache,
        plugin_path: &Path,
        load_scope: LoadScope,
    ) -> Result<Self, LoadPluginError> {
        let name = name_string(game_type, plugin_path)?;

        let (parse_options, crc) = if load_scope == LoadScope::HeaderOnly {
            (ParseOptions::header_only(), None)
        } else {
            let crc = calculate_crc(plugin_path)?;
            (ParseOptions::whole_plugin(), Some(crc))
        };

        let mut version = None;
        let mut tags = Vec::new();
        let mut archive_paths = Vec::new();
        let mut archive_assets = BTreeMap::new();
        let plugin =
            if game_type != GameType::OpenMW || !has_ascii_extension(plugin_path, "omwscripts") {
                let mut plugin = esplugin::Plugin::new(game_type.into(), plugin_path);
                plugin.parse_file(parse_options)?;

                if let Some(description) = plugin.description()? {
                    tags = extract_bash_tags(&description);
                    version = extract_version(&description)?;
                }

                archive_paths = find_associated_archives(game_type, game_cache, plugin_path);

                if load_scope == LoadScope::WholePlugin {
                    archive_assets = assets_in_archives(&archive_paths);
                }

                Some(plugin)
            } else {
                None
            };

        Ok(Self {
            name,
            plugin,
            game_type,
            crc,
            version,
            tags,
            archive_paths,
            archive_assets,
        })
    }

    /// Get the plugin's filename.
    ///
    /// If the plugin was ghosted when it was loaded, this filename will be
    /// without the .ghost suffix, unless the game is OpenMW, in which case
    /// ghosted plugins are not supported.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get the value of the version field in the `HEDR` subrecord of the
    /// plugin's `TES4` record.
    ///
    /// Returns `None` if the `TES4` record does not exist (e.g. for Morrowind
    /// and OpenMW) or if the `HEDR` subrecord could not be found, of if the
    /// version field's value was `NaN`.
    pub fn header_version(&self) -> Option<f32> {
        self.plugin.as_ref().and_then(|p| p.header_version())
    }

    /// Get the plugin's version number from its description field.
    ///
    /// The description field may not contain a version number, or libloot may
    /// be unable to detect it. The description field parsing may fail to
    /// extract the version number correctly, though it functions correctly in
    /// all known cases.
    pub fn version(&self) -> Option<&str> {
        self.version.as_deref()
    }

    /// Get the plugin's masters.
    pub fn masters(&self) -> Result<Vec<String>, PluginDataError> {
        self.plugin
            .as_ref()
            .map(|p| p.masters().map_err(Into::into))
            .unwrap_or_else(|| Ok(Vec::new()))
    }

    /// Get any Bash Tags found in the plugin's description field.
    pub fn bash_tags(&self) -> &[String] {
        &self.tags
    }

    /// Get the plugin's CRC-32 checksum.
    ///
    /// This will be `None` if the plugin is not fully loaded.
    pub fn crc(&self) -> Option<u32> {
        self.crc
    }

    /// Check if the plugin is a master plugin.
    ///
    /// What causes a plugin to be a master plugin varies by game, but is
    /// usually indicated by the plugin having its master flag set and/or by its
    /// file extension. However, OpenMW uses neither for determining plugins'
    /// load order so all OpenMW plugins are treated as non-masters.
    ///
    /// The term "master" is potentially confusing: a plugin A may not be a
    /// *master plugin*, but may still be a *master of* another plugin by being
    /// listed as such in that plugin's header record. Master plugins are
    /// sometimes referred to as *master files* or simply *masters*, while the
    /// other meaning is always referenced in relation to another plugin.
    pub fn is_master(&self) -> bool {
        if self.game_type == GameType::OpenMW {
            false
        } else {
            self.plugin
                .as_ref()
                .map(|p| p.is_master_file())
                .unwrap_or(false)
        }
    }

    /// Check if the plugin is a light plugin.
    pub fn is_light_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .map(|p| p.is_light_plugin())
            .unwrap_or(false)
    }

    /// Check if the plugin is a medium plugin.
    pub fn is_medium_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .map(|p| p.is_medium_plugin())
            .unwrap_or(false)
    }

    /// Check if the plugin is an update plugin.
    pub fn is_update_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .map(|p| p.is_update_plugin())
            .unwrap_or(false)
    }

    /// Check if the plugin is a blueprint plugin.
    pub fn is_blueprint_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .map(|p| p.is_blueprint_plugin())
            .unwrap_or(false)
    }

    /// Check if the plugin is or would be valid as a light plugin.
    pub fn is_valid_as_light_plugin(&self) -> Result<bool, PluginDataError> {
        self.plugin
            .as_ref()
            .map(|p| p.is_valid_as_light_plugin().map_err(Into::into))
            .unwrap_or(Ok(false))
    }

    /// Check if the plugin is or would be valid as a medium plugin.
    pub fn is_valid_as_medium_plugin(&self) -> Result<bool, PluginDataError> {
        self.plugin
            .as_ref()
            .map(|p| p.is_valid_as_medium_plugin().map_err(Into::into))
            .unwrap_or(Ok(false))
    }

    /// Check if the plugin is or would be valid as an update plugin.
    pub fn is_valid_as_update_plugin(&self) -> Result<bool, PluginDataError> {
        self.plugin
            .as_ref()
            .map(|p| p.is_valid_as_update_plugin().map_err(Into::into))
            .unwrap_or(Ok(false))
    }

    /// Check if the plugin contains any records other than its `TES3`/`TES4`
    /// header.
    pub fn is_empty(&self) -> bool {
        self.plugin
            .as_ref()
            .and_then(|p| p.record_and_group_count())
            .unwrap_or(0)
            == 0
    }

    /// Check if the plugin loads an archive (BSA/BA2 depending on the game).
    pub fn loads_archive(&self) -> bool {
        !self.archive_paths.is_empty()
    }

    /// Check if two plugins contain a record with the same ID.
    ///
    /// FormIDs are compared for all games apart from Morrowind, which doesn't
    /// have FormIDs and so has other identifying data compared.
    pub fn do_records_overlap(&self, plugin: &Plugin) -> Result<bool, PluginDataError> {
        if let (Some(plugin), Some(other_plugin)) = (&self.plugin, &plugin.plugin) {
            plugin.overlaps_with(other_plugin).map_err(Into::into)
        } else {
            Ok(false)
        }
    }

    pub(crate) fn override_record_count(&self) -> Result<usize, PluginDataError> {
        self.plugin
            .as_ref()
            .map(|p| p.count_override_records().map_err(Into::into))
            .unwrap_or(Ok(0))
    }

    pub(crate) fn asset_count(&self) -> usize {
        self.archive_assets.values().fold(0, |acc, e| acc + e.len())
    }

    pub(crate) fn do_assets_overlap(&self, plugin: &Plugin) -> bool {
        let mut assets_iter = self.archive_assets.iter();
        let mut other_assets_iter = plugin.archive_assets.iter();

        let mut assets = assets_iter.next();
        let mut other_assets = other_assets_iter.next();
        while let (Some((folder, files)), Some((other_folder, other_files))) =
            (assets, other_assets)
        {
            if folder < other_folder {
                assets = assets_iter.next();
            } else if folder > other_folder {
                other_assets = other_assets_iter.next();
            } else if files.intersection(other_files).next().is_some() {
                return true;
            } else {
                // The folder hashes are equal but they don't contain any of the same
                // file hashes, move on to the next folder. It doesn't matter which
                // iterator gets incremented.
                assets = assets_iter.next();
            }
        }

        false
    }

    pub(crate) fn resolve_record_ids(
        &mut self,
        plugins_metadata: &[esplugin::PluginMetadata],
    ) -> Result<(), PluginDataError> {
        if let Some(plugin) = &mut self.plugin {
            plugin.resolve_record_ids(plugins_metadata)?;
        }
        Ok(())
    }
}

pub(crate) fn validate_plugin_path_and_header(
    game_type: GameType,
    plugin_path: &Path,
) -> Result<(), PluginValidationError> {
    if game_type == GameType::OpenMW && has_ascii_extension(plugin_path, "omwscripts") {
        Ok(())
    } else if !has_plugin_file_extension(game_type, plugin_path) {
        logging::debug!(
            "The file \"{}\" is not a valid plugin",
            plugin_path.display()
        );
        Err(PluginValidationError::invalid(
            plugin_path.into(),
            InvalidFilenameReason::UnsupportedFileExtension,
        ))
    } else if esplugin::Plugin::is_valid(game_type.into(), plugin_path, ParseOptions::header_only())
    {
        Ok(())
    } else {
        logging::debug!(
            "The file \"{}\" is not a valid plugin",
            plugin_path.display()
        );
        Err(PluginValidationError::new(
            plugin_path.into(),
            PluginValidationErrorReason::InvalidPluginHeader,
        ))
    }
}

fn has_plugin_file_extension(game_type: GameType, plugin_path: &Path) -> bool {
    let extension = if game_type != GameType::OpenMW && has_ascii_extension(plugin_path, "ghost") {
        plugin_path
            .file_stem()
            .and_then(|s| Path::new(s).extension())
    } else {
        plugin_path.extension()
    };

    if let Some(extension) = extension {
        if extension.eq_ignore_ascii_case("esp")
            || extension.eq_ignore_ascii_case("esm")
            || (game_type == GameType::OpenMW
                && (extension.eq_ignore_ascii_case("omwaddon")
                    || extension.eq_ignore_ascii_case("omwgame")
                    || extension.eq_ignore_ascii_case("omwscripts")))
        {
            true
        } else {
            matches!(
                game_type,
                GameType::FO4
                    | GameType::FO4VR
                    | GameType::TES5SE
                    | GameType::TES5VR
                    | GameType::Starfield
            ) && extension.eq_ignore_ascii_case("esl")
        }
    } else {
        false
    }
}

pub(crate) fn has_ascii_extension(path: &Path, extension: &str) -> bool {
    path.extension()
        .map(|e| e.eq_ignore_ascii_case(extension))
        .unwrap_or(false)
}

pub(crate) fn plugins_metadata(
    plugins: &[Plugin],
) -> Result<Vec<esplugin::PluginMetadata>, PluginDataError> {
    let esplugins: Vec<_> = plugins.iter().filter_map(|p| p.plugin.as_ref()).collect();
    Ok(esplugin::plugins_metadata(&esplugins)?)
}

fn name_string(game_type: GameType, path: &Path) -> Result<String, LoadPluginError> {
    match path.file_name() {
        Some(f) => match f.to_str() {
            Some(f) if game_type == GameType::OpenMW => Ok(f.to_string()),
            Some(f) => Ok(trim_dot_ghost(f).to_string()),
            None => Err(LoadPluginError::InvalidFilename(
                InvalidFilenameReason::NonUnicode,
            )),
        },
        None => Err(LoadPluginError::InvalidFilename(
            InvalidFilenameReason::Empty,
        )),
    }
}

fn calculate_crc(path: &Path) -> std::io::Result<u32> {
    let file = File::open(path)?;
    let mut reader = BufReader::new(file);
    let mut hasher = crc32fast::Hasher::new();

    let mut buffer = reader.fill_buf()?;
    while !buffer.is_empty() {
        hasher.write(buffer);
        let length = buffer.len();
        reader.consume(length);

        buffer = reader.fill_buf()?;
    }

    Ok(hasher.finalize())
}

fn extract_bash_tags(description: &str) -> Vec<String> {
    let bash_tags_opener = "{{BASH:";

    if let Some(mut start_pos) = description.find(bash_tags_opener) {
        start_pos += bash_tags_opener.len();

        if let Some(end_pos) = description[start_pos..].find("}}") {
            return description[start_pos..start_pos + end_pos]
                .split(",")
                .map(|s| s.trim().to_string())
                .collect();
        }
    }
    Vec::new()
}

fn extract_version(description: &str) -> Result<Option<String>, Box<fancy_regex::Error>> {
    for regex in &*VERSION_REGEXES {
        let version = regex
            .captures(description)?
            .iter()
            .flat_map(|captures| captures.iter())
            .flatten()
            .skip(1) // Skip the first capture as that's the whole regex.
            .map(|m| m.as_str().trim())
            .find(|v| !v.is_empty())
            .map(|v| v.to_string());

        if version.is_some() {
            return Ok(version);
        }
    }

    Ok(None)
}
