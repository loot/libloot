use std::{
    fs::File,
    io::{BufReader, Read},
    path::{Path, PathBuf},
};

use super::error::{ArchiveParsingError, ArchivePathParsingError};
use crate::{
    archive::ArchiveAssets,
    escape_ascii,
    logging::{self, format_details},
};

use super::{ba2, bsa};

pub(crate) fn assets_in_archives(archive_paths: &[PathBuf]) -> ArchiveAssets {
    let mut archive_assets: ArchiveAssets = ArchiveAssets::new();

    for archive_path in archive_paths {
        logging::trace!(
            "Getting assets loaded from the Bethesda archive at \"{}\"",
            escape_ascii(archive_path)
        );

        let assets = match get_assets_in_archive(archive_path) {
            Ok(a) => a,
            Err(e) => {
                logging::error!(
                    "Encountered an error while trying to read the Bethesda archive at \"{}\": {}",
                    escape_ascii(archive_path),
                    format_details(&e)
                );
                continue;
            }
        };

        // If two archives contain the same combination of folder hash and file
        // hash, they will be deduplicated. Since each asset is only identified
        // by a hash pair, collisions are possible, but since BSAs don't
        // necessarily contain asset file paths, it's not necessarily possible
        // to tell if a collision is for two different file paths or not, and
        // logging all collisions is too noisy.
        for (folder_hash, mut file_hashes) in assets {
            archive_assets
                .entry(folder_hash)
                .or_default()
                .append(&mut file_hashes);
        }
    }

    archive_assets
}

fn get_assets_in_archive(archive_path: &Path) -> Result<ArchiveAssets, ArchivePathParsingError> {
    let file = File::open(archive_path)
        .map_err(|e| ArchivePathParsingError::from_io_error(archive_path.into(), e))?;
    let mut reader = BufReader::new(file);

    let mut type_id: [u8; 4] = [0; 4];
    reader
        .read_exact(&mut type_id)
        .map_err(|e| ArchivePathParsingError::from_io_error(archive_path.into(), e))?;

    match type_id {
        bsa::TYPE_ID => bsa::read_assets(reader)
            .map_err(|e| ArchivePathParsingError::new(archive_path.into(), e)),
        ba2::TYPE_ID => ba2::read_assets(reader)
            .map_err(|e| ArchivePathParsingError::new(archive_path.into(), e)),
        _ => Err(ArchivePathParsingError::new(
            archive_path.into(),
            ArchiveParsingError::UnsupportedArchiveTypeId(type_id),
        )),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    mod get_assets_in_archive {
        use std::{collections::BTreeSet, io::SeekFrom};

        use array_parameterized_test::{parameterized_test, test_parameter};
        use tempfile::tempdir;

        use super::*;

        #[test]
        fn should_error_if_file_cannot_be_opened() {
            let path = Path::new("./invalid.bsa");
            assert!(get_assets_in_archive(path).is_err());
        }

        #[test]
        fn should_support_v103_bsas() {
            let path = Path::new("./testing-plugins/Oblivion/Data/Blank.bsa");
            let assets = get_assets_in_archive(path).unwrap();

            let files_count: usize = assets.values().map(BTreeSet::len).sum();

            let expected_key = "".as_bytes();
            assert_eq!(1, assets.len());
            assert_eq!(1, files_count);
            assert_eq!(expected_key, assets.first_key_value().unwrap().0.as_ref());
            assert_eq!(1, assets[expected_key].len());
            assert_eq!(
                "license".as_bytes(),
                assets[expected_key].first().unwrap().as_ref()
            );
        }

        #[test]
        fn should_support_v104_bsas() {
            let path = Path::new("./testing-plugins/Skyrim/Data/Blank.bsa");
            let assets = get_assets_in_archive(path).unwrap();

            let files_count: usize = assets.values().map(BTreeSet::len).sum();

            let expected_key = "\x2E".as_bytes();
            assert_eq!(1, assets.len());
            assert_eq!(1, files_count);
            assert_eq!(expected_key, assets.first_key_value().unwrap().0.as_ref());
            assert_eq!(1, assets[expected_key].len());
            assert_eq!(
                "license".as_bytes(),
                assets[expected_key].first().unwrap().as_ref()
            );
        }

        #[test]
        fn should_support_v105_bsas() {
            let path = Path::new("./testing-plugins/SkyrimSE/Data/Blank.bsa");
            let assets = get_assets_in_archive(path).unwrap();

            let files_count: usize = assets.values().map(BTreeSet::len).sum();

            let expected_key = "dev\\git\\testing-plugins".as_bytes();
            assert_eq!(1, assets.len());
            assert_eq!(1, files_count);
            assert_eq!(expected_key, assets.first_key_value().unwrap().0.as_ref());
            assert_eq!(1, assets[expected_key].len());
            assert_eq!(
                "license".as_bytes(),
                assets[expected_key].first().unwrap().as_ref()
            );
        }

        #[test]
        fn should_support_general_ba2s() {
            let path = Path::new("./testing-plugins/Fallout 4/Data/Blank - Main.ba2");
            let assets = get_assets_in_archive(path).unwrap();

            let files_count: usize = assets.values().map(BTreeSet::len).sum();

            let expected_key = "dev\\git\\testing-plugins".as_bytes();
            let expected_file_hash = "license.txt".as_bytes();

            assert_eq!(1, assets.len());
            assert_eq!(1, files_count);

            let (key, value) = assets.first_key_value().unwrap();
            assert_eq!(expected_key, key.as_ref());
            assert_eq!(1, value.len());
            assert_eq!(expected_file_hash, value.first().unwrap().as_ref());
        }

        #[test]
        fn should_support_texture_ba2s() {
            let path = Path::new("./testing-plugins/Fallout 4/Data/Blank - Textures.ba2");
            let assets = get_assets_in_archive(path).unwrap();

            let files_count: usize = assets.values().map(BTreeSet::len).sum();

            let expected_key = "dev\\git\\testing-plugins".as_bytes();
            let expected_file_hash = "blank.dds".as_bytes();

            assert_eq!(1, assets.len());
            assert_eq!(1, files_count);

            let (key, value) = assets.first_key_value().unwrap();
            assert_eq!(expected_key, key.as_ref());
            assert_eq!(1, value.len());
            assert_eq!(expected_file_hash, value.first().unwrap().as_ref());
        }

        #[test_parameter]
        const BA2_VERSIONS: [u32; 5] = [1, 2, 3, 7, 8];

        #[parameterized_test(BA2_VERSIONS)]
        fn should_support_ba2_versions(version: u32) {
            use std::io::{Seek, Write};

            let tmp_dir = tempdir().unwrap();
            let path = tmp_dir.path().join("test.ba2");

            std::fs::copy("./testing-plugins/Fallout 4/Data/Blank - Main.ba2", &path).unwrap();

            {
                let mut file = File::options().write(true).open(&path).unwrap();
                file.seek(SeekFrom::Start(4)).unwrap();
                file.write_all(&version.to_le_bytes()).unwrap();
            }

            let assets = get_assets_in_archive(&path).unwrap();
            assert!(!assets.is_empty());
        }
    }

    mod assets_in_archives {
        use std::collections::BTreeSet;

        use super::*;

        #[test]
        fn should_skip_files_that_cannot_be_read() {
            let paths = [
                PathBuf::from("invalid.bsa"),
                PathBuf::from("./testing-plugins/Skyrim/Data/Blank.bsa"),
            ];

            let assets = assets_in_archives(&paths);

            let files_count: usize = assets.values().map(BTreeSet::len).sum();

            assert_eq!(1, assets.len());
            assert_eq!(1, files_count);

            let (key, value) = assets.first_key_value().unwrap();
            assert_eq!("\x2E".as_bytes(), key.as_ref());
            assert_eq!(1, value.len());
            assert_eq!("license".as_bytes(), value.first().unwrap().as_ref());
        }

        #[test]
        fn should_combine_assets_from_each_loaded_archive() {
            let paths = [
                PathBuf::from("./testing-plugins/Oblivion/Data/Blank.bsa"),
                PathBuf::from("./testing-plugins/Skyrim/Data/Blank.bsa"),
                PathBuf::from("./testing-plugins/SkyrimSE/Data/Blank.bsa"),
            ];

            let assets = assets_in_archives(&paths);

            let files_count: usize = assets.values().map(BTreeSet::len).sum();

            assert_eq!(3, assets.len());
            assert_eq!(3, files_count);

            let value = &assets["".as_bytes()];
            assert_eq!(1, value.len());
            assert_eq!("license".as_bytes(), value.first().unwrap().as_ref());

            let value = &assets["\x2E".as_bytes()];
            assert_eq!(1, value.len());
            assert_eq!("license".as_bytes(), value.first().unwrap().as_ref());

            let value = &assets["dev\\git\\testing-plugins".as_bytes()];
            assert_eq!(1, value.len());
            assert_eq!("license".as_bytes(), value.first().unwrap().as_ref());
        }
    }
}
