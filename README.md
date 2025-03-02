# libloot-rs

This is an **incomplete** and **experimental** reimplementation of [libloot](https://github.com/loot/libloot) using Rust instead of C++.

## Current status

Currently complete:

- [x] Public API types and function declarations (excluding errors)
- [ ] Public API error types
- [x] Public API doc comments
- [x] Library versioning
- [ ] Setting a logging callback
- [x] Parsing metadata from YAML
- [ ] Serialising metadata to YAML
- [x] Game-related functionality
- [x] Plugin-related functionality
- [x] Archive-related functionality
- [x] Metadata-related functionality (excluding writing YAML)
- [x] Sorting functionality
- [ ] Unit tests
- [ ] Integration tests
- [ ] C++ FFI
- [ ] Python FFI

The complete bits should match libloot commit [55b341fc6cbdccee52e42923c13a91eddb5ca97d](https://github.com/loot/libloot/commit/55b341fc6cbdccee52e42923c13a91eddb5ca97d), which is libloot v0.25.3 plus a few changes prompted by this translation.

## Build

Make sure you have [Rust](https://www.rust-lang.org/) installed.

To build the library, set the `LIBLOOT_REVISION` env var and then run Cargo. Using PowerShell:

```powershell
$env:LIBLOOT_REVISION = git rev-parse --short HEAD
cargo build --release
```

There aren't many tests, but those that exist can be run using:

```
cargo test
```

The public API has doc comments copied from libloot, and the API documentation can be built and viewed using:

```
cargo doc --open
```
