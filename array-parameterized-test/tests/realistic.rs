use std::path::Path;

use array_parameterized_test::{parameterized_test, test_parameter};

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[non_exhaustive]
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

fn has_ascii_extension(path: &Path, extension: &str) -> bool {
    path.extension()
        .is_some_and(|e| e.eq_ignore_ascii_case(extension))
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

#[cfg(test)]
mod tests {
    use super::*;
    #[test_parameter]
    const ALL_GAME_TYPES: [GameType; 12] = [
        GameType::Oblivion,
        GameType::Skyrim,
        GameType::Fallout3,
        GameType::FalloutNV,
        GameType::Fallout4,
        GameType::SkyrimSE,
        GameType::Fallout4VR,
        GameType::SkyrimVR,
        GameType::Morrowind,
        GameType::Starfield,
        GameType::OpenMW,
        GameType::OblivionRemastered,
    ];

    #[parameterized_test(ALL_GAME_TYPES)]
    fn should_be_true_if_file_ends_in_dot_esp_or_dot_esm(game_type: GameType) {
        assert!(has_plugin_file_extension(game_type, Path::new("file.esp")));
        assert!(has_plugin_file_extension(game_type, Path::new("file.esm")));
        assert!(!has_plugin_file_extension(game_type, Path::new("file.bsa")));
    }
}
