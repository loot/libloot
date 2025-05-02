# libloot-rs

This is an **experimental** reimplementation of [libloot](https://github.com/loot/libloot) using Rust instead of C++, that should match libloot v0.26.1.

There is one intentional difference in behaviour: if a plugin has metadata but it is all filtered out when conditions are evaluated, getting its metadata returns `None` instead of a name-only PluginMetadata object.

There are C++, Python and Node.js wrappers in the `cpp`, `python` and `nodejs` subdirectories respectively.

Expect:

- bugs
- Git history to be rewritten without warning
- this Git repository to disappear without warning

If this experiment is successful it will probably end up being merged into the libloot repository, replacing much of its contents.

## Outstanding issues

- It uses more memory than the C++ implementation, and I haven't finished investigating why or if there's anything I can/should do about that.
- The `saphyr` and `saphyr-parser` dependencies that are used to parse metadata YAML have some rough edges, and compared to the other dependencies they are relatively new, unpopular libraries that are maintained by people I've not heard of before, so they may be relatively risky dependencies.

## Build

Make sure you have [Rust](https://www.rust-lang.org/) installed.

To build the library, set the `LIBLOOT_REVISION` env var and then run Cargo.

Using PowerShell:

```powershell
$env:LIBLOOT_REVISION = git rev-parse --short HEAD
cargo build --release
```

Using a POSIX shell:

```sh
export LIBLOOT_REVISION=$(git rev-parse --short HEAD)
cargo build --release
```

`LIBLOOT_REVISION` is used to embed the commit hash into the build, if it's not defined then `unknown` will be used instead.

### Tests

The tests include a complete port of libloot's tests, and can be run by first extracting the [testing-plugins](https://github.com/Ortham/testing-plugins) archive to this readme's directory (so that there's a `testing-plugins` directory there).

To do that using `curl` and `tar` in a POSIX shell:

```sh
curl -sSfL https://github.com/Ortham/testing-plugins/archive/refs/tags/1.6.2.tar.gz | tar -xz --strip=1 --one-top-level=testing-plugins
```

To do that in PowerShell:

```powershell
Invoke-WebRequest https://github.com/Ortham/testing-plugins/archive/refs/tags/1.6.2.zip -OutFile testing-plugins-1.6.2.zip
Expand-Archive testing-plugins-1.6.2.zip .
Move-Item testing-plugins-1.6.2 testing-plugins
Remove-Item testing-plugins-1.6.2.zip
```

The tests can then be run using:

```
cargo test
```

### API documentation

The public API has doc comments copied from libloot, and the API documentation can be built and viewed using:

```
cargo doc --open
```
