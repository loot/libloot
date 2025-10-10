# libloot Documentation

This directory contains documentation for libloot and LOOT's metadata syntax. It does not include Rust API reference documentation: that is generated using `cargo doc`.

To build the documentation, install [Python](https://www.python.org/) and [uv](https://docs.astral.sh/uv/getting-started/installation/) and make sure they're accessible from your `PATH`, then run:

```
uv run -- sphinx-build -b html . build/html
```

If [Doxygen](https://www.doxygen.nl/) is also installed and accessible from your `PATH`, the C++ API's reference documentation will also be included.

The files for dependency licenses and copyright notices are auto-generated. The script is hardcoded to generate files for the C++ wrapper's dependencies, but in practice that doesn't add any content over doing the same for just the core libloot Rust library.

Run `scripts/licenses.py` to regenerate the generated files, e.g. `py scripts/licenses.py` or `python3 scripts/licenses.py`.
