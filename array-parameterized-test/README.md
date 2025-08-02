# array-parameterized-test

A few macros to support parameterized test cases where the inputs are defined using an const array. This can be used to concisely define several different parameterized tests that share the same set of inputs.

Use it like this:

```rust
use array_parameterized_test::{test_parameter, parameterized_test};

#[test_parameter]
const INT_INPUTS: [u32; 2] = [1, 2];

#[parameterized_test(INT_INPUTS)]
fn test_with_ints(value: u32) {
    assert!(value > 0 && value < 3);
}
```

which produces tests that look like this:

```
test test_with_ints::_1 ... ok
test test_with_ints::_2 ... ok
```

See the tests for more examples.

The macros that implement this functionality may not work with arbitrary array element types. They've been tested with:

- u32
- &str
- (u32, bool)
- Unit-only enums
