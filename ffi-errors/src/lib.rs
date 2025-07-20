#![allow(clippy::missing_errors_doc)]

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct UnsupportedEnumValueError;

impl std::fmt::Display for UnsupportedEnumValueError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Enum value is unsupported")
    }
}

impl std::error::Error for UnsupportedEnumValueError {}

pub fn fmt_error_chain(
    mut error: &dyn std::error::Error,
    f: &mut std::fmt::Formatter<'_>,
) -> std::fmt::Result {
    write!(f, "{error}")?;
    while let Some(source) = error.source() {
        write!(f, ": {source}")?;
        error = source;
    }
    Ok(())
}

#[macro_export]
macro_rules! variant_box_from_error {
    ( $from_type:ident, $to_type:ident::$to_variant:ident ) => {
        impl From<$from_type> for $to_type {
            fn from(value: $from_type) -> Self {
                Self::$to_variant(Box::new(value))
            }
        }
    };
}
