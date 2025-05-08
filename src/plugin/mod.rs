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
use fancy_regex::{Error as RegexImplError, Regex};

use crate::{
    GameType,
    archive::{assets_in_archives, do_assets_overlap, find_associated_archives},
    case_insensitive_regex, escape_ascii,
    game::GameCache,
    logging,
    metadata::plugin_metadata::trim_dot_ghost,
};
use error::{
    InvalidFilenameReason, LoadPluginError, PluginDataError, PluginValidationError,
    PluginValidationErrorReason,
};

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
    tags: Box<[String]>,
    archive_paths: Box<[PathBuf]>,
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
        let mut tags = Box::default();
        let mut archive_paths = Box::default();
        let mut archive_assets = BTreeMap::new();
        let plugin =
            if game_type != GameType::OpenMW || !has_ascii_extension(plugin_path, "omwscripts") {
                let mut plugin = esplugin::Plugin::new(game_type.into(), plugin_path);
                plugin.parse_file(parse_options)?;

                if let Some(description) = plugin.description()? {
                    tags = extract_bash_tags(&description).into_boxed_slice();
                    version = extract_version(&description)?;
                }

                archive_paths =
                    find_associated_archives(game_type, game_cache, plugin_path).into_boxed_slice();

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
        self.plugin
            .as_ref()
            .and_then(esplugin::Plugin::header_version)
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
            .map_or_else(|| Ok(Vec::new()), |p| p.masters().map_err(Into::into))
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
        if matches!(
            self.game_type,
            GameType::OpenMW | GameType::OblivionRemastered
        ) {
            false
        } else {
            self.plugin
                .as_ref()
                .is_some_and(esplugin::Plugin::is_master_file)
        }
    }

    /// Check if the plugin is a light plugin.
    pub fn is_light_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .is_some_and(esplugin::Plugin::is_light_plugin)
    }

    /// Check if the plugin is a medium plugin.
    pub fn is_medium_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .is_some_and(esplugin::Plugin::is_medium_plugin)
    }

    /// Check if the plugin is an update plugin.
    pub fn is_update_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .is_some_and(esplugin::Plugin::is_update_plugin)
    }

    /// Check if the plugin is a blueprint plugin.
    pub fn is_blueprint_plugin(&self) -> bool {
        self.plugin
            .as_ref()
            .is_some_and(esplugin::Plugin::is_blueprint_plugin)
    }

    /// Check if the plugin is or would be valid as a light plugin.
    pub fn is_valid_as_light_plugin(&self) -> Result<bool, PluginDataError> {
        self.plugin.as_ref().map_or(Ok(false), |p| {
            p.is_valid_as_light_plugin().map_err(Into::into)
        })
    }

    /// Check if the plugin is or would be valid as a medium plugin.
    pub fn is_valid_as_medium_plugin(&self) -> Result<bool, PluginDataError> {
        self.plugin.as_ref().map_or(Ok(false), |p| {
            p.is_valid_as_medium_plugin().map_err(Into::into)
        })
    }

    /// Check if the plugin is or would be valid as an update plugin.
    pub fn is_valid_as_update_plugin(&self) -> Result<bool, PluginDataError> {
        self.plugin.as_ref().map_or(Ok(false), |p| {
            p.is_valid_as_update_plugin().map_err(Into::into)
        })
    }

    /// Check if the plugin contains any records other than its `TES3`/`TES4`
    /// header.
    pub fn is_empty(&self) -> bool {
        self.plugin
            .as_ref()
            .and_then(esplugin::Plugin::record_and_group_count)
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
            .map_or(Ok(0), |p| p.count_override_records().map_err(Into::into))
    }

    pub(crate) fn asset_count(&self) -> usize {
        self.archive_assets.values().fold(0, |acc, e| acc + e.len())
    }

    pub(crate) fn do_assets_overlap(&self, plugin: &Plugin) -> bool {
        do_assets_overlap(&self.archive_assets, &plugin.archive_assets)
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
            escape_ascii(plugin_path)
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
            escape_ascii(plugin_path)
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
                GameType::Fallout4
                    | GameType::Fallout4VR
                    | GameType::SkyrimSE
                    | GameType::SkyrimVR
                    | GameType::Starfield
            ) && extension.eq_ignore_ascii_case("esl")
        }
    } else {
        false
    }
}

pub(crate) fn has_ascii_extension(path: &Path, extension: &str) -> bool {
    path.extension()
        .is_some_and(|e| e.eq_ignore_ascii_case(extension))
}

pub(crate) fn plugins_metadata(
    plugins: &[&Plugin],
) -> Result<Vec<esplugin::PluginMetadata>, PluginDataError> {
    let esplugins: Vec<_> = plugins.iter().filter_map(|p| p.plugin.as_ref()).collect();
    Ok(esplugin::plugins_metadata(&esplugins)?)
}

fn name_string(game_type: GameType, path: &Path) -> Result<String, LoadPluginError> {
    match path.file_name() {
        Some(f) => match f.to_str() {
            Some(f) if game_type == GameType::OpenMW => Ok(f.to_owned()),
            Some(f) => Ok(trim_dot_ghost(f).to_owned()),
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
    if let Some((_, bash_tags)) = description.split_once("{{BASH:") {
        if let Some((bash_tags, _)) = bash_tags.split_once("}}") {
            return bash_tags.split(',').map(|s| s.trim().to_owned()).collect();
        }
    }
    Vec::new()
}

fn extract_version(description: &str) -> Result<Option<String>, Box<RegexImplError>> {
    #[expect(
        clippy::expect_used,
        reason = "Only panics if a hardcoded regex string is invalid"
    )]
    static VERSION_REGEXES: LazyLock<Box<[Regex]>> = LazyLock::new(|| {
        // The string below matches the range of version strings supported by
        // Pseudosem v1.0.1, excluding space separators, as they make version
        // extraction from inside sentences very tricky and have not been seen "in
        // the wild". The last non-capturing group prevents version numbers
        // followed by a comma from matching.
        let pseudosem_regex_str = r"(\d+(?:\.\d+)+(?:[-._:]?[A-Za-z0-9]+)*)(?:[^,]|$)";

        /* There are a few different version formats that can appear in strings
        together, and in order to extract the correct one, they must be searched
        for in order of priority. */
        Box::new([
            /* The string below matches timestamps that use forwardslashes for date
            separators. However, Pseudosem v1.0.1 will only compare the first
            two digits as it does not recognise forwardslashes as separators. */
            case_insensitive_regex(r"(\d{1,2}/\d{1,2}/\d{1,4} \d{1,2}:\d{1,2}:\d{1,2})")
                .expect("Hardcoded version timestamp regex should be valid"),
            case_insensitive_regex(&format!(r"version:?\s{pseudosem_regex_str}"))
                .expect("Hardcoded version-prefixed pseudosem version regex should be valid"),
            case_insensitive_regex(&format!(r"(?:^|v|\s){pseudosem_regex_str}"))
                .expect("Hardcoded pseudosem version regex should be valid"),
            /* The string below matches a number containing one or more
            digits found at the start of the search string or preceded by
            'v' or 'version:. */
            case_insensitive_regex(r"(?:^|v|version:\s*)(\d+)")
                .expect("Hardcoded prefixed version number regex should be valid"),
        ])
    });

    for regex in &*VERSION_REGEXES {
        let version = find_captured_text(regex, description)?;

        if version.is_some() {
            return Ok(version);
        }
    }

    Ok(None)
}

fn find_captured_text(regex: &Regex, text: &str) -> Result<Option<String>, Box<RegexImplError>> {
    let captured_text = regex
        .captures(text)?
        .iter()
        .flat_map(|captures| captures.iter())
        .flatten()
        .skip(1) // Skip the first capture as that's the whole regex.
        .map(|m| m.as_str().trim())
        .find(|v| !v.is_empty())
        .map(str::to_owned);

    Ok(captured_text)
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::tests::ALL_GAME_TYPES;
    use parameterized_test::parameterized_test;

    mod plugin {
        use std::io::Seek;
        use std::io::Write;

        use tempfile::tempdir;

        use crate::tests::{
            BLANK_ESL, BLANK_ESM, BLANK_ESP, BLANK_FULL_ESM, BLANK_MASTER_DEPENDENT_ESM,
            BLANK_MASTER_DEPENDENT_ESP, BLANK_MEDIUM_ESM, BLANK_OVERRIDE_ESP, NON_ASCII_ESM,
            source_plugins_path,
        };

        use super::*;

        fn blank_esm(game_type: GameType) -> &'static str {
            if game_type == GameType::Starfield {
                BLANK_FULL_ESM
            } else {
                BLANK_ESM
            }
        }

        fn blank_master_dependent_esm(game_type: GameType) -> &'static str {
            if game_type == GameType::Starfield {
                "Blank - Override.full.esm"
            } else {
                BLANK_MASTER_DEPENDENT_ESM
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn new_should_trim_ghost_extension_unless_game_is_openmw(game_type: GameType) {
            let tmp_dir = tempdir().unwrap();
            let source_path = source_plugins_path(game_type).join(BLANK_ESP);
            let ghosted_path = tmp_dir.path().join(BLANK_ESP.to_owned() + ".ghost");

            std::fs::copy(source_path, &ghosted_path).unwrap();

            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &ghosted_path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            if game_type == GameType::OpenMW {
                assert_eq!(BLANK_ESP.to_owned() + ".ghost", plugin.name());
            } else {
                assert_eq!(BLANK_ESP, plugin.name());
            }
        }

        #[test]
        fn new_should_handle_non_ascii_filenames_correctly() {
            let tmp_dir = tempdir().unwrap();
            let source_path = source_plugins_path(GameType::Oblivion).join(BLANK_ESM);
            let path = tmp_dir.path().join(NON_ASCII_ESM);

            std::fs::copy(source_path, &path).unwrap();
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn new_with_header_only_scope_should_read_header_data_only(game_type: GameType) {
            let plugin_name = blank_master_dependent_esm(game_type);
            let path = source_plugins_path(game_type).join(plugin_name);

            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            let expected_masters = vec![blank_esm(game_type)];

            assert_eq!(plugin_name, plugin.name());
            assert_eq!(expected_masters, plugin.masters().unwrap());
            if matches!(game_type, GameType::OpenMW | GameType::OblivionRemastered) {
                assert!(!plugin.is_master());
            } else {
                assert!(plugin.is_master());
            }
            assert!(!plugin.is_empty());
            assert!(plugin.version().is_none());

            #[expect(clippy::float_cmp, reason = "float values should be exactly equal")]
            match game_type {
                GameType::Morrowind | GameType::OpenMW => {
                    assert_eq!(1.2, plugin.header_version().unwrap());
                }
                GameType::Oblivion | GameType::OblivionRemastered => {
                    assert_eq!(0.8, plugin.header_version().unwrap());
                }
                GameType::Starfield => assert_eq!(0.96, plugin.header_version().unwrap()),
                _ => assert_eq!(0.94, plugin.header_version().unwrap()),
            }

            assert!(plugin.crc().is_none());
            assert!(!plugin.do_assets_overlap(&plugin));
            assert_eq!(0, plugin.asset_count());
            assert!(!plugin.do_records_overlap(&plugin).unwrap());
            assert_eq!(0, plugin.override_record_count().unwrap());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn new_with_header_only_scope_should_read_version_from_header_description(
            game_type: GameType,
        ) {
            let path = source_plugins_path(game_type).join(blank_esm(game_type));

            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            assert_eq!("5.0", plugin.version().unwrap());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn new_with_header_only_scope_should_not_read_assets(game_type: GameType) {
            let path = source_plugins_path(game_type).join(blank_esm(game_type));

            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            assert!(!plugin.do_assets_overlap(&plugin));
            assert_eq!(0, plugin.asset_count());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn new_with_whole_plugin_scope_should_read_records(game_type: GameType) {
            let plugin_name = blank_master_dependent_esm(game_type);
            let path = source_plugins_path(game_type).join(plugin_name);

            let mut plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::WholePlugin,
            )
            .unwrap();

            let expected_masters = vec![blank_esm(game_type)];

            assert_eq!(plugin_name, plugin.name());
            assert_eq!(expected_masters, plugin.masters().unwrap());
            if matches!(game_type, GameType::OpenMW | GameType::OblivionRemastered) {
                assert!(!plugin.is_master());
            } else {
                assert!(plugin.is_master());
            }
            assert!(!plugin.is_empty());
            assert!(plugin.version().is_none());

            #[expect(clippy::float_cmp, reason = "float values should be exactly equal")]
            match game_type {
                GameType::Morrowind | GameType::OpenMW => {
                    assert_eq!(1.2, plugin.header_version().unwrap());
                }
                GameType::Oblivion | GameType::OblivionRemastered => {
                    assert_eq!(0.8, plugin.header_version().unwrap());
                }
                GameType::Starfield => assert_eq!(0.96, plugin.header_version().unwrap()),
                _ => assert_eq!(0.94, plugin.header_version().unwrap()),
            }

            let expected_crc = match game_type {
                GameType::Morrowind | GameType::OpenMW => 3_317_676_987,
                GameType::Starfield => 1_422_425_298,
                GameType::Oblivion | GameType::OblivionRemastered => 3_759_349_588,
                _ => 3_000_242_590,
            };

            assert_eq!(expected_crc, plugin.crc().unwrap());
            assert!(!plugin.do_assets_overlap(&plugin));
            assert_eq!(0, plugin.asset_count());

            if matches!(game_type, GameType::Morrowind | GameType::OpenMW) {
                let master = Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &source_plugins_path(game_type).join(BLANK_ESM),
                    LoadScope::WholePlugin,
                )
                .unwrap();

                let metadata = plugins_metadata(&[&master]).unwrap();

                plugin.resolve_record_ids(&metadata).unwrap();

                assert_eq!(4, plugin.override_record_count().unwrap());
            } else if game_type == GameType::Starfield {
                let master = Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &source_plugins_path(game_type).join(BLANK_FULL_ESM),
                    LoadScope::WholePlugin,
                )
                .unwrap();

                let metadata = plugins_metadata(&[&master]).unwrap();

                plugin.resolve_record_ids(&metadata).unwrap();

                assert_eq!(1, plugin.override_record_count().unwrap());
            } else {
                assert_eq!(4, plugin.override_record_count().unwrap());
            }
            assert!(plugin.do_records_overlap(&plugin).unwrap());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn new_with_whole_plugin_scope_should_read_assets(game_type: GameType) {
            let data_path = source_plugins_path(game_type);
            let path = data_path.join(BLANK_ESP);

            let mut cache = GameCache::default();
            cache.set_archive_paths(vec![
                data_path.join("Blank.bsa"),
                data_path.join("Blank - Main.ba2"),
            ]);

            let plugin = Plugin::new(game_type, &cache, &path, LoadScope::WholePlugin).unwrap();

            if matches!(
                game_type,
                GameType::Morrowind | GameType::OpenMW | GameType::Starfield
            ) {
                // The Starfield test data doesn't include a BA2 file.
                assert!(!plugin.loads_archive());
                assert_eq!(0, plugin.asset_count());
                assert!(!plugin.do_assets_overlap(&plugin));
            } else {
                assert!(plugin.loads_archive());
                assert_eq!(1, plugin.asset_count());
                assert!(plugin.do_assets_overlap(&plugin));
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn new_with_whole_plugin_scope_should_succeed_for_openmw_plugins(game_type: GameType) {
            let tmp_dir = tempdir().unwrap();

            let data_path = source_plugins_path(game_type);
            let omwgame = tmp_dir.path().join("Blank.omwgame");
            let omwaddon = tmp_dir.path().join("Blank.omwaddon");
            let omwscripts = tmp_dir.path().join("Blank.omwscripts");

            std::fs::copy(data_path.join(blank_esm(game_type)), &omwgame).unwrap();
            std::fs::copy(data_path.join(BLANK_ESP), &omwaddon).unwrap();
            File::create(&omwscripts).unwrap();

            assert!(
                Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &omwgame,
                    LoadScope::WholePlugin
                )
                .is_ok()
            );
            assert!(
                Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &omwaddon,
                    LoadScope::WholePlugin
                )
                .is_ok()
            );

            assert_eq!(
                game_type == GameType::OpenMW,
                Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &omwscripts,
                    LoadScope::WholePlugin
                )
                .is_ok()
            );
        }

        #[test]
        fn new_should_error_if_plugin_does_not_exist() {
            let path = Path::new("missing.esp");
            assert!(!path.exists());

            assert!(
                Plugin::new(
                    GameType::Oblivion,
                    &GameCache::default(),
                    path,
                    LoadScope::HeaderOnly
                )
                .is_err()
            );
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_master_should_be_false_for_a_non_master_plugin(game_type: GameType) {
            let path = source_plugins_path(game_type).join(BLANK_ESP);
            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            assert!(!plugin.is_master());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_light_plugin_should_be_true_for_a_plugin_with_esl_extension_for_fo4_and_later(
            game_type: GameType,
        ) {
            let tmp_dir = tempdir().unwrap();
            let data_path = source_plugins_path(game_type);

            let light_path = tmp_dir.path().join(BLANK_ESL);
            std::fs::copy(data_path.join(BLANK_ESP), &light_path).unwrap();

            let master = Plugin::new(
                game_type,
                &GameCache::default(),
                &data_path.join(blank_esm(game_type)),
                LoadScope::HeaderOnly,
            )
            .unwrap();
            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &data_path.join(BLANK_ESP),
                LoadScope::HeaderOnly,
            )
            .unwrap();
            let light = Plugin::new(
                game_type,
                &GameCache::default(),
                &light_path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            assert!(!master.is_light_plugin());
            assert!(!plugin.is_light_plugin());

            if matches!(
                game_type,
                GameType::Fallout4
                    | GameType::Fallout4VR
                    | GameType::SkyrimSE
                    | GameType::SkyrimVR
                    | GameType::Starfield
            ) {
                assert!(light.is_light_plugin());
            } else {
                assert!(!light.is_light_plugin());
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_medium_plugin_should_be_true_for_a_medium_flagged_plugin_for_starfield(
            game_type: GameType,
        ) {
            let tmp_dir = tempdir().unwrap();

            let data_path = source_plugins_path(game_type);
            let path = tmp_dir.path().join(BLANK_MEDIUM_ESM);
            if game_type == GameType::Starfield {
                std::fs::copy(data_path.join(BLANK_MEDIUM_ESM), &path).unwrap();
            } else {
                std::fs::copy(data_path.join(BLANK_ESM), &path).unwrap();

                let mut file = std::fs::File::options().write(true).open(&path).unwrap();
                file.seek(std::io::SeekFrom::Start(9)).unwrap();
                file.write_all(&[0x4]).unwrap();
            }

            let master = Plugin::new(
                game_type,
                &GameCache::default(),
                &data_path.join(blank_esm(game_type)),
                LoadScope::HeaderOnly,
            )
            .unwrap();
            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            assert!(!master.is_medium_plugin());
            assert_eq!(game_type == GameType::Starfield, plugin.is_medium_plugin());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_update_plugin_should_be_true_for_an_update_plugin_for_starfield(game_type: GameType) {
            let tmp_dir = tempdir().unwrap();

            let source_name = if game_type == GameType::Starfield {
                BLANK_OVERRIDE_ESP
            } else {
                BLANK_MASTER_DEPENDENT_ESP
            };
            let data_path = source_plugins_path(game_type);
            let path = tmp_dir.path().join("Blank - Update.esp");
            std::fs::copy(data_path.join(source_name), &path).unwrap();

            let mut file = std::fs::File::options().write(true).open(&path).unwrap();
            file.seek(std::io::SeekFrom::Start(9)).unwrap();
            file.write_all(&[0x2]).unwrap();

            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &source_plugins_path(game_type).join(BLANK_ESP),
                LoadScope::HeaderOnly,
            )
            .unwrap();
            let update = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::HeaderOnly,
            )
            .unwrap();

            assert!(!plugin.is_update_plugin());
            assert_eq!(game_type == GameType::Starfield, update.is_update_plugin());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_blueprint_plugin_should_be_true_for_a_blueprint_plugin_for_starfield(
            game_type: GameType,
        ) {
            let blueprint_plugin_name = if game_type == GameType::Starfield {
                BLANK_OVERRIDE_ESP
            } else {
                BLANK_MASTER_DEPENDENT_ESP
            };
            let data_path = source_plugins_path(game_type);

            let plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &source_plugins_path(game_type).join(BLANK_ESP),
                LoadScope::HeaderOnly,
            )
            .unwrap();
            let update = Plugin::new(
                game_type,
                &GameCache::default(),
                &data_path.join(blueprint_plugin_name),
                LoadScope::HeaderOnly,
            )
            .unwrap();

            assert!(!plugin.is_update_plugin());
            assert_eq!(game_type == GameType::Starfield, update.is_update_plugin());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_valid_as_light_plugin_should_be_true_only_for_a_skyrim_fallout4_or_starfield_plugin_with_new_formids_in_the_valid_range(
            game_type: GameType,
        ) {
            let path = source_plugins_path(game_type).join(BLANK_ESP);
            let mut plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::WholePlugin,
            )
            .unwrap();

            if game_type == GameType::Starfield {
                let master = Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &source_plugins_path(game_type).join(BLANK_FULL_ESM),
                    LoadScope::WholePlugin,
                )
                .unwrap();

                let metadata = plugins_metadata(&[&master]).unwrap();

                plugin.resolve_record_ids(&metadata).unwrap();
            }

            let result = plugin.is_valid_as_light_plugin().unwrap();

            if matches!(
                game_type,
                GameType::Fallout4
                    | GameType::Fallout4VR
                    | GameType::SkyrimSE
                    | GameType::SkyrimVR
                    | GameType::Starfield
            ) {
                assert!(result);
            } else {
                assert!(!result);
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_valid_as_medium_plugin_should_be_true_only_for_a_starfield_plugin_with_new_formids_in_the_valid_range(
            game_type: GameType,
        ) {
            let path = source_plugins_path(game_type).join(BLANK_ESP);
            let mut plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::WholePlugin,
            )
            .unwrap();

            if game_type == GameType::Starfield {
                let master = Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &source_plugins_path(game_type).join(BLANK_FULL_ESM),
                    LoadScope::WholePlugin,
                )
                .unwrap();

                let metadata = plugins_metadata(&[&master]).unwrap();

                plugin.resolve_record_ids(&metadata).unwrap();
            }

            let result = plugin.is_valid_as_medium_plugin().unwrap();

            if game_type == GameType::Starfield {
                assert!(result);
            } else {
                assert!(!result);
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn is_valid_as_update_plugin_should_be_true_only_for_a_starfield_plugin_with_no_new_records(
            game_type: GameType,
        ) {
            let plugin_name = blank_master_dependent_esm(game_type);
            let path = source_plugins_path(game_type).join(plugin_name);
            let mut plugin = Plugin::new(
                game_type,
                &GameCache::default(),
                &path,
                LoadScope::WholePlugin,
            )
            .unwrap();

            if game_type == GameType::Starfield {
                let master = Plugin::new(
                    game_type,
                    &GameCache::default(),
                    &source_plugins_path(game_type).join(BLANK_FULL_ESM),
                    LoadScope::WholePlugin,
                )
                .unwrap();

                let metadata = plugins_metadata(&[&master]).unwrap();

                plugin.resolve_record_ids(&metadata).unwrap();
            }

            let result = plugin.is_valid_as_update_plugin().unwrap();

            if game_type == GameType::Starfield {
                assert!(result);
            } else {
                assert!(!result);
            }
        }
    }

    mod has_plugin_file_extension {
        use super::*;

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_be_true_if_file_ends_in_dot_esp_or_dot_esm(game_type: GameType) {
            assert!(has_plugin_file_extension(game_type, Path::new("file.esp")));
            assert!(has_plugin_file_extension(game_type, Path::new("file.esm")));
            assert!(!has_plugin_file_extension(game_type, Path::new("file.bsa")));
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_be_true_if_file_ends_in_dot_esl_and_game_is_fo4_or_later(game_type: GameType) {
            let result = has_plugin_file_extension(game_type, Path::new("file.esl"));
            if matches!(
                game_type,
                GameType::Fallout4
                    | GameType::SkyrimSE
                    | GameType::Fallout4VR
                    | GameType::SkyrimVR
                    | GameType::Starfield
            ) {
                assert!(result);
            } else {
                assert!(!result);
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_trim_ghost_extension_unless_game_is_openmw(game_type: GameType) {
            if game_type == GameType::OpenMW {
                assert!(!has_plugin_file_extension(
                    game_type,
                    Path::new("file.esp.ghost")
                ));
                assert!(!has_plugin_file_extension(
                    game_type,
                    Path::new("file.esm.ghost")
                ));
            } else {
                assert!(has_plugin_file_extension(
                    game_type,
                    Path::new("file.esp.ghost")
                ));
                assert!(has_plugin_file_extension(
                    game_type,
                    Path::new("file.esm.ghost")
                ));
            }
            assert!(!has_plugin_file_extension(
                game_type,
                Path::new("file.bsa.ghost")
            ));
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_recognise_openmw_plugin_extensions(game_type: GameType) {
            if game_type == GameType::OpenMW {
                assert!(has_plugin_file_extension(
                    game_type,
                    Path::new("file.omwgame")
                ));
                assert!(has_plugin_file_extension(
                    game_type,
                    Path::new("file.omwaddon")
                ));
                assert!(has_plugin_file_extension(
                    game_type,
                    Path::new("file.omwscripts")
                ));
            } else {
                assert!(!has_plugin_file_extension(
                    game_type,
                    Path::new("file.omwgame")
                ));
                assert!(!has_plugin_file_extension(
                    game_type,
                    Path::new("file.omwaddon")
                ));
                assert!(!has_plugin_file_extension(
                    game_type,
                    Path::new("file.omwscripts")
                ));
            }
        }
    }

    #[test]
    fn extract_bash_tags_should_extract_tags_from_plugin_description_text() {
        let text = "Unofficial Skyrim Special Edition Patch

A comprehensive bugfixing mod for The Elder Scrolls V: Skyrim - Special Edition

Version: 4.1.4

Requires Skyrim Special Edition 1.5.39 or greater.

{{BASH:C.Climate,C.Encounter,C.ImageSpace,C.Light,C.Location,C.Music,C.Name,C.Owner,C.Water,Delev,Graphics,Invent,Names,Relev,Sound,Stats}}";

        let tags = extract_bash_tags(text);

        assert_eq!(
            vec![
                "C.Climate".to_owned(),
                "C.Encounter".into(),
                "C.ImageSpace".into(),
                "C.Light".into(),
                "C.Location".into(),
                "C.Music".into(),
                "C.Name".into(),
                "C.Owner".into(),
                "C.Water".into(),
                "Delev".into(),
                "Graphics".into(),
                "Invent".into(),
                "Names".into(),
                "Relev".into(),
                "Sound".into(),
                "Stats".into(),
            ],
            tags
        );
    }

    mod extract_version {
        use crate::plugin::extract_version;

        #[test]
        fn should_extract_a_version_containing_a_single_digit() {
            assert_eq!("5", extract_version("5").unwrap().unwrap());
        }

        #[test]
        fn should_extract_a_version_containing_multiple_digits() {
            assert_eq!("10", extract_version("10").unwrap().unwrap());
        }

        #[test]
        fn should_extract_a_version_containing_multiple_numbers() {
            assert_eq!(
                "10.11.12.13",
                extract_version("10.11.12.13").unwrap().unwrap()
            );
        }

        #[test]
        fn should_extract_a_semantic_version() {
            assert_eq!(
                "1.0.0-x.7.z.92",
                extract_version("1.0.0-x.7.z.92+exp.sha.5114f85")
                    .unwrap()
                    .unwrap()
            );
        }

        #[test]
        fn should_extract_a_pseudosem_extended_version_stopping_at_the_first_space_separator() {
            assert_eq!(
                "01.0.0_alpha:1-2",
                extract_version("01.0.0_alpha:1-2 3").unwrap().unwrap()
            );
        }

        #[test]
        fn should_extract_a_version_substring() {
            assert_eq!("5.0", extract_version("v5.0").unwrap().unwrap());
        }

        #[test]
        fn should_return_none_if_the_string_contains_no_version() {
            assert!(
                extract_version("The quick brown fox jumped over the lazy dog.")
                    .unwrap()
                    .is_none()
            );
        }

        #[test]
        fn should_extract_a_timestamp_with_forwardslash_date_separators() {
            // Found in a Bashed Patch. Though the timestamp isn't useful to
            // LOOT, it is semantically a version, and extracting it is far
            // easier than trying to skip it and the number of records changed.
            assert_eq!(
                "10/09/2016 13:15:18",
                extract_version("Updated: 10/09/2016 13:15:18\r\n\r\nRecords Changed: 43")
                    .unwrap()
                    .unwrap()
            );
        }

        #[test]
        fn should_not_extract_trailing_periods() {
            // Found in <https://www.nexusmods.com/fallout4/mods/2955/>.
            assert_eq!("0.2", extract_version("Version 0.2.").unwrap().unwrap());
        }

        #[test]
        fn should_extract_a_version_following_text_and_a_version_colon_string() {
            // Found in <https://www.nexusmods.com/skyrim/mods/71214/>.
            assert_eq!(
                "3.0.0",
                extract_version("Legendary Edition\r\n\r\nVersion: 3.0.0")
                    .unwrap()
                    .unwrap()
            );
        }

        #[test]
        fn should_ignore_numbers_containing_commas() {
            // Found in <https://www.nexusmods.com/oblivion/mods/5296/>.
            assert_eq!(
                "3.5.3",
                extract_version("fixing over 2,300 bugs so far! Version: 3.5.3")
                    .unwrap()
                    .unwrap()
            );
        }

        #[test]
        fn should_extract_a_version_before_text() {
            // Found in <https://www.nexusmods.com/fallout3/mods/19122/>.
            assert_eq!(
                "2.1",
                extract_version("Version: 2.1 The Unofficial Fallout 3 Patch")
                    .unwrap()
                    .unwrap()
            );
        }

        #[test]
        fn should_extract_a_version_with_a_preceding_v() {
            // Found in <https://www.nexusmods.com/oblivion/mods/22795/>.
            assert_eq!(
                "2.11",
                extract_version("V2.11\r\n\r\n{{BASH:Invent}}")
                    .unwrap()
                    .unwrap()
            );
        }

        #[test]
        fn should_extract_a_version_preceded_by_colon_period_whitespace() {
            // Found in <https://www.nexusmods.com/oblivion/mods/45570>.
            assert_eq!("1.09", extract_version("Version:. 1.09").unwrap().unwrap());
        }

        #[test]
        fn should_extract_a_version_with_letters_immediately_after_numbers() {
            // Found in <https://www.nexusmods.com/skyrim/mods/19>.
            assert_eq!("2.1.3b", extract_version("comprehensive bugfixing mod for The Elder Scrolls V: Skyrim\r\n\r\nVersion: 2.1.3b\r\n\r\n").unwrap().unwrap());
        }

        #[test]
        fn should_extract_a_version_with_period_and_no_preceding_identifier() {
            // Found in <https://www.nexusmods.com/skyrim/mods/3863>.
            assert_eq!("5.1", extract_version("SkyUI 5.1").unwrap().unwrap());
        }

        #[test]
        fn should_not_extract_a_single_digit_in_a_sentence() {
            // Found in <https://www.nexusmods.com/skyrim/mods/4708>.
            assert!(
                extract_version(
                    "Adds 8 variants of Triss Merigold's outfit from \"The Witcher 2\""
                )
                .unwrap()
                .is_none()
            );
        }

        #[test]
        fn should_prefer_version_prefixed_numbers_over_versions_in_sentence() {
            // Found in <https://www.nexusmods.com/skyrim/mods/47327>
            assert_eq!("2.0.0", extract_version("Requires Skyrim patch 1.9.32.0.8 or greater.\nRequires Unofficial Skyrim Legendary Edition Patch 3.0.0 or greater.\nVersion 2.0.0").unwrap().unwrap());
        }

        #[test]
        fn should_extract_a_version_that_is_a_single_digit_preceded_by_v() {
            // Found in <https://www.nexusmods.com/skyrim/mods/19733>
            assert_eq!(
                "8",
                extract_version("Immersive Armors v8 Main Plugin")
                    .unwrap()
                    .unwrap()
            );
        }

        #[test]
        fn should_prefer_version_prefixed_numbers_over_v_prefixed_number() {
            // Found in <https://www.nexusmods.com/skyrim/mods/43773>
            assert_eq!("1.0", extract_version("Compatibility patch for AOS v2.5 and True Storms v1.5 (or later),\nPatch Version: 1.0").unwrap().unwrap());
        }

        #[test]
        fn should_extract_a_version_that_is_a_single_digit_after_version_colon_space() {
            // Found in <https://www.nexusmods.com/oblivion/mods/14720>
            assert_eq!(
                "2",
                extract_version("Version: 2 {{BASH:C.Water}}")
                    .unwrap()
                    .unwrap()
            );
        }
    }
}
