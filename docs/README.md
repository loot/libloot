# libloot Documentation

This directory contains documentation for libloot and LOOT's metadata syntax. It does not include Rust API reference documentation: that is generated using `cargo doc`.

To build the documentation, install [Doxygen](https://www.doxygen.nl/), Python and [uv](https://docs.astral.sh/uv/getting-started/installation/) and make sure they're accessible from your `PATH`, then run:

```
cd ../docs
uv run -- sphinx-build -b html . build/html
```

The documentation files for dependency licenses and copyright notices are auto-generated using [cargo-attribution](https://github.com/ameknite/cargo-attribution): to regenerate them run `py scripts/licenses.py`. The script is hardcoded to generate files for the C++ wrapper's dependencies, but in practice that doesn't add any content over doing the same for just the core libloot Rust library.
