# libloot-pyo3

An **experimental** Python wrapper around the libloot Rust implementation, built using [PyO3](https://pyo3.rs).

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
- The `LogLevel` enum and `set_logging_callback()` and `set_log_level()` functions are not exposed because the logging is integrated with Python's `logging` module instead.
