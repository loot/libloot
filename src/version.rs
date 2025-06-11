/// libloot's major version number.
pub const LIBLOOT_VERSION_MAJOR: u32 = parse_u32(env!("CARGO_PKG_VERSION_MAJOR"));

/// libloot's minor version number.
pub const LIBLOOT_VERSION_MINOR: u32 = parse_u32(env!("CARGO_PKG_VERSION_MINOR"));

/// libloot's patch version number.
pub const LIBLOOT_VERSION_PATCH: u32 = parse_u32(env!("CARGO_PKG_VERSION_PATCH"));

/// Get the library version in the form "major.minor.patch".
pub fn libloot_version() -> String {
    env!("CARGO_PKG_VERSION").to_owned()
}

/// Get the ID of the source control revision that libloot was built from.
pub fn libloot_revision() -> String {
    libloot_revision_const().to_owned()
}

/// Checks whether the loaded API is compatible with the given version of the
/// API, abstracting API stability policy away from clients. The version
/// numbering used is major.minor.patch.
pub fn is_compatible(major: u32, minor: u32, _patch: u32) -> bool {
    if major > 0 {
        major == LIBLOOT_VERSION_MAJOR
    } else {
        minor == LIBLOOT_VERSION_MINOR
    }
}

#[expect(
    clippy::as_conversions,
    reason = "Can't convert the u8 to a u32 another way in a const context"
)]
#[expect(
    clippy::indexing_slicing,
    reason = "Can't iterate over the byte values another way in a const context"
)]
const fn parse_u32(value: &str) -> u32 {
    let bytes = value.as_bytes();
    let mut acc = 0;
    let mut i = 0;
    while i < bytes.len() {
        acc = acc * 10 + (bytes[i] - b'0') as u32;
        i += 1;
    }
    acc
}

const fn libloot_revision_const() -> &'static str {
    if let Some(s) = option_env!("LIBLOOT_REVISION") {
        s
    } else {
        "unknown"
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    mod is_compatible {
        use super::*;

        #[test]
        fn should_return_true_if_given_the_current_version() {
            assert!(is_compatible(
                LIBLOOT_VERSION_MAJOR,
                LIBLOOT_VERSION_MINOR,
                LIBLOOT_VERSION_PATCH
            ));
        }

        #[test]
        fn should_return_true_if_given_a_different_patch_version() {
            assert!(is_compatible(
                LIBLOOT_VERSION_MAJOR,
                LIBLOOT_VERSION_MINOR,
                LIBLOOT_VERSION_PATCH + 1
            ));
        }

        #[test]
        fn should_return_false_if_given_a_different_major_version() {
            assert!(!is_compatible(
                LIBLOOT_VERSION_MAJOR + 1,
                LIBLOOT_VERSION_MINOR,
                LIBLOOT_VERSION_PATCH
            ));
        }

        #[test]
        fn should_return_false_if_given_a_different_minor_version() {
            assert!(!is_compatible(
                LIBLOOT_VERSION_MAJOR,
                LIBLOOT_VERSION_MINOR + 1,
                LIBLOOT_VERSION_PATCH
            ));
        }
    }
    mod libloot_version {
        use super::*;

        #[test]
        fn should_be_version_numbers_separated_by_periods() {
            let expected =
                format!("{LIBLOOT_VERSION_MAJOR}.{LIBLOOT_VERSION_MINOR}.{LIBLOOT_VERSION_PATCH}",);

            assert_eq!(expected, libloot_version());
        }
    }

    mod libloot_revision {
        use super::*;

        #[test]
        fn should_not_be_empty() {
            assert!(!libloot_revision().is_empty());
        }
    }
}
