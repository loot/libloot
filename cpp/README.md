# libloot-rs C++ wrapper

This is an **experimental** wrapper around the Rust reimplementation of libloot that provides a C++ interface that's ABI-compatible with libloot v0.26.1.

The wrapper has two layers:

- a static library built using Cargo, which provides a C++ interface
- a shared library built using CMake, which wraps that C++ interface to provide another that is ABI-compatible with C++ libloot.

## Building

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

### Tests & Packaging

The build process also builds a copy of the public API tests from C++ libloot v0.26.1 by default. To skip building the tests, pass `-DLIBLOOT_BUILD_TESTS=OFF` when first running CMake.

If built, the tests can be run using:

```
ctest --test-dir build --output-on-failure --parallel -V
```

To package the build:

```
cpack --config build/CPackConfig.cmake -C RelWithDebInfo
```

This repository (and so the created package) doesn't currently include any of libloot's documentation, besides the API documentation included in the Rust source code.

## Usage notes

For the first layer of the wrapper, built using Cargo:

- `LIBLOOT_VERSION_MAJOR`, `LIBLOOT_VERSION_MINOR` and `LIBLOOT_VERSION_PATCH` are exposed as `extern "C"` `static unsigned int` globals.
- CXX doesn't provide integration between Rust's `Option<_>` and C++'s `std::optional<T>`, so sentinel values are used to communicate the absence of a value in the wrapper's API:
    - For `Option<&str>`, `None` is represented using an empty string, which is how it's already done for many metadata object fields in the C++ implementation of libloot.
    - For `Option<f32>`, `None` is represented using `NaN`, since the C++ implementation of libloot's public API already says that `NaN` values get converted to `None`.
    - Plugin CRCs (which are `Option<u32>` in Rust) are represented as `int64_t`, with `-1` representing `None`, since all `uint32_t` values are possibly valid CRCs.
- All Rust errors are converted to `::rust::Error` exceptions that have a `what()` string that is the concatenation of the error's display string and all its recursive source error display strings. For some errors the `what()` string also includes some data that is parsed by the wrapper's second layer to differentiate certain error types.

For the ABI-compatible second layer of the wrapper, built using CMake:

- Some exceptions have changed type: they all still derive from `std::exception`, but for example some `std::logic_error` and `std::invalid_argument` exceptions have become `std::runtime_error` and `FileAccessError` exceptions, some `YAML::RepresentationException` exceptions have become `FileAccessError` exceptions, etc.
- Exception messages are generally not expected to be the same between the two implementations.
