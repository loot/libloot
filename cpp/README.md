# libloot-rs C++ wrapper

This is a wrapper around libloot that provides a C++ interface that's ABI-compatible with libloot v0.26.1.

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

The documentation is built using [Doxygen](http://www.stack.nl/~dimitri/doxygen/), [Breathe](https://breathe.readthedocs.io/en/latest/) and [Sphinx](http://www.sphinx-doc.org/en/stable/). Install Doxygen and Python and make sure they're accessible from your `PATH`, then run:

```
py -m venv .venv
.venv\Scripts\activate
pip install -r docs/requirements.txt
sphinx-build -b html docs build/docs/html
```

If running on Linux, replace `.venv\Scripts\activate` with `.venv/bin/activate`.

Alternatively, you can use Docker to avoid changing your development environment, by running `docker run -it --rm -v ${PWD}/docs:/docs/docs -v ${PWD}/build:/docs/build -v ${PWD}/include:/docs/include sphinxdoc/sphinx:7.3.7 bash` to obtain a shell that you can use to run `apt-get update && apt-get install -y doxygen` and then the two commands above.

The documentation files for dependency licenses and copyright notices are auto-generated using [cargo-attribution](https://github.com/ameknite/cargo-attribution): to regenerate them run `py scripts/licenses.py`.

### Packaging

To package the build:

```
cpack --config build/CPackConfig.cmake -C RelWithDebInfo
```

## Usage notes

For the first layer of the wrapper, built using Cargo:

- `LIBLOOT_VERSION_MAJOR`, `LIBLOOT_VERSION_MINOR` and `LIBLOOT_VERSION_PATCH` are exposed as `extern "C"` `static unsigned int` globals.
- CXX doesn't provide integration between Rust's `Option<_>` and C++'s `std::optional<T>`, so sentinel values are used to communicate the absence of a value in the wrapper's API:
    - For `Option<&str>`, `None` is represented using an empty string, which is how it's already done for many metadata object fields in the C++ implementation of libloot.
    - For `Option<f32>`, `None` is represented using `NaN`, since the C++ implementation of libloot's public API already says that `NaN` values get converted to `None`.
- All Rust errors are converted to `::rust::Error` exceptions that have a `what()` string that is the concatenation of the error's display string and all its recursive source error display strings. For some errors the `what()` string also includes some data that is parsed by the wrapper's second layer to differentiate certain error types.

For the ABI-compatible second layer of the wrapper, built using CMake:

- Some exceptions have changed type: they all still derive from `std::exception`, but for example some `std::logic_error` and `std::invalid_argument` exceptions have become `std::runtime_error` and `FileAccessError` exceptions, some `YAML::RepresentationException` exceptions have become `FileAccessError` exceptions, etc.
- Exception messages are generally not expected to be the same between the two implementations.
