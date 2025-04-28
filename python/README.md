# libloot-python

An **experimental** Python wrapper around the libloot Rust implementation, built using [PyO3](https://pyo3.rs).

## Build

To build, first set up a Python virtual environment and install [maturin](https://github.com/PyO3/maturin):

```powershell
python -m venv .venv
.\.venv\Scripts\activate
pip install maturin
```

or in a POSIX shell:

```sh
python -m venv .venv
. .venv/bin/activate
pip install maturin
```

Then build the library in the virtual environment:

```
maturin develop
```

The library can then be imported in Python:

```
python
> import loot
```

## Usage notes

- The Python exceptions that errors are mapped to are not the same as in the Rust or C++ interfaces:
    - The API provides the custom `CyclicInteractionError`, `UndefinedGroupError`, `EspluginError` exception types.
    - All other errors are raised as `ValueError` exceptions.
    - There's no equivalent to the C++ interface's `FileAccessError` or `ConditionSyntaxError` classes or the libloadorder and loot-condition-interpreter system error categories.
- The `LogLevel` enum and `set_logging_callback()` and `set_log_level()` functions are not exposed because the logging is integrated with Python's `logging` module instead.
