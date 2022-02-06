# libloot

![CI](https://github.com/loot/libloot/workflows/CI/badge.svg?branch=master&event=push)
[![Documentation Status](https://readthedocs.org/projects/loot-api/badge/?version=latest)](http://loot-api.readthedocs.io/en/latest/?badge=latest)

## Introduction

LOOT is a plugin load order optimisation tool for TES III: Morrowind, TES IV: Oblivion, TES V: Skyrim, TES V: Skyrim Special Edition, Fallout 3, Fallout: New Vegas, Fallout 4 and Fallout 4 VR. It is designed to assist mod users in avoiding detrimental conflicts, by automatically calculating a load order that satisfies all plugin dependencies and maximises each plugin's impact on the user's game.

LOOT also provides some load order error checking, including checks for requirements, incompatibilities and cyclic dependencies. In addition, it provides a large number of plugin-specific usage notes, bug warnings and Bash Tag suggestions.

libloot provides access to LOOT's metadata and sorting functionality, and the LOOT application is built using it.

## Downloads

Releases are hosted on [GitHub](https://github.com/loot/libloot/releases), and snapshot builds are available on [Artifactory](https://loot.jfrog.io/ui/repos/tree/General/libloot). The snapshot build archives are named like so:

```
libloot-<last tag>-<revisions since tag>-g<short revision ID>_<branch>-<platform>.7z
```

## Building libloot

Refer to `.github/workflows/release.yml` for the build process.

### Linux

The build process assumes that you have already cloned the libloot repository,
that the current working directory is its root, and that the following
applications are already installed:

- `cmake`
- `curl`
- `git`
- `pip3` (and therefore Python 3)
- `cargo` and the rest of the Rust toolchain (e.g. via
  [rustup](https://rustup.rs/))
- `wget`

The list above may be incomplete.

### CMake Variables

libloot uses the following CMake variables to set build parameters:

Parameter | Values | Default |Description
----------|--------|---------|-----------
`BUILD_SHARED_LIBS` | `ON`, `OFF` | `ON` | Whether or not to build a shared libloot binary.

You may also need to set `BOOST_ROOT` if CMake cannot find Boost.

## Building The Documentation

The documentation is built using [Doxygen](http://www.stack.nl/~dimitri/doxygen/), [Breathe](https://breathe.readthedocs.io/en/latest/) and [Sphinx](http://www.sphinx-doc.org/en/stable/). Install Doxygen and Python (2 or 3) and make sure they're accessible from your `PATH`, then run:

```
pip install -r docs/requirements.txt
sphinx-build -b html docs build/docs/html
```

Alternatively, you can use Docker to avoid changing your development environment, by running `docker run -it --rm -v ${PWD}/docs:/docs/docs -v ${PWD}/build:/docs/build sphinxdoc/sphinx:4.2.0 bash` to obtain a shell that you can use to run `apt-get update && apt-get install -y doxygen` and then the two commands above.
