use std::io::{BufRead, Seek};

use crate::archive::{ArchiveAssets, normalise_path};

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
) -> Result<ArchiveAssets, ArchiveParsingError> {
    let mut header_buffer = [0; HEADER_SIZE - TYPE_ID.len()];

    reader.read_exact(&mut header_buffer)?;

    let header = Header::try_from(header_buffer)?;

    let mut assets = ArchiveAssets::new();

    reader.seek(std::io::SeekFrom::Start(header.file_paths_offset))?;

    for _ in 0..header.file_count {
        let mut length_buf = [0; 2];
        reader.read_exact(&mut length_buf)?;

        let path_length = u16::from_le_bytes(length_buf);
        let mut file_path_bytes = vec![0; path_length.into()];
        reader.read_exact(file_path_bytes.as_mut_slice())?;

        normalise_path(&mut file_path_bytes);

        trim_slashes(&mut file_path_bytes);

        let (folder_path, file_name) = rsplit_on(file_path_bytes, b'\\');

        assets.entry(folder_path).or_default().insert(file_name);
    }

    Ok(assets)
}

fn trim_slashes(path_bytes: &mut Vec<u8>) {
    let predicate = |c: &u8| *c != b'\\';

    match path_bytes.iter().rposition(predicate) {
        Some(i) => path_bytes.truncate(i + 1),
        None => path_bytes.clear(),
    }

    let mut trimmed = path_bytes
        .iter()
        .position(predicate)
        .map(|i| path_bytes.split_off(i))
        .unwrap_or_default();

    std::mem::swap(path_bytes, &mut trimmed);
}

fn rsplit_on(mut path_bytes: Vec<u8>, needle: u8) -> (Box<[u8]>, Box<[u8]>) {
    let second = path_bytes
        .iter()
        .rposition(|b| *b == needle)
        .map(|i| path_bytes.split_off(i + 1))
        .unwrap_or_default();

    path_bytes.truncate(path_bytes.len().saturating_sub(1));

    (path_bytes.into_boxed_slice(), second.into_boxed_slice())
}
