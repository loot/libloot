# libloot-rs

This is an **experimental** reimplementation of [libloot](https://github.com/loot/libloot) using Rust instead of C++, that should match libloot v0.25.5.

## Build

Make sure you have [Rust](https://www.rust-lang.org/) installed.

To build the library, set the `LIBLOOT_REVISION` env var and then run Cargo. Using PowerShell:

```powershell
$env:LIBLOOT_REVISION = git rev-parse --short HEAD
cargo build --release
```

The tests include a complete port of libloot's tests, and can be run by first extracting the [testing-plugins](https://github.com/Ortham/testing-plugins) archive to this readme's directory (so that there's a `testing-plugins` directory there), then running:

```
cargo test
```

The public API has doc comments copied from libloot, and the API documentation can be built and viewed using:

```
cargo doc --open
```
