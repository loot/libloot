use std::{
    fs::{File, copy, create_dir_all},
    path::{Path, PathBuf, absolute},
    time::{Duration, SystemTime},
};

use crate::GameType;
use parameterized_test::test_parameter;
use tempfile::TempDir;

pub const BLANK_ESM: &str = "Blank.esm";
pub const BLANK_DIFFERENT_ESM: &str = "Blank - Different.esm";
pub const BLANK_MASTER_DEPENDENT_ESM: &str = "Blank - Master Dependent.esm";
const BLANK_DIFFERENT_MASTER_DEPENDENT_ESM: &str = "Blank - Different Master Dependent.esm";
pub const BLANK_ESP: &str = "Blank.esp";
pub const BLANK_DIFFERENT_ESP: &str = "Blank - Different.esp";
pub const BLANK_MASTER_DEPENDENT_ESP: &str = "Blank - Master Dependent.esp";
const BLANK_DIFFERENT_MASTER_DEPENDENT_ESP: &str = "Blank - Different Master Dependent.esp";
const BLANK_PLUGIN_DEPENDENT_ESP: &str = "Blank - Plugin Dependent.esp";
const BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP: &str = "Blank - Different Plugin Dependent.esp";

pub const BLANK_FULL_ESM: &str = "Blank.full.esm";
pub const BLANK_MEDIUM_ESM: &str = "Blank.medium.esm";
pub const BLANK_OVERRIDE_ESP: &str = "Blank - Override.esp";
pub const BLANK_ESL: &str = "Blank.esl";
pub const NON_PLUGIN_FILE: &str = "NotAPlugin.esm";
pub const NON_ASCII_ESM: &str = "non\u{00C1}scii.esm";

pub fn source_plugins_path(game_type: GameType) -> PathBuf {
    match game_type {
        GameType::Morrowind | GameType::OpenMW => {
            absolute("./testing-plugins/Morrowind/Data Files")
        }
        GameType::Oblivion | GameType::OblivionRemastered => {
            absolute("./testing-plugins/Oblivion/Data")
        }
        GameType::Starfield => absolute("./testing-plugins/Starfield/Data"),
        GameType::Fallout3 | GameType::FalloutNV | GameType::Skyrim => {
            absolute("./testing-plugins/Skyrim/Data")
        }
        _ => absolute("./testing-plugins/SkyrimSE/Data"),
    }
    .unwrap()
}

fn master_file(game_type: GameType) -> &'static str {
    match game_type {
        GameType::Morrowind | GameType::OpenMW => "Morrowind.esm",
        GameType::Oblivion | GameType::OblivionRemastered => "Oblivion.esm",
        GameType::Skyrim | GameType::SkyrimSE | GameType::SkyrimVR => "Skyrim.esm",
        GameType::Fallout3 => "Fallout3.esm",
        GameType::FalloutNV => "FalloutNV.esm",
        GameType::Fallout4 | GameType::Fallout4VR => "Fallout4.esm",
        GameType::Starfield => "Starfield.esm",
    }
}

pub fn copy_file(source_dir: &Path, dest_dir: &Path, filename: &str) {
    copy(source_dir.join(filename), dest_dir.join(filename)).unwrap();
}

fn touch(file_path: &Path) {
    std::fs::File::create(file_path).unwrap();
}

fn supports_light_plugins(game_type: GameType) -> bool {
    matches!(
        game_type,
        GameType::SkyrimSE
            | GameType::SkyrimVR
            | GameType::Fallout4
            | GameType::Fallout4VR
            | GameType::Starfield
    )
}

fn is_load_order_timestamp_based(game_type: GameType) -> bool {
    matches!(
        game_type,
        GameType::Morrowind | GameType::Oblivion | GameType::Fallout3 | GameType::FalloutNV
    )
}

pub fn initial_load_order(game_type: GameType) -> Vec<(&'static str, bool)> {
    if game_type == GameType::Starfield {
        vec![
            (master_file(game_type), true),
            (BLANK_ESM, true),
            (BLANK_DIFFERENT_ESM, false),
            (BLANK_FULL_ESM, false),
            (BLANK_MASTER_DEPENDENT_ESM, false),
            (BLANK_MEDIUM_ESM, false),
            (BLANK_ESL, false),
            (BLANK_ESP, false),
            (BLANK_DIFFERENT_ESP, false),
            (BLANK_MASTER_DEPENDENT_ESP, false),
        ]
    } else {
        let mut load_order = vec![
            (master_file(game_type), true),
            (BLANK_ESM, true),
            (BLANK_DIFFERENT_ESM, false),
            (BLANK_MASTER_DEPENDENT_ESM, false),
            (BLANK_DIFFERENT_MASTER_DEPENDENT_ESM, false),
            (BLANK_ESP, false),
            (BLANK_DIFFERENT_ESP, false),
            (BLANK_MASTER_DEPENDENT_ESP, false),
            (BLANK_DIFFERENT_MASTER_DEPENDENT_ESP, true),
            (BLANK_PLUGIN_DEPENDENT_ESP, false),
            (BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP, false),
        ];

        if supports_light_plugins(game_type) {
            load_order.insert(5, (BLANK_ESL, false));
        }

        load_order
    }
}

fn set_load_order(
    game_type: GameType,
    data_path: &Path,
    local_path: &Path,
    load_order: &[(&'static str, bool)],
) {
    use std::io::Write;

    match game_type {
        GameType::Morrowind | GameType::OpenMW => {}
        _ => {
            let mut file = File::create(local_path.join("Plugins.txt")).unwrap();
            for (plugin, is_active) in load_order {
                if supports_light_plugins(game_type) {
                    if *is_active {
                        write!(file, "*").unwrap();
                    }
                } else if !is_active {
                    continue;
                }

                writeln!(file, "{plugin}").unwrap();
            }
        }
    }

    if is_load_order_timestamp_based(game_type) {
        let mut mod_time = SystemTime::now();
        for (plugin, _) in load_order {
            let ghosted_path = data_path.join(format!("{plugin}.ghost"));
            let file = if ghosted_path.exists() {
                File::options().write(true).open(ghosted_path)
            } else {
                File::options().write(true).open(data_path.join(plugin))
            };
            file.unwrap().set_modified(mod_time).unwrap();

            mod_time += Duration::from_secs(60);
        }
    } else if matches!(game_type, GameType::Skyrim | GameType::OblivionRemastered) {
        let mut file = File::create(local_path.join("loadorder.txt")).unwrap();
        for (plugin, _) in load_order {
            writeln!(file, "{plugin}").unwrap();
        }
    }
}

fn data_path(game_type: GameType, game_path: &Path) -> PathBuf {
    match game_type {
        GameType::OpenMW => game_path.join("resources/vfs"),
        GameType::Morrowind => game_path.join("Data Files"),
        GameType::OblivionRemastered => {
            game_path.join("OblivionRemastered/Content/Dev/ObvData/Data")
        }
        _ => game_path.join("Data"),
    }
}

pub struct Fixture {
    _temp_dir: TempDir,
    pub(crate) game_type: GameType,
    pub(crate) game_path: PathBuf,
    pub(crate) local_path: PathBuf,
}

impl Fixture {
    pub fn new(game_type: GameType) -> Self {
        let temp_dir = tempfile::Builder::new()
            .prefix("libloot-t\u{00E9}st-")
            .tempdir()
            .unwrap();

        Self::with_tempdir(game_type, temp_dir)
    }

    pub fn in_path(game_type: GameType, in_path: &Path) -> Fixture {
        let temp_dir = tempfile::Builder::new()
            .prefix("libloot-t\u{00E9}st-")
            .tempdir_in(in_path)
            .unwrap();

        Self::with_tempdir(game_type, temp_dir)
    }

    fn with_tempdir(game_type: GameType, temp_dir: TempDir) -> Fixture {
        let root_path = temp_dir.path();
        let game_path = root_path.join("games/game");
        let local_path = root_path.join("local/game");
        let data_path = data_path(game_type, &game_path);

        create_dir_all(&data_path).unwrap();
        create_dir_all(&local_path).unwrap();

        let source_plugins_path = source_plugins_path(game_type);

        if game_type == GameType::Starfield {
            copy_file(&source_plugins_path, &data_path, BLANK_FULL_ESM);
            copy_file(&source_plugins_path, &data_path, BLANK_MEDIUM_ESM);

            copy(
                source_plugins_path.join(BLANK_FULL_ESM),
                data_path.join(BLANK_ESM),
            )
            .unwrap();
            copy(
                source_plugins_path.join(BLANK_FULL_ESM),
                data_path.join(BLANK_DIFFERENT_ESM),
            )
            .unwrap();
            copy(
                source_plugins_path.join("Blank - Override.full.esm"),
                data_path.join(BLANK_MASTER_DEPENDENT_ESM),
            )
            .unwrap();
            copy_file(&source_plugins_path, &data_path, BLANK_ESP);
            copy(
                source_plugins_path.join(BLANK_ESP),
                data_path.join(BLANK_DIFFERENT_ESP),
            )
            .unwrap();
            copy(
                source_plugins_path.join(BLANK_OVERRIDE_ESP),
                data_path.join(BLANK_MASTER_DEPENDENT_ESP),
            )
            .unwrap();
        } else {
            copy_file(&source_plugins_path, &data_path, BLANK_ESM);
            copy_file(&source_plugins_path, &data_path, BLANK_DIFFERENT_ESM);
            copy_file(&source_plugins_path, &data_path, BLANK_MASTER_DEPENDENT_ESM);
            copy_file(
                &source_plugins_path,
                &data_path,
                BLANK_DIFFERENT_MASTER_DEPENDENT_ESM,
            );
            copy_file(&source_plugins_path, &data_path, BLANK_ESP);
            copy_file(&source_plugins_path, &data_path, BLANK_DIFFERENT_ESP);
            copy_file(&source_plugins_path, &data_path, BLANK_MASTER_DEPENDENT_ESP);
            copy_file(
                &source_plugins_path,
                &data_path,
                BLANK_DIFFERENT_MASTER_DEPENDENT_ESP,
            );
            copy_file(&source_plugins_path, &data_path, BLANK_PLUGIN_DEPENDENT_ESP);
            copy_file(
                &source_plugins_path,
                &data_path,
                BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP,
            );
        }

        if supports_light_plugins(game_type) {
            if game_type == GameType::Starfield {
                copy(
                    source_plugins_path.join("Blank.small.esm"),
                    data_path.join(BLANK_ESL),
                )
                .unwrap();
            } else {
                copy_file(&source_plugins_path, &data_path, BLANK_ESL);
            }
        }

        let master_file = master_file(game_type);
        copy(data_path.join(BLANK_ESM), data_path.join(master_file)).unwrap();

        set_load_order(
            game_type,
            &data_path,
            &local_path,
            &initial_load_order(game_type),
        );

        if game_type == GameType::OpenMW {
            touch(&game_path.join("openmw.cfg"));
        } else {
            std::fs::rename(
                data_path.join(BLANK_MASTER_DEPENDENT_ESM),
                data_path.join(BLANK_MASTER_DEPENDENT_ESM.to_owned() + ".ghost"),
            )
            .unwrap();
        }

        std::fs::write(
            data_path.join(NON_PLUGIN_FILE),
            "This isn't a valid plugin file.",
        )
        .unwrap();

        Self {
            _temp_dir: temp_dir,
            game_type,
            game_path,
            local_path,
        }
    }

    pub fn data_path(&self) -> PathBuf {
        data_path(self.game_type, &self.game_path)
    }
}

#[test_parameter]
pub const ALL_GAME_TYPES: [GameType; 12] = [
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

mod unicase {
    #[test]
    fn eq_should_be_case_insensitive() {
        assert!(unicase::eq("i", "I"));
        assert!(!unicase::eq("i", "\u{0130}"));
        assert!(!unicase::eq("i", "\u{0131}"));
        assert!(!unicase::eq("i", "\u{0307}"));
        assert!(!unicase::eq("i", "\u{03a1}"));
        assert!(!unicase::eq("i", "\u{03c1}"));
        assert!(!unicase::eq("i", "\u{03f1}"));

        assert!(!unicase::eq("I", "\u{0130}"));
        assert!(!unicase::eq("I", "\u{0131}"));
        assert!(!unicase::eq("I", "\u{0307}"));
        assert!(!unicase::eq("I", "\u{03a1}"));
        assert!(!unicase::eq("I", "\u{03c1}"));
        assert!(!unicase::eq("I", "\u{03f1}"));

        assert!(!unicase::eq("\u{0130}", "\u{0131}"));
        assert!(!unicase::eq("\u{0130}", "\u{0307}"));
        assert!(!unicase::eq("\u{0130}", "\u{03a1}"));
        assert!(!unicase::eq("\u{0130}", "\u{03c1}"));
        assert!(!unicase::eq("\u{0130}", "\u{03f1}"));

        assert!(!unicase::eq("\u{0131}", "\u{0307}"));
        assert!(!unicase::eq("\u{0131}", "\u{03a1}"));
        assert!(!unicase::eq("\u{0131}", "\u{03c1}"));
        assert!(!unicase::eq("\u{0131}", "\u{03f1}"));

        assert!(!unicase::eq("\u{0307}", "\u{03a1}"));
        assert!(!unicase::eq("\u{0307}", "\u{03c1}"));
        assert!(!unicase::eq("\u{0307}", "\u{03f1}"));

        assert!(unicase::eq("\u{03a1}", "\u{03c1}"));
        assert!(unicase::eq("\u{03a1}", "\u{03f1}"));

        assert!(unicase::eq("\u{03c1}", "\u{03f1}"));
    }
}
