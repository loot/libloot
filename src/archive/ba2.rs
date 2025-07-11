use std::{
    collections::{BTreeMap, BTreeSet},
    hash::{DefaultHasher, Hash, Hasher},
    io::{BufRead, Seek},
};

use super::error::ArchiveParsingError;

pub(super) const TYPE_ID: [u8; 4] = *b"BTDX";
const HEADER_SIZE: usize = 24;
const BA2_GENERAL_TYPE: [u8; 4] = *b"GNRL";
const BA2_TEXTURE_TYPE: [u8; 4] = *b"DX10";

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
struct Header {
    type_id: [u8; 4],
    version: u32,
    archive_type: [u8; 4],
    file_count: u32,
    file_paths_offset: u64,
}

impl TryFrom<[u8; HEADER_SIZE - TYPE_ID.len()]> for Header {
    type Error = ArchiveParsingError;

    fn try_from(value: [u8; HEADER_SIZE - TYPE_ID.len()]) -> Result<Self, Self::Error> {
        // LIMITATION: There's no syntax to infallibly split an array into sub-arrays.
        #[rustfmt::skip]
        let [
            v0, v1, v2, v3, // version
            a0, a1, a2, a3, // archive_type
            c0, c1, c2, c3, // file_count
            file_paths_offset @ ..,
        ] = value;
        let header = Self {
            type_id: TYPE_ID,
            version: u32::from_le_bytes([v0, v1, v2, v3]),
            archive_type: [a0, a1, a2, a3],
            file_count: u32::from_le_bytes([c0, c1, c2, c3]),
            file_paths_offset: u64::from_le_bytes(file_paths_offset),
        };

        // The header version is 1, 7 or 8 for Fallout 4 and 2 or 3 for Starfield.
        if !matches!(header.version, 1 | 2 | 3 | 7 | 8) {
            return Err(ArchiveParsingError::UnsupportedHeaderVersion(
                header.version,
            ));
        }

        if !matches!(header.archive_type, BA2_GENERAL_TYPE | BA2_TEXTURE_TYPE) {
            return Err(ArchiveParsingError::UnsupportedHeaderArchiveType(
                header.archive_type,
            ));
        }

        Ok(header)
    }
}

pub(super) fn read_assets<T: BufRead + Seek>(
    mut reader: T,
) -> Result<BTreeMap<u64, BTreeSet<u64>>, ArchiveParsingError> {
    let mut header_buffer = [0; HEADER_SIZE - TYPE_ID.len()];

    reader.read_exact(&mut header_buffer)?;

    let header = Header::try_from(header_buffer)?;

    let mut assets = BTreeMap::new();

    reader.seek(std::io::SeekFrom::Start(header.file_paths_offset))?;

    for _ in 0..header.file_count {
        let mut length_buf = [0; 2];
        reader.read_exact(&mut length_buf)?;

        let path_length = u16::from_le_bytes(length_buf);
        let mut file_path_bytes = vec![0; path_length.into()];
        reader.read_exact(file_path_bytes.as_mut_slice())?;

        normalise_path(&mut file_path_bytes);

        let file_path_bytes = trim_slashes(&file_path_bytes);

        let (folder_hash, file_hash) = rsplit_on(file_path_bytes, b'\\').map_or_else(
            || (0, hash(&file_path_bytes)),
            |(folder_path, file_path)| (hash(&folder_path), hash(&file_path)),
        );

        let file_hashes: &mut BTreeSet<u64> = assets.entry(folder_hash).or_default();

        if !file_hashes.insert(file_hash) {
            return Err(ArchiveParsingError::HashCollision {
                folder_hash,
                file_hash,
            });
        }
    }

    Ok(assets)
}

fn normalise_path(path_bytes: &mut [u8]) {
    for byte in path_bytes {
        // Ignore any non-ASCII characters.
        if *byte > 127 {
            continue;
        }

        *byte = match byte {
            b'/' => b'\\',
            _ => byte.to_ascii_lowercase(),
        }
    }
}

fn trim_slashes(mut path_bytes: &[u8]) -> &[u8] {
    while let [first, rest @ ..] = path_bytes {
        if *first == b'\\' {
            path_bytes = rest;
        } else {
            break;
        }
    }

    while let [rest @ .., last] = path_bytes {
        if *last == b'\\' {
            path_bytes = rest;
        } else {
            break;
        }
    }

    path_bytes
}

fn rsplit_on(slice: &[u8], needle: u8) -> Option<(&[u8], &[u8])> {
    let mut iter = slice.rsplitn(2, |b| *b == needle);
    let second = iter.next()?;
    let first = iter.next()?;

    Some((first, second))
}

fn hash<T: Hash>(value: &T) -> u64 {
    let mut hasher = DefaultHasher::new();
    value.hash(&mut hasher);
    hasher.finish()
}
