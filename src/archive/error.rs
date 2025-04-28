use std::path::PathBuf;

use crate::escape_ascii;

#[derive(Debug)]
pub(crate) struct ArchivePathParsingError {
    path: PathBuf,
    error: ArchiveParsingError,
}

impl ArchivePathParsingError {
    pub(crate) fn new(path: PathBuf, error: ArchiveParsingError) -> Self {
        Self { path, error }
    }

    pub(crate) fn from_io_error(path: PathBuf, error: std::io::Error) -> Self {
        Self {
            path,
            error: ArchiveParsingError::IoError(error),
        }
    }
}

impl std::fmt::Display for ArchivePathParsingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "failed to parse the archive at \"{}\"",
            escape_ascii(&self.path)
        )
    }
}

impl std::error::Error for ArchivePathParsingError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

#[derive(Debug)]
pub(crate) enum ArchiveParsingError {
    IoError(std::io::Error),
    UnsupportedHeaderVersion(u32),
    UnsupportedHeaderArchiveType([u8; 4]),
    UnsupportedArchiveTypeId([u8; 4]),
    InvalidRecordsOffset(u32),
    InvalidFolderNameLengthOffset(usize),
    InvalidFileRecordsOffset(usize),
    UsesBigEndianNumbers,
    FolderHashCollision(u64),
    HashCollision { folder_hash: u64, file_hash: u64 },
    SliceTooSmall { expected: usize, actual: usize },
}

impl std::fmt::Display for ArchiveParsingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::IoError(_) => write!(f, "an I/O error occurred"),
            Self::UnsupportedHeaderVersion(v) => write!(f, "unsupported archive version {v}"),
            Self::UnsupportedHeaderArchiveType(a) => {
                write!(f, "unsupported archive type {}", a.escape_ascii())
            }
            Self::UnsupportedArchiveTypeId(t) => {
                write!(f, "unsupported archive type ID {}", t.escape_ascii())
            }
            Self::InvalidRecordsOffset(o) => write!(f, "invalid records offset {o}"),
            Self::InvalidFolderNameLengthOffset(o) => {
                write!(f, "invalid folder name length offset {o}")
            }
            Self::InvalidFileRecordsOffset(o) => write!(f, "invalid file records offset {o}"),
            Self::UsesBigEndianNumbers => {
                write!(f, "archive uses big-endian numbers, which is unsupported")
            }
            Self::FolderHashCollision(h) => {
                write!(f, "unexpected collision for folder name hash {h:x}")
            }
            Self::HashCollision {
                folder_hash,
                file_hash,
            } => write!(
                f,
                "unexpected collision for file name hash {file_hash:x} in set for folder name hash {folder_hash:x}"
            ),
            Self::SliceTooSmall { expected, actual } => write!(
                f,
                "byte slice was unexpectedly too small: expected {expected} bytes, got {actual} bytes"
            ),
        }
    }
}

impl std::error::Error for ArchiveParsingError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::IoError(e) => Some(e),
            _ => None,
        }
    }
}

impl From<std::io::Error> for ArchiveParsingError {
    fn from(value: std::io::Error) -> Self {
        ArchiveParsingError::IoError(value)
    }
}

pub(super) fn slice_too_small(slice: &[u8], expected_size: usize) -> ArchiveParsingError {
    ArchiveParsingError::SliceTooSmall {
        expected: expected_size,
        actual: slice.len(),
    }
}
