mod ba2;
mod bsa;
mod error;
mod find;
mod parse;

use std::collections::{BTreeMap, BTreeSet};

pub use find::find_associated_archives;
pub use parse::assets_in_archives;

pub fn do_assets_overlap(
    assets: &BTreeMap<u64, BTreeSet<u64>>,
    other_assets: &BTreeMap<u64, BTreeSet<u64>>,
) -> bool {
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

            assert_eq!(assets1.get(&0), assets2.get(&0x2E01_002E));

            assert!(!do_assets_overlap(&assets1, &assets2));
        }
    }
}
