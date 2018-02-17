# LOOT

[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/loot/loot-api?branch=dev&svg=true)](https://ci.appveyor.com/project/WrinklyNinja/loot-api)
[![Travis Build Status](https://travis-ci.org/loot/loot-api.svg?branch=dev)](https://travis-ci.org/loot/loot-api)
[![Documentation Status](https://readthedocs.org/projects/loot-api/badge/?version=latest)](http://loot-api.readthedocs.io/en/latest/?badge=latest)

## Introduction

LOOT is a plugin load order optimisation tool for TES IV: Oblivion, TES V: Skyrim, TES V: Skyrim Special Edition, Fallout 3, Fallout: New Vegas, Fallout 4 and Fallout 4 VR. It is designed to assist mod users in avoiding detrimental conflicts, by automatically calculating a load order that satisfies all plugin dependencies and maximises each plugin's impact on the user's game.

LOOT also provides some load order error checking, including checks for requirements, incompatibilities and cyclic dependencies. In addition, it provides a large number of plugin-specific usage notes, bug warnings and Bash Tag suggestions.

The LOOT API provides access to LOOT's metadata and sorting functionality, and the LOOT application is built using it.

## Downloads

Releases are hosted on [GitHub](https://github.com/loot/loot-api/releases), and snapshot builds are available on [Bintray](https://bintray.com/wrinklyninja/loot/loot-api). The snapshot build archives are named like so:

```
loot_api-<last tag>-<revisions since tag>-g<short revision ID>_<branch>-<platform>.7z
```

## Building The LOOT API

### Windows

Refer to `appveyor.yml` for the build process.

### Linux

Refer to `.travis.yml` for the build process. A [local Docker image](https://docs.travis-ci.com/user/common-build-problems/#Troubleshooting-Locally-in-a-Docker-Image) can be used to ensure the assumed build environment.

### CMake Variables

LOOT uses the following CMake variables to set build parameters:

Parameter | Values | Default |Description
----------|--------|---------|-----------
`BUILD_SHARED_LIBS` | `ON`, `OFF` | `ON` | Whether or not to build a shared LOOT API binary.
`MSVC_STATIC_RUNTIME` | `ON`, `OFF` | `OFF` | Whether to link the C++ runtime statically or not when building with MSVC.

You may also need to set `BOOST_ROOT` if CMake cannot find Boost.

## Building The Documentation

The documentation is built using [Doxygen](http://www.stack.nl/~dimitri/doxygen/), [Breathe](https://breathe.readthedocs.io/en/latest/) and [Sphinx](http://www.sphinx-doc.org/en/stable/). Install Doxygen and Python (2 or 3) and make sure they're accessible from your `PATH`, then run:

```
pip install -r docs/api/requirements.txt
sphinx-build -b html docs build/docs/html
```
