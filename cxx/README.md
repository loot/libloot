# libloot-rs C++ wrapper

This is an **imcomplete** and **experimental** wrapper around the Rust reimplementation of libloot that provides a C++ interface that's ABI-compatible with libloot v0.25.3.

## Current status

- [x] `EdgeType`
- [x] `GameType`
- [x] `LogLevel`
- [x] `MessageType`
- [ ] `ConditionSyntaxError`
- [ ] `CyclicInteractionError`
- [ ] `FileAccessError`
- [ ] `UndefinedGroupError`
- [ ] `std::system_error` with esplugin `std::error_category`
- [ ] `std::system_error` with libloadorder `std::error_category`
- [ ] `std::system_error` with loot-condition-interpreter `std::error_category`
- [x] `ConditionalMetadata`
- [x] `File`
- [x] `Filename`
- [x] `Group`
- [x] `Location`
- [x] `MessageContent`
- [x] `SelectMessageContent`
- [x] `Message`
- [x] `PluginCleaningData`
- [x] `PluginMetadata`
- [x] `Tag`
- [x] `SetLoggingCallback`
- [x] `IsCompatible`
- [x] `CreateGameHandle`
- [x] `DatabaseInterface`
- [x] `GameInterface`
- [x] `LIBLOOT_VERSION_MAJOR`
- [x] `LIBLOOT_VERSION_MINOR`
- [x] `LIBLOOT_VERSION_PATCH`
- [x] `GetLiblootVersion`
- [x] `GetLiblootRevision`
- [x] `PluginInterface`
- [x] `Vertex`

The error types are defined but currently unused, causing behavioural differences when errors occur - see the implementation notes below for details.

## Building

The wrapper is currently built in two layers:

- a static library built using Cargo, which provides a C++ interface
- a shared library built using CMake, which wraps that C++ interface to provide another that is ABI-compatible with C++ libloot.

To build the wrapper:

```
cargo build --release
cmake -B build .
cmake --build build --config RelWithDebInfo
```

This also builds a copy of the public API tests from C++ libloot v0.25.3, which can be run using:

```
ctest --test-dir build --output-on-failure -V
```

Some of the tests currently fail: they are all expecting the exception classes that aren't yet used.

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
