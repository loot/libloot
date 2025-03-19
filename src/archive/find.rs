use std::{
    fs::OpenOptions,
    path::{Path, PathBuf},
};

use crate::{GameType, game::GameCache, plugin::has_ascii_extension};

const BSA_FILE_EXTENSION: &str = "bsa";

pub fn find_associated_archives(
    game_type: GameType,
    game_cache: &GameCache,
    plugin_path: &Path,
) -> Vec<PathBuf> {
    match game_type {
        GameType::TES3 | GameType::OpenMW => Vec::new(),

        // Skyrim (non-SE) plugins can only load BSAs that have exactly the same
        // basename, ignoring file extensions.
        GameType::TES5 => find_associated_archive(plugin_path),

        // Skyrim SE can load BSAs that have exactly the same basename, ignoring
        // file extensions, and also BSAs with filenames of the form "<basename>
        // - Textures.bsa" (case-insensitively). This assumes that Skyrim VR
        // works the same way as Skyrim SE.
        GameType::TES5SE | GameType::TES5VR => find_associated_archives_with_suffixes(plugin_path, BSA_FILE_EXTENSION, &["", " - Textures"]),

        // Oblivion .esp files can load archives which begin with the plugin
        // basename.
        GameType::TES4 => {
            if has_ascii_extension(plugin_path, "esp") {
                Vec::new()
            } else {
                find_associated_archives_with_arbitrary_suffixes(plugin_path, game_cache)
            }
        },

        // FO3, FNV, FO4 plugins can load archives which begin with the plugin
        // basename. This assumes that FO4 VR works the same way as FO4.
        GameType::FO3 | GameType::FONV | GameType::FO4 | GameType::FO4VR =>
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
    let file_stem = match plugin_path.file_stem() {
        Some(s) => s,
        None => return Vec::new(),
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

    game_cache
        .archives()
        .filter(|path| {
            // Need to check if it starts with the given plugin's basename,
            // but case insensitively. This is hard to do accurately, so
            // instead check if the plugin with the same length basename and
            // and the given plugin's file extension is equivalent.
            let archive_filename = match path.file_name().and_then(|s| s.to_str()) {
                Some(f) => f,
                None => return false,
            };

            // Can't just slice the archive filename to the same length as the plugin file stem directly because that might not slice on a character boundary, so truncate the byte slice and then check it's still valid UTF-8.
            let filename =
                match std::str::from_utf8(&archive_filename.as_bytes()[..plugin_stem_len]) {
                    Ok(f) => f,
                    Err(_) => return false,
                };

            let archive_plugin_path = plugin_path.with_file_name(filename);

            are_file_paths_equivalent(&archive_plugin_path, plugin_path)
        })
        .cloned()
        .collect()
}

#[cfg(windows)]
fn are_file_paths_equivalent(lhs: &Path, rhs: &Path) -> bool {
    if lhs == rhs {
        return true;
    }

    // See <https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499->
    // Or windows::Win32::Foundation::ERROR_SHARING_VIOLATION in the "windows" crate.
    const ERROR_SHARING_VIOLATION: i32 = 32;
    use std::os::windows::fs::OpenOptionsExt;

    let lhs_file = match OpenOptions::new().read(true).share_mode(0).open(lhs) {
        Ok(f) => f,
        Err(_) => return false,
    };

    let result = match OpenOptions::new().read(true).share_mode(0).open(rhs) {
        Ok(_) => {
            false
        }
        Err(e) => {
            if let Some(error_code) = e.raw_os_error() {
                error_code == ERROR_SHARING_VIOLATION
            } else {
                false
            }
        }
    };

    drop(lhs_file);

    result
}

#[cfg(not(windows))]
fn are_file_paths_equivalent(lhs: &Path, rhs: &Path) -> bool {
    if lhs == rhs {
        return true;
    }

    use std::fs::unix::fs::MetadataExt;

    let lhs_metadata = match lhs.metadata() {
        Ok(m) => m,
        _ => return false,
    };

    let rhs_metadata = match rhs.metadata() {
        Ok(m) => m,
        _ => return false,
    };

    lhs_metadata.dev() == rhs_metadata.dev() && lhs_metadata.ino() == rhs_metadata.ino()
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

    use super::*;

    #[test]
    fn are_file_paths_equivalent_should_be_true_if_given_the_same_path_twice() {
        let temp_dir = tempdir().unwrap();
        let file_path = temp_dir.path().join("test");
        std::fs::write(&file_path, "").unwrap();

        assert!(are_file_paths_equivalent(&file_path, &file_path))
    }
}
