# libloot-rs C++ wrapper

This is a wrapper around libloot that provides a C++ interface that's ABI-compatible with libloot v0.27.0.

The wrapper has two layers:

- a static library built using Cargo, which provides a C++ interface
- a shared library built using CMake, which wraps that C++ interface to provide another that is ABI-compatible with C++ libloot.

## Building

libloot uses the following CMake variables to set build parameters:

Parameter | Values | Default |Description
----------|--------|---------|-----------
`BUILD_SHARED_LIBS` | `ON`, `OFF` | `ON` | Whether or not to build a shared libloot binary.
`LIBLOOT_BUILD_TESTS` | `ON`, `OFF` | `ON` | Whether or not to build libloot's tests.
`LIBLOOT_INSTALL_DOCS` | `ON`, `OFF` | `ON` | Whether or not to install libloot's docs (which need to be built separately).
`RUN_CLANG_TIDY` | `ON`, `OFF` | `OFF` | Whether or not to run clang-tidy during build. Has no effect when using CMake's MSVC generator.

### Windows

To build a release build with debug info:

```
cmake -B build .
cmake --build build --parallel --config RelWithDebInfo
```

To build a debug build:

```
cmake -B build .
cmake --build build --parallel --config Debug
```

### Linux

To build a release build with debug info:

```
cmake -B build . -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

to build a debug build:

```
cargo build
cmake -B build . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### Tests

The build process also builds the test suite by default. To skip building the tests, pass `-DLIBLOOT_BUILD_TESTS=OFF` when first running CMake.

If built, the tests can be run using:

```
ctest --test-dir build --output-on-failure --parallel -V
```

### Documentation

Install [Doxygen](https://www.doxygen.nl/), Python and [uv](https://docs.astral.sh/uv/getting-started/installation/) and make sure they're accessible from your `PATH`, then run:

```
cd ../docs
uv run -- sphinx-build -b html . build/html
```

### Packaging

To package the build:

```
cpack --config build/CPackConfig.cmake -C RelWithDebInfo
```
