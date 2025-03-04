use std::{
    collections::{BTreeMap, BTreeSet},
    fs::File,
    io::{BufReader, Read},
    path::{Path, PathBuf},
};

use super::error::{ArchiveParsingError, ArchivePathParsingError};
use crate::{logging, plugin::has_ascii_extension};

use super::{ba2, bsa};

pub fn assets_in_archives(archive_paths: &[PathBuf]) -> BTreeMap<u64, BTreeSet<u64>> {
    let mut archive_assets: BTreeMap<u64, BTreeSet<u64>> = BTreeMap::new();

    for archive_path in archive_paths {
        logging::trace!(
            "Getting assets loaded from the Bethesda archive at \"{}\"",
            archive_path.display()
        );

        let assets = match get_assets_in_archive(archive_path) {
            Ok(a) => a,
            Err(e) => {
                logging::error!(
                    "Encountered an error while trying to read the Bethesda archive at \"{}\": {}",
                    archive_path.display(),
                    e
                );
                continue;
            }
        };

        let warn_on_hash_collisions = should_warn_on_hash_collisions(archive_path);

        for (folder_hash, file_hashes) in assets {
            let entry_file_hashes = archive_assets.entry(folder_hash).or_default();

            for file_hash in file_hashes {
                if !entry_file_hashes.insert(file_hash) && warn_on_hash_collisions {
                    logging::warn!(
                        "The folder and file with hashes {:x} and {:x} in \"{}\" are present in another Bethesda archive.",
                        folder_hash,
                        file_hash,
                        archive_path.display()
                    );
                }
            }
        }
    }

    archive_assets
}

fn should_warn_on_hash_collisions(archive_path: &Path) -> bool {
    if !has_ascii_extension(archive_path, "ba2") {
        return true;
    }

    let filename = archive_path
        .file_name()
        .unwrap_or_default()
        .to_string_lossy()
        .to_ascii_lowercase();

    filename.starts_with("fallout4 - ") || filename.starts_with("dlcultrahighresolution - ")
}

fn get_assets_in_archive(
    archive_path: &Path,
) -> Result<BTreeMap<u64, BTreeSet<u64>>, ArchivePathParsingError> {
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

pub(super) fn to_u32(bytes: &[u8]) -> u32 {
    let array =
        <[u8; 4]>::try_from(&bytes[..4]).expect("Bytes slice is large enough to hold a u32");
    u32::from_le_bytes(array)
}

pub(super) fn to_u64(bytes: &[u8]) -> u64 {
    let array =
        <[u8; 8]>::try_from(&bytes[..8]).expect("Bytes slice is large enough to hold a u64");
    u64::from_le_bytes(array)
}

pub(super) fn to_usize(size: u32) -> usize {
    usize::try_from(size).expect("usize can hold a u32")
}
