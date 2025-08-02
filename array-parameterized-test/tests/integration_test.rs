use array_parameterized_test::{parameterized_test, test_parameter};

#[derive(Copy, Clone, Eq, PartialEq)]
enum TestValue {
    A,
    B,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test_parameter]
    const INT_INPUTS: [u32; 2] = [1, 2];

    #[test_parameter]
    const ENUM_INPUTS: [TestValue; 2] = [TestValue::A, TestValue::B];

    #[test_parameter]
    const TUPLE_INPUTS: [(u32, bool); 5] =
        [(1, false), (2, true), (3, false), (4, true), (5, false)];

    #[expect(clippy::non_ascii_literal)]
    #[test_parameter]
    const NON_ASCII_INPUTS: [&str; 2] = ["nonÁscii", "藏"];

    #[parameterized_test(INT_INPUTS)]
    fn test_with_ints(value: u32) {
        assert!(value > 0 && value < 3);
    }

    #[parameterized_test(ENUM_INPUTS)]
    fn test_with_enums(value: TestValue) {
        assert!(value == TestValue::A || value == TestValue::B);
    }

    #[parameterized_test(NON_ASCII_INPUTS)]
    fn test_with_non_ascii_strings(value: &str) {
        assert!(!value.is_ascii());
    }

    #[parameterized_test(TUPLE_INPUTS)]
    fn test_with_tuples(value: (u32, bool)) {
        assert_eq!(value.1, value.0.is_multiple_of(2));
    }

    #[parameterized_test(TUPLE_INPUTS)]
    fn test_with_tuples_destructured((value, expected): (u32, bool)) {
        assert_eq!(expected, value.is_multiple_of(2));
    }
}
