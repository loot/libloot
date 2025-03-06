# libloot-rs C++ wrapper

This is an **imcomplete** and **experimental** wrapper around the Rust reimplementation of libloot that provides a C++ interface.

## Current status

- [x] Expose all API types, aside from `LogLevel` and error types
- [x] Expose all getters and setters on metadata types
- [x] Expose all other methods on `PluginMetadata`
- [x] Expose creator functions (`new_*`) for all metadata types
- [x] Expose `Group::DEFAULT_NAME` (using an accessor function)
- [x] Expose `MessageContent::DEFAULT_LANGUAGE` (using an accessor function)
- [x] Expose `select_message_content()`
- [x] Expose `is_compatible()`
- [x] Expose `libloot_revision()`
- [x] Expose `libloot_version()`
- [x] Expose `LIBLOOT_VERSION_*` constants (as unmangled extern C statics)
- [ ] Expose `LogLevel`
- [ ] Expose `set_logging_callback()`
- [ ] Write tests to cover API usage patterns
- [ ] Write a C++ layer that provides the same API as the C++ implementation of libloot

## Building

The wrapper is currently built as a static library using Cargo and cbindgen.

```
cargo build --release
cbindgen -o include/libloot.h
```

cbindgen is needed to create a header that contains the C FFI exports. It needs to be v0.28.0 or higher to support the Rust 2024 edition's unsafe attributes. The header needs to be included into C++ code separately from the CXX-generated header. While CXX does support including headers, it produces the wrong include path in the C++ it generates due to this crate being a member of a workspace, so the C++ side expects the header in a subdirectory that doesn't exist.

There are also a small number of C++ tests that can be built and run using CMake:

```
cmake -B build .
cmake --build build --config RelWithDebInfo
ctest --test-dir build --output-on-failure -V
```

To package the build:

```
cpack --config build/CPackConfig.cmake
```

This repository (and so the created package) doesn't currently include any of libloot's documentation, besides the API documentation included in the Rust source code.

## Usage notes

- `LIBLOOT_VERSION_MAJOR`, `LIBLOOT_VERSION_MINOR` and `LIBLOOT_VERSION_PATCH` are exposed as `extern "C"` `static unsigned int` globals.
- CXX doesn't provide integration between Rust's `Option<_>` and C++'s `std::optional<T>`, so sentinel values are used to communicate the absence of a value in the wrapper's API:
    - For `Option<&str>`, `None` is represented using an empty string, which is how it's already done for many metadata object fields in the C++ implementation of libloot.
    - For `Option<f32>`, `None` is represented using `NaN`, since the C++ implementation of libloot's public API already says that `NaN` values get converted to `None`.
    - Plugin CRCs (which are `Option<u32>` in Rust) are represented as `int64_t`, with `-1` representing `None`, since all `uint32_t` values are possibly valid CRCs.
- All Rust errors are converted to `::rust::Error` exceptions that have a `what()` string that is the concatenation of the error's display string and all its recursive source error display strings.
