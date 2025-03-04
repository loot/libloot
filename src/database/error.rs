use crate::metadata::error::RegexError;

/// Represents an error that occurred while evaluating a metadata condition.
#[derive(Debug)]
pub struct ConditionEvaluationError(Box<loot_condition_interpreter::Error>);

impl std::fmt::Display for ConditionEvaluationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "failed to evaluate condition")
    }
}

impl std::error::Error for ConditionEvaluationError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.0)
    }
}

impl From<loot_condition_interpreter::Error> for ConditionEvaluationError {
    fn from(value: loot_condition_interpreter::Error) -> Self {
        ConditionEvaluationError(Box::new(value))
    }
}

/// Represents an error that occurred while retrieving metadata for a plugin.
#[derive(Debug)]
pub enum MetadataRetrievalError {
    ConditionEvaluationError(ConditionEvaluationError),
    RegexError(RegexError),
}

impl std::fmt::Display for MetadataRetrievalError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "failed to retrieve metadata")
    }
}

impl std::error::Error for MetadataRetrievalError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::ConditionEvaluationError(e) => Some(e),
            Self::RegexError(e) => Some(e),
        }
    }
}

impl From<loot_condition_interpreter::Error> for MetadataRetrievalError {
    fn from(value: loot_condition_interpreter::Error) -> Self {
        MetadataRetrievalError::ConditionEvaluationError(value.into())
    }
}

impl From<RegexError> for MetadataRetrievalError {
    fn from(value: RegexError) -> Self {
        MetadataRetrievalError::RegexError(value)
    }
}
