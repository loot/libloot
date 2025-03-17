# libloot-pyo3

An **incomplete** and **experimental** Python wrapper around the libloot Rust implementation, built using [PyO3](https://pyo3.rs).

## Current coverage

- [x] `LIBLOOT_VERSION_MAJOR`
- [x] `LIBLOOT_VERSION_MINOR`
- [x] `LIBLOOT_VERSION_PATCH`
- [x] `is_compatible()`
- [x] `libloot_revision()`
- [x] `libloot_version()`
- [ ] `set_logging_callback()`
- [ ] `set_log_level()`
- [x] `EdgeType`
- [x] `GameType`
- [ ] `LogLevel`
- [x] `Database`
- [x] `Game`
- [x] `Plugin`
- [x] `Vertex`
- [x] `File`
- [x] `Filename`
- [x] `Group`
- [x] `Location`
- [x] `Message`
- [x] `MessageContent`
- [x] `PluginCleaningData`
- [x] `PluginMetadata`
- [x] `Tag`
- [x] `MessageType`
- [x] `TagSuggestion`
- [x] `select_message_content()`
- [x] Error types

## Build

To build, first set up a Python venv and install [maturin](https://github.com/PyO3/maturin):

```
python -m venv .venv
.\.venv\Scripts\activate
pip install maturin
maturin develop
```

Then build the library:

```
.\.venv\Scripts\activate
maturin develop
```

Then import it in Python:

```
py
> import loot
```

## Usage notes

- The Python exceptions that errors are mapped to are not the same as in the Rust or C++ interfaces:
    - The API provides the custom `CyclicInteractionError`, `UndefinedGroupError`, `EspluginError` exception types.
    - All other errors are raised as `ValueError` exceptions.
    - There's no equivalent to the C++ interface's `FileAccessError` or `ConditionSyntaxError` classes or the libloadorder and loot-condition-interpreter system error categories.
