mod ba2;
mod bsa;
mod error;
mod find;
mod parse;

use std::collections::{BTreeMap, BTreeSet};

pub(crate) use find::find_associated_archives;
pub(crate) use parse::assets_in_archives;

pub(crate) type ArchiveAssets = BTreeMap<Box<[u8]>, BTreeSet<Box<[u8]>>>;

pub(crate) fn do_assets_overlap(assets: &ArchiveAssets, other_assets: &ArchiveAssets) -> bool {
    let mut assets_iter = assets.iter();
    let mut other_assets_iter = other_assets.iter();

    let mut assets = assets_iter.next();
    let mut other_assets = other_assets_iter.next();
    while let (Some((folder, files)), Some((other_folder, other_files))) = (assets, other_assets) {
        if folder < other_folder {
            assets = assets_iter.next();
        } else if folder > other_folder {
            other_assets = other_assets_iter.next();
        } else if files.intersection(other_files).next().is_some() {
            return true;
        } else {
            // The folder hashes are equal but they don't contain any of the same
            // file hashes, move on to the next folder. It doesn't matter which
            // iterator gets incremented.
            assets = assets_iter.next();
        }
    }

    false
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

#[cfg(test)]
mod tests {
    use super::*;

    mod do_assets_overlap {
        use std::path::PathBuf;

        use super::*;

        #[test]
        fn should_return_true_if_the_same_file_exists_in_the_same_folder() {
            let path = PathBuf::from("./testing-plugins/Oblivion/Data/Blank.bsa");
            let assets = assets_in_archives(&[path]);

            assert!(do_assets_overlap(&assets, &assets));
        }

        #[test]
        fn should_return_false_if_the_same_file_exists_in_different_folders() {
            let path = PathBuf::from("./testing-plugins/Oblivion/Data/Blank.bsa");
            let assets1 = assets_in_archives(&[path]);

            let path = PathBuf::from("./testing-plugins/Skyrim/Data/Blank.bsa");
            let assets2 = assets_in_archives(&[path]);

            assert_eq!(assets1.get("".as_bytes()), assets2.get("\x2E".as_bytes()));

            assert!(!do_assets_overlap(&assets1, &assets2));
        }
    }
}
