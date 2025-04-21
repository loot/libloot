# libloot-nodejs

An **experimental** Node.js wrapper around the libloot Rust implementation, built using [NAPI-RS](https://napi.rs/docs/concepts/class).

## Build

To build, first install Rust and Node.js, then run:

```
npm install
npm run build
```

The tests can be run using:

```
npm test
```

## Usage notes

- All errors are thrown as JavaScript `Error` values. The error message is the concatenated display text of all the recursive Rust errors from the libloot crate, with the exception of `PluginDataError` errors, which are turned into messages of the form `esplugin error, code <code>: <display text>`, where `<code>` is one of the esplugin error codes currently exposed by the C++ implementation of libloot.
