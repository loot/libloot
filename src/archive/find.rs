use std::path::{Path, PathBuf};

#[cfg(windows)]
use windows::Win32::Storage::FileSystem::BY_HANDLE_FILE_INFORMATION;

use crate::{GameType, game::GameCache, plugin::has_ascii_extension};

const BSA_FILE_EXTENSION: &str = "bsa";

pub fn find_associated_archives(
    game_type: GameType,
    game_cache: &GameCache,
    plugin_path: &Path,
) -> Vec<PathBuf> {
    match game_type {
        GameType::Morrowind | GameType::OpenMW => Vec::new(),

        // Skyrim (non-SE) plugins can only load BSAs that have exactly the same
        // basename, ignoring file extensions.
        GameType::Skyrim => find_associated_archive(plugin_path),

        // Skyrim SE can load BSAs that have exactly the same basename, ignoring
        // file extensions, and also BSAs with filenames of the form "<basename>
        // - Textures.bsa" (case-insensitively). This assumes that Skyrim VR
        // works the same way as Skyrim SE.
        GameType::SkyrimSE | GameType::SkyrimVR => find_associated_archives_with_suffixes(plugin_path, BSA_FILE_EXTENSION, &["", " - Textures"]),

        // Oblivion .esp files can load archives which begin with the plugin
        // basename.
        GameType::Oblivion | GameType::OblivionRemastered => {
            if has_ascii_extension(plugin_path, "esp") {
                find_associated_archives_with_arbitrary_suffixes(plugin_path, game_cache)
            } else {
                Vec::new()
            }
        },

        // FO3, FNV, FO4 plugins can load archives which begin with the plugin
        // basename. This assumes that FO4 VR works the same way as FO4.
        GameType::Fallout3 | GameType::FalloutNV | GameType::Fallout4 | GameType::Fallout4VR =>
            find_associated_archives_with_arbitrary_suffixes(plugin_path, game_cache)
        ,

        // The game will load a BA2 that's suffixed with " - Voices_<language>"
        // where <language> is whatever language Starfield is configured to use
        // (sLanguage in the ini), so this isn't exactly correct but will work
        // so long as a plugin with voices has voices for English, which seems
        // likely.
        GameType::Starfield => find_associated_archives_with_suffixes(plugin_path, "ba2", &[" - Main", " - Textures", " - Localization", " - Voices_en"]),
    }
}

fn find_associated_archive(plugin_path: &Path) -> Vec<PathBuf> {
    let archive_path = plugin_path.with_extension(BSA_FILE_EXTENSION);

    if archive_path.exists() {
        vec![archive_path]
    } else {
        Vec::new()
    }
}

fn find_associated_archives_with_suffixes(
    plugin_path: &Path,
    archive_extension: &str,
    supported_suffixes: &[&str],
) -> Vec<PathBuf> {
    let Some(file_stem) = plugin_path.file_stem() else {
        return Vec::new();
    };

    supported_suffixes
        .iter()
        .map(|suffix| {
            let mut filename = file_stem.to_os_string();
            filename.push(suffix);
            filename.push(".");
            filename.push(archive_extension);

            plugin_path.with_file_name(filename)
        })
        .filter(|p| p.exists())
        .collect()
}

fn find_associated_archives_with_arbitrary_suffixes(
    plugin_path: &Path,
    game_cache: &GameCache,
) -> Vec<PathBuf> {
    let plugin_stem_len = match plugin_path.file_stem().and_then(|s| s.to_str()) {
        Some(s) => s.len(),
        None => return Vec::new(),
    };
    let Some(plugin_extension) = plugin_path.extension() else {
        return Vec::new();
    };

    game_cache
        .archives_iter()
        .filter(|path| {
            // Need to check if it starts with the given plugin's basename,
            // but case insensitively. This is hard to do accurately, so
            // instead check if the plugin with the same length basename and
            // and the given plugin's file extension is equivalent.
            let Some(archive_filename) = path.file_name().and_then(|s| s.to_str()) else {
                return false;
            };

            // Can't just slice the archive filename to the same length as the plugin file stem directly because that might not slice on a character boundary, so truncate the byte slice and then check it's still valid UTF-8.
            if archive_filename.len() < plugin_stem_len {
                return false;
            }

            let Some(filename) = archive_filename.get(..plugin_stem_len) else {
                return false;
            };

            let archive_plugin_path = plugin_path
                .with_file_name(filename)
                .with_extension(plugin_extension);

            are_file_paths_equivalent(&archive_plugin_path, plugin_path)
        })
        .cloned()
        .collect()
}

#[cfg(windows)]
fn are_file_paths_equivalent(lhs: &Path, rhs: &Path) -> bool {
    use std::fs::File;

    if lhs == rhs {
        return true;
    }

    let Ok(lhs_file) = File::open(lhs) else {
        return false;
    };

    let Ok(rhs_file) = File::open(rhs) else {
        return false;
    };

    let Some(lhs_info) = get_file_info(&lhs_file) else {
        return false;
    };

    let Some(rhs_info) = get_file_info(&rhs_file) else {
        return false;
    };

    lhs_info.dwVolumeSerialNumber == rhs_info.dwVolumeSerialNumber
        && lhs_info.nFileIndexHigh == rhs_info.nFileIndexHigh
        && lhs_info.nFileIndexLow == rhs_info.nFileIndexLow
}

#[cfg(windows)]
fn get_file_info(file: &std::fs::File) -> Option<BY_HANDLE_FILE_INFORMATION> {
    use std::os::windows::io::AsRawHandle;
    use windows::Win32::{Foundation::HANDLE, Storage::FileSystem::GetFileInformationByHandle};

    let mut info = BY_HANDLE_FILE_INFORMATION::default();

    // SAFETY: This is safe because the file handles and the info struct pointers are all valid until this function exits.
    #[expect(
        unsafe_code,
        reason = "There is currently no way to get this data safely"
    )]
    unsafe {
        GetFileInformationByHandle(HANDLE(file.as_raw_handle()), &mut info)
            .is_ok()
            .then_some(info)
    }
}

#[cfg(not(windows))]
fn are_file_paths_equivalent(lhs: &Path, rhs: &Path) -> bool {
    use std::os::unix::fs::MetadataExt;

    if lhs == rhs {
        return true;
    }

    let Ok(lhs_metadata) = lhs.metadata() else {
        return false;
    };

    let Ok(rhs_metadata) = rhs.metadata() else {
        return false;
    };

    lhs_metadata.dev() == rhs_metadata.dev() && lhs_metadata.ino() == rhs_metadata.ino()
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

    use super::*;

    mod find_associated_archives {
        use std::path::absolute;

        use parameterized_test::parameterized_test;
        use tempfile::TempDir;

        use super::*;

        use crate::tests::{
            ALL_GAME_TYPES, BLANK_DIFFERENT_ESM, BLANK_DIFFERENT_ESP, BLANK_ESM, BLANK_ESP,
            BLANK_MASTER_DEPENDENT_ESM, copy_file, source_plugins_path,
        };

        const NON_ASCII_ESP: &str = "non\u{00C1}scii.esp";

        struct Fixture {
            _temp_dir: TempDir,
            cache: GameCache,
            data_path: PathBuf,
        }

        impl Fixture {
            pub fn new(game_type: GameType) -> Self {
                let tmp_dir = tempdir().unwrap();

                let mut cache = GameCache::default();

                let data_path = tmp_dir.path().to_path_buf();

                match game_type {
                    GameType::Morrowind | GameType::OpenMW => {}
                    GameType::Fallout4 | GameType::Fallout4VR | GameType::Starfield => {
                        let source = absolute("./testing-plugins/Fallout 4/Data").unwrap();
                        copy_file(&source, &data_path, "Blank - Main.ba2");
                        copy_file(&source, &data_path, "Blank - Textures.ba2");
                        std::fs::copy(
                            source.join("Blank - Main.ba2"),
                            data_path.join("non\u{00C1}scii.ba2"),
                        )
                        .unwrap();
                        std::fs::copy(
                            source.join("Blank - Main.ba2"),
                            data_path.join("Blank - Different - Suffix.ba2"),
                        )
                        .unwrap();

                        cache.set_archive_paths(vec![
                            data_path.join("Blank - Main.ba2"),
                            data_path.join("Blank - Textures.ba2"),
                            data_path.join("non\u{00C1}scii.ba2"),
                            data_path.join("Blank - Different - Main.ba2"),
                        ]);
                    }
                    _ => {
                        let source = source_plugins_path(game_type);
                        copy_file(&source, &data_path, "Blank.bsa");
                        std::fs::copy(
                            source.join("Blank.bsa"),
                            data_path.join("non\u{00C1}scii.bsa"),
                        )
                        .unwrap();
                        std::fs::copy(
                            source.join("Blank.bsa"),
                            data_path.join("Blank - Different - Main.bsa"),
                        )
                        .unwrap();

                        cache.set_archive_paths(vec![
                            data_path.join("Blank.bsa"),
                            data_path.join("non\u{00C1}scii.bsa"),
                            data_path.join("Blank - Different - Suffix.bsa"),
                        ]);
                    }
                }

                Self {
                    _temp_dir: tmp_dir,
                    data_path,
                    cache,
                }
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_return_empty_vec_if_no_matching_archives_are_found(game_type: GameType) {
            let fixture = Fixture::new(game_type);

            let archives = find_associated_archives(
                game_type,
                &fixture.cache,
                &fixture.data_path.join(BLANK_MASTER_DEPENDENT_ESM),
            );

            assert!(archives.is_empty());
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_find_an_archive_that_exactly_matches_an_esm_file_basename_except_for_morrowind_and_oblivion(
            game_type: GameType,
        ) {
            let fixture = Fixture::new(game_type);

            let archives = find_associated_archives(
                game_type,
                &fixture.cache,
                &fixture.data_path.join(BLANK_ESM),
            );

            if matches!(
                game_type,
                GameType::Morrowind
                    | GameType::OpenMW
                    | GameType::Oblivion
                    | GameType::OblivionRemastered
            ) {
                assert!(archives.is_empty());
            } else {
                assert!(!archives.is_empty());
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_find_an_archive_that_exactly_matches_a_non_ascii_esp_file_basename_except_for_morrowind_and_starfield(
            game_type: GameType,
        ) {
            let fixture = Fixture::new(game_type);

            let archives = find_associated_archives(
                game_type,
                &fixture.cache,
                &fixture.data_path.join(NON_ASCII_ESP),
            );

            if matches!(
                game_type,
                GameType::Morrowind | GameType::OpenMW | GameType::Starfield
            ) {
                assert!(archives.is_empty());
            } else {
                assert!(!archives.is_empty());
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_find_an_archive_that_starts_with_an_esp_file_basename_except_for_morrowind_and(
            game_type: GameType,
        ) {
            let fixture = Fixture::new(game_type);

            let archives = find_associated_archives(
                game_type,
                &fixture.cache,
                &fixture.data_path.join(BLANK_ESP),
            );

            if matches!(game_type, GameType::Morrowind | GameType::OpenMW) {
                assert!(archives.is_empty());
            } else {
                assert!(!archives.is_empty());
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_find_an_archive_that_starts_with_an_esm_file_basename_only_for_fallout(
            game_type: GameType,
        ) {
            let fixture = Fixture::new(game_type);

            let archives = find_associated_archives(
                game_type,
                &fixture.cache,
                &fixture.data_path.join(BLANK_DIFFERENT_ESM),
            );

            if matches!(
                game_type,
                GameType::Fallout3
                    | GameType::FalloutNV
                    | GameType::Fallout4
                    | GameType::Fallout4VR
            ) {
                assert!(!archives.is_empty());
            } else {
                assert!(archives.is_empty());
            }
        }

        #[parameterized_test(ALL_GAME_TYPES)]
        fn should_find_an_archive_that_starts_with_an_esp_file_basename_only_for_oblivion_and_fallout(
            game_type: GameType,
        ) {
            let fixture = Fixture::new(game_type);

            let archives = find_associated_archives(
                game_type,
                &fixture.cache,
                &fixture.data_path.join(BLANK_DIFFERENT_ESP),
            );

            if matches!(
                game_type,
                GameType::Oblivion
                    | GameType::OblivionRemastered
                    | GameType::Fallout3
                    | GameType::FalloutNV
                    | GameType::Fallout4
                    | GameType::Fallout4VR
            ) {
                assert!(!archives.is_empty());
            } else {
                assert!(archives.is_empty());
            }
        }
    }

    mod are_file_paths_equivalent {
        use super::*;

        #[test]
        fn should_be_true_if_given_equal_paths_that_exist() {
            let file_path = Path::new("README.md");

            assert!(file_path.exists());
            assert!(are_file_paths_equivalent(file_path, file_path));
        }

        #[test]
        fn should_be_true_if_given_equal_paths_that_do_not_exist() {
            let file_path = Path::new("missing");

            assert!(!file_path.exists());
            assert!(are_file_paths_equivalent(file_path, file_path));
        }

        #[test]
        fn should_be_false_if_given_case_insensitively_equal_paths_that_do_not_exist() {
            let file_path1 = Path::new("missing");
            let file_path2 = Path::new("MISSING");

            assert!(!file_path1.exists());
            assert!(!file_path2.exists());
            assert!(!are_file_paths_equivalent(file_path1, file_path2));
        }

        #[test]
        fn should_be_false_if_given_case_insensitively_unequal_paths_that_exist() {
            let file_path1 = Path::new("README.md");
            let file_path2 = Path::new("LICENSE");

            assert!(file_path1.exists());
            assert!(file_path2.exists());
            assert!(!are_file_paths_equivalent(file_path1, file_path2));
        }

        #[test]
        #[cfg(windows)]
        fn should_be_true_if_given_case_insensitively_equal_paths_that_exist() {
            let file_path1 = Path::new("README.md");
            let file_path2 = Path::new("readme.md");

            assert!(file_path1.exists());
            assert!(file_path2.exists());
            assert!(are_file_paths_equivalent(file_path1, file_path2));
        }

        #[test]
        #[cfg(windows)]
        fn should_be_true_if_equal_paths_have_characters_that_are_unrepresentable_in_the_system_multi_byte_code_page()
         {
            let file_path =
                Path::new("\u{2551}\u{00BB}\u{00C1}\u{2510}\u{2557}\u{00FE}\u{00C3}\u{00CE}.txt");

            assert!(are_file_paths_equivalent(file_path, file_path));
        }

        #[test]
        #[cfg(windows)]
        fn should_be_false_if_case_insensitively_equal_paths_have_characters_that_are_unrepresentable_in_the_system_multi_byte_code_page_and_do_not_exist()
         {
            let file_path1 =
                Path::new("\u{2551}\u{00BB}\u{00C1}\u{2510}\u{2557}\u{00FE}\u{00E3}\u{00CE}.txt");
            let file_path2 =
                Path::new("\u{2551}\u{00BB}\u{00C1}\u{2510}\u{2557}\u{00FE}\u{00C3}\u{00CE}.txt");

            assert!(!are_file_paths_equivalent(file_path1, file_path2));
        }

        #[test]
        #[cfg(not(windows))]
        fn should_be_false_if_given_case_insensitively_equal_paths_that_exist() {
            let tmp_dir = tempdir().unwrap();
            let file_path1 = tmp_dir.path().join("test");
            let file_path2 = tmp_dir.path().join("TEST");

            std::fs::File::create(&file_path1).unwrap();
            std::fs::File::create(&file_path2).unwrap();

            assert!(file_path1.exists());
            assert!(file_path2.exists());
            assert!(!are_file_paths_equivalent(&file_path1, &file_path2));
        }
    }
}
