# libloot-nodejs

An **experimental** Node.js wrapper around the libloot Rust implementation, built using [NAPI-RS](https://napi.rs/).

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

- All errors are thrown as JavaScript `Error` values. The error message is the concatenated display text of all the recursive Rust errors from the libloot crate.
