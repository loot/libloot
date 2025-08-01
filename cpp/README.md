# libloot C++ wrapper

This is a wrapper around libloot that provides a C++ interface that's ABI-compatible with libloot v0.27.0.

The wrapper has two layers:

- a static library built using Cargo, which provides a C++ interface
- a shared library built using CMake, which wraps that C++ interface to provide another that is more idiomatic.

## Build

The prerequisites for building libloot and its C++ wrapper are [CMake](https://cmake.org/), the [Rust](https://www.rust-lang.org/) toolchain and a C++ toolchain. The CI builds currently use a recent version of CMake, the latest version of Rust, MSVC 2022 on Windows and GCC 13 on Linux, so alternatives such as other versions, Mingw-w64 or Clang may not work without modifications.

To build a release build with debug info on Windows:

```
cmake -B build .
cmake --build build --parallel --config RelWithDebInfo
```

To do the same on Linux:

```
cmake -B build . -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

To build a debug build, pass `Debug` instead of `RelWithDebInfo`.

The following CMake variables can be used to configure the build:

Parameter | Values | Default |Description
----------|--------|---------|-----------
`LIBLOOT_BUILD_SHARED` | `ON`, `OFF` | `ON` | Whether or not to build a shared libloot binary.
`LIBLOOT_BUILD_TESTS` | `ON`, `OFF` | `ON` | Whether or not to build libloot's tests.
`LIBLOOT_INSTALL_DOCS` | `ON`, `OFF` | `ON` | Whether or not to install libloot's docs (which need to be built separately).
`RUN_CLANG_TIDY` | `ON`, `OFF` | `OFF` | Whether or not to run clang-tidy during build. Has no effect when using CMake's Visual Studio generator.

An example of using libloot with CMake's FetchContent:

```cmake
set(LIBLOOT_BUILD_TESTS OFF)
set(LIBLOOT_INSTALL_DOCS OFF)

FetchContent_Declare(libloot
    GIT_REPOSITORY "https://github.com/loot/libloot.git"
    GIT_TAG "master" # Better to use a specific commit hash.
    SOURCE_SUBDIR "cpp")

FetchContent_MakeAvailable(libloot)

add_executable(myapp ${MYAPP_SOURCES})
target_link_libraries(myapp PRIVATE libloot::loot)
```

### Documentation

Install [Doxygen](https://www.doxygen.nl/), [Python](https://www.python.org/) and [uv](https://docs.astral.sh/uv/getting-started/installation/) and make sure they're accessible from your `PATH`, then run:

```
uv run --directory ../docs -- sphinx-build -b html . build/html
```

## Tests

If the tests are built they can be run using:

```
ctest --test-dir build --output-on-failure --parallel -V
```

## Packaging

To package the build:

```
cpack --config build/CPackConfig.cmake -C RelWithDebInfo
```
