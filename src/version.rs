/// libloot's major version number.
pub const LIBLOOT_VERSION_MAJOR: u32 = parse_u32(env!("CARGO_PKG_VERSION_MAJOR"));

/// libloot's minor version number.
pub const LIBLOOT_VERSION_MINOR: u32 = parse_u32(env!("CARGO_PKG_VERSION_MINOR"));

/// libloot's patch version number.
pub const LIBLOOT_VERSION_PATCH: u32 = parse_u32(env!("CARGO_PKG_VERSION_PATCH"));

/// Get the library version in the form "major.minor.patch".
pub fn libloot_version() -> String {
    env!("CARGO_PKG_VERSION").to_string()
}

/// Get the ID of the source control revision that libloot was built from.
pub fn libloot_revision() -> String {
    libloot_revision_const().to_string()
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

const fn parse_u32(value: &str) -> u32 {
    let mut acc = 0;
    let mut i = 0;
    while i < value.len() {
        acc = acc * 10 + (value.as_bytes()[i] - b'0') as u32;
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
