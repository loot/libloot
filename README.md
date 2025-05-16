# libloot-rs

This is an **experimental** reimplementation of [libloot](https://github.com/loot/libloot) using Rust instead of C++, that should match libloot v0.26.2.

There is one intentional difference in behaviour: if a plugin has metadata but it is all filtered out when conditions are evaluated, getting its metadata returns `None` instead of a name-only PluginMetadata object.

## Introduction

LOOT is a plugin load order optimisation tool for Starfield, TES III: Morrowind, TES IV: Oblivion, TES IV: Oblivion Remastered, TES V: Skyrim, TES V: Skyrim Special Edition, TES V: Skyrim VR, Fallout 3, Fallout: New Vegas, Fallout 4, Fallout 4 VR and OpenMW. It is designed to assist mod users in avoiding detrimental conflicts, by automatically calculating a load order that satisfies all plugin dependencies and maximises each plugin's impact on the user's game.

LOOT also provides some load order error checking, including checks for requirements, incompatibilities and cyclic dependencies. In addition, it provides a large number of plugin-specific usage notes, bug warnings and Bash Tag suggestions.

libloot provides access to LOOT's metadata and sorting functionality, and the LOOT application is built using it.

libloot's core is written in Rust, and C++, Python and Node.js wrappers can be found in the `cpp`, `python` and `nodejs` subdirectories respectively.

## Downloads

Releases are hosted on [GitHub](https://github.com/loot/libloot/releases).

Snapshot builds are available as artifacts from [GitHub Actions runs](https://github.com/loot/libloot/actions), though they are only kept for 90 days and can only be downloaded when logged into a GitHub account. To mitigate these restrictions, snapshot build artifacts include a GPG signature that can be verified using the public key hosted [here](https://loot.github.io/.well-known/openpgpkey/hu/mj86by43a9hz8y8rbddtx54n3bwuuucg), which means it's possible to re-upload the artifacts elsewhere and still prove their authenticity.

The snapshot build artifacts are named like so:

```
libloot-<last tag>-<revisions since tag>-g<short revision ID>_<branch>-<platform>.<file extension>
```

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

Before running the tests, first extract the [testing-plugins](https://github.com/Ortham/testing-plugins) archive to this readme's directory (so that there's a `testing-plugins` directory there).

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

The API documentation can be built and viewed using:

```
cargo doc --open
```

The C++ wrapper also has more general documentation.
