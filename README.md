# NoU programming language

Simple language, targeting [WASM](https://webassembly.org/).

## Getting started

To compile the U compiler, run:

```shell
make u
```

By default `CC` is set to `clang`, you can change that in `Makefile`.

To run tests (NOTE: `nodejs` is required to run tests; I use `20.5.0`), run:

```shell
make test
```

## Examples

```
export add;

add := fn a: i32, b: i32 -> i32 {
    return a + b;
}
```

Example code can be found in `tests/`.
