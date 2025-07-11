use std::{
    collections::{BTreeMap, BTreeSet, btree_map::Entry},
    io::BufRead,
};

use super::error::ArchiveParsingError;

pub(super) const TYPE_ID: [u8; 4] = *b"BSA\0";
const HEADER_SIZE: usize = 36;
const FILE_RECORD_SIZE: usize = 16;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
struct Header {
    type_id: [u8; 4],
    version: u32,
    records_offset: u32,
    archive_flags: u32,
    folder_count: u32,
    total_file_count: u32,
    total_folder_names_length: u32,
    total_file_names_length: u32,
    content_type_flags: u32,
}

impl TryFrom<[u8; HEADER_SIZE - TYPE_ID.len()]> for Header {
    type Error = ArchiveParsingError;

    fn try_from(value: [u8; HEADER_SIZE - TYPE_ID.len()]) -> Result<Self, Self::Error> {
        // LIMITATION: There's no syntax to infallibly split an array into sub-arrays.
        #[rustfmt::skip]
        let [
            v0, v1, v2, v3, // version
            r0, r1, r2, r3, // records_offset
            a0, a1, a2, a3, // archive_flags
            folder_count0, folder_count1, folder_count2, folder_count3,
            file_count0, file_count1, file_count2, file_count3,
            folder_names0, folder_names1, folder_names2, folder_names3,
            file_names0, file_names1, file_names2, file_names3,
            content_type_flags @ ..,
        ] = value;
        let header = Self {
            type_id: TYPE_ID,
            version: u32::from_le_bytes([v0, v1, v2, v3]),
            records_offset: u32::from_le_bytes([r0, r1, r2, r3]),
            archive_flags: u32::from_le_bytes([a0, a1, a2, a3]),
            folder_count: u32::from_le_bytes([
                folder_count0,
                folder_count1,
                folder_count2,
                folder_count3,
            ]),
            total_file_count: u32::from_le_bytes([
                file_count0,
                file_count1,
                file_count2,
                file_count3,
            ]),
            total_folder_names_length: u32::from_le_bytes([
                folder_names0,
                folder_names1,
                folder_names2,
                folder_names3,
            ]),
            total_file_names_length: u32::from_le_bytes([
                file_names0,
                file_names1,
                file_names2,
                file_names3,
            ]),
            content_type_flags: u32::from_le_bytes(content_type_flags),
        };

        if to_usize(header.records_offset) != HEADER_SIZE {
            return Err(ArchiveParsingError::InvalidRecordsOffset(
                header.records_offset,
            ));
        }

        if (header.archive_flags & 0x40) != 0 {
            return Err(ArchiveParsingError::UsesBigEndianNumbers);
        }

        Ok(header)
    }
}

struct FolderRecord {
    name_hash: u64,
    file_count: u32,
    file_records_offset: u32,
}

// Also used for v104 BSAs.
mod v103 {
    use super::FolderRecord;

    pub(super) const FOLDER_RECORD_SIZE: usize = 16;

    pub(super) fn read_folder_record(value: &[u8; FOLDER_RECORD_SIZE]) -> FolderRecord {
        // LIMITATION: There's no syntax to infallibly split an array into sub-arrays.
        let [name_hash @ .., c0, c1, c2, c3, o0, o1, o2, o3] = *value;

        FolderRecord {
            name_hash: u64::from_le_bytes(name_hash),
            file_count: u32::from_le_bytes([c0, c1, c2, c3]),
            file_records_offset: u32::from_le_bytes([o0, o1, o2, o3]),
        }
    }
}

mod v105 {
    use super::FolderRecord;

    pub(super) const FOLDER_RECORD_SIZE: usize = 24;

    pub(super) fn read_folder_record(value: &[u8; FOLDER_RECORD_SIZE]) -> FolderRecord {
        // LIMITATION: There's no syntax to infallibly split an array into sub-arrays.
        #[rustfmt::skip]
        let [
            name_hash @ ..,
            c0, c1, c2, c3, // file_count
            _, _, _, _,
            o0, o1, o2, o3, // file_records_offset
            _, _, _, _,
        ] = *value;

        FolderRecord {
            name_hash: u64::from_le_bytes(name_hash),
            file_count: u32::from_le_bytes([c0, c1, c2, c3]),
            file_records_offset: u32::from_le_bytes([o0, o1, o2, o3]),
        }
    }
}

pub(super) fn read_assets<T: BufRead>(
    mut reader: T,
) -> Result<BTreeMap<u64, BTreeSet<u64>>, ArchiveParsingError> {
    let mut header_buffer = [0; HEADER_SIZE - TYPE_ID.len()];

    reader.read_exact(&mut header_buffer)?;

    let header = Header::try_from(header_buffer)?;

    match header.version {
        103 | 104 => read_assets_with_header::<T, { v103::FOLDER_RECORD_SIZE }>(
            reader,
            &header,
            v103::read_folder_record,
        ),
        105 => read_assets_with_header::<T, { v105::FOLDER_RECORD_SIZE }>(
            reader,
            &header,
            v105::read_folder_record,
        ),
        _ => Err(ArchiveParsingError::UnsupportedHeaderVersion(
            header.version,
        )),
    }
}

fn read_assets_with_header<T: BufRead, const U: usize>(
    mut reader: T,
    header: &Header,
    read_folder_record: impl Fn(&[u8; U]) -> FolderRecord,
) -> Result<BTreeMap<u64, BTreeSet<u64>>, ArchiveParsingError> {
    let mut folders_buffer: Vec<u8> = vec![0; U * to_usize(header.folder_count)];

    reader.read_exact(folders_buffer.as_mut_slice())?;

    let file_records_size = to_usize(header.folder_count)
        + to_usize(header.total_folder_names_length)
        + to_usize(header.total_file_count) * FILE_RECORD_SIZE;

    let mut file_records_buffer: Vec<u8> = vec![0; file_records_size];

    reader.read_exact(file_records_buffer.as_mut_slice())?;

    let folder_record_offset_baseline =
        HEADER_SIZE + folders_buffer.len() + to_usize(header.total_file_names_length);

    let mut assets = BTreeMap::new();
    for chunk in folders_buffer.as_chunks::<U>().0 {
        let folder_record = read_folder_record(chunk);

        let entry = assets.entry(folder_record.name_hash);
        if let Entry::Occupied(_) = entry {
            return Err(ArchiveParsingError::FolderHashCollision(
                folder_record.name_hash,
            ));
        }

        let file_records_offset = if (header.archive_flags & 0x1) == 0 {
            to_usize(folder_record.file_records_offset) - folder_record_offset_baseline
        } else {
            let folder_name_length_offset =
                to_usize(folder_record.file_records_offset) - folder_record_offset_baseline;

            if let Some(folder_name_length) = file_records_buffer.get(folder_name_length_offset) {
                folder_name_length_offset + 1 + usize::from(*folder_name_length)
            } else {
                return Err(ArchiveParsingError::InvalidFolderNameLengthOffset(
                    folder_name_length_offset,
                ));
            }
        };

        let Some(file_records_buffer) = file_records_buffer.get(file_records_offset..) else {
            return Err(ArchiveParsingError::InvalidFileRecordsOffset(
                file_records_offset,
            ));
        };

        let file_hashes: &mut BTreeSet<u64> = entry.or_default();

        for file_chunk in file_records_buffer
            .as_chunks::<FILE_RECORD_SIZE>()
            .0
            .iter()
            .take(to_usize(folder_record.file_count))
        {
            let file_hash = file_record_hash(file_chunk);

            if !file_hashes.insert(file_hash) {
                return Err(ArchiveParsingError::HashCollision {
                    folder_hash: folder_record.name_hash,
                    file_hash,
                });
            }
        }
    }

    Ok(assets)
}

fn file_record_hash(file_record: &[u8; FILE_RECORD_SIZE]) -> u64 {
    // LIMITATION: There's no syntax to infallibly split an array into sub-arrays.
    let [hash_bytes @ .., _, _, _, _, _, _, _, _] = file_record;
    u64::from_le_bytes(*hash_bytes)
}

#[expect(
    clippy::as_conversions,
    reason = "A compile-time assertion ensures that this conversion will be lossless on all relevant target platforms"
)]
const fn to_usize(value: u32) -> usize {
    // Error at compile time if this conversion isn't lossless.
    const _: () = assert!(u32::BITS <= usize::BITS, "cannot fit a u32 into a usize!");
    value as usize
}
