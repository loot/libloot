# libloot-rs

This is an **experimental** reimplementation of [libloot](https://github.com/loot/libloot) using Rust instead of C++, that should match libloot v0.25.5.

See the `cxx` and `pyo3` subdirectories for C++ and Python wrappers respectively.

Expect:

- bugs
- Git history to be rewritten without warning
- this Git repository to disappear without warning

If this experiment is successful it will probably end up being merged into the libloot repository, replacing much of its contents.

## Outstanding issues

- I haven't yet attempted to build it on Linux.
- There is no CI.
- It relies on some unreleased improvements to the Rust APIs of libloadorder and loot-condition-interpreter.
- It uses more memory than the C++ implementation, and I haven't finished investigating why or if there's anything I can/should do about that.
- I'm not very confident about the YAML emitter I've written, it needs more test cases and probably another review against the spec.
- The `saphyr` and `saphyr-parser` dependencies that are used to parse metadata YAML have some rough edges, and compared to the other dependencies they are relatively new, unpopular libraries that are maintained by people I've not heard of before, so they may be relatively risky dependencies.

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
