use saphyr::YamlData;

use super::error::ExpectedType;
use super::error::ParseMetadataError;
use super::yaml::{YamlObjectType, get_required_string_value, parse_condition};
use super::yaml_emit::EmitYaml;
use super::yaml_emit::YamlEmitter;

/// Represents whether a Bash Tag suggestion is for addition or removal.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum TagSuggestion {
    #[default]
    Addition,
    Removal,
}

/// Represents a Bash Tag suggestion for a plugin.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Tag {
    name: String,
    suggestion: TagSuggestion,
    condition: Option<String>,
}

impl Tag {
    /// Create a [Tag] suggestion for the given tag name.
    #[must_use]
    pub fn new(name: String, suggestion: TagSuggestion) -> Self {
        Self {
            name,
            suggestion,
            condition: None,
        }
    }

    /// Set the condition string.
    #[must_use]
    pub fn with_condition(mut self, condition: String) -> Self {
        self.condition = Some(condition);
        self
    }

    /// Get the tag's name.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get if the tag should be added.
    pub fn is_addition(&self) -> bool {
        self.suggestion == TagSuggestion::Addition
    }

    /// Get the condition string.
    pub fn condition(&self) -> Option<&str> {
        self.condition.as_deref()
    }
}

impl TryFrom<&saphyr::MarkedYaml> for Tag {
    type Error = ParseMetadataError;

    fn try_from(value: &saphyr::MarkedYaml) -> Result<Self, Self::Error> {
        match &value.data {
            YamlData::String(s) => {
                let (name, suggestion) = name_and_suggestion(s);
                Ok(Tag {
                    name,
                    suggestion,
                    condition: None,
                })
            }
            YamlData::Hash(h) => {
                let name =
                    get_required_string_value(value.span.start, h, "name", YamlObjectType::Tag)?;

                let condition = parse_condition(h, YamlObjectType::Tag)?;

                let (name, suggestion) = name_and_suggestion(name);
                Ok(Tag {
                    name,
                    suggestion,
                    condition,
                })
            }
            _ => Err(ParseMetadataError::unexpected_type(
                value.span.start,
                YamlObjectType::Tag,
                ExpectedType::MapOrString,
            )),
        }
    }
}

fn name_and_suggestion(value: &str) -> (String, TagSuggestion) {
    if let Some(name) = value.strip_prefix("-") {
        (name.to_string(), TagSuggestion::Removal)
    } else {
        (value.to_string(), TagSuggestion::Addition)
    }
}

impl EmitYaml for Tag {
    fn is_scalar(&self) -> bool {
        self.condition.is_none()
    }

    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        if let Some(condition) = &self.condition {
            emitter.begin_map();

            emitter.map_key("name");
            if self.is_addition() {
                emitter.unquoted_str(&self.name);
            } else {
                emitter.unquoted_str(&format!("-{}", self.name));
            }

            emitter.map_key("condition");
            emitter.single_quoted_str(condition);

            emitter.end_map();
        } else if self.is_addition() {
            emitter.unquoted_str(&self.name);
        } else {
            emitter.unquoted_str(&format!("-{}", self.name));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    mod emit_yaml {
        use crate::metadata::emit;

        use super::*;

        #[test]
        fn should_emit_name_only_if_unconditional_addition() {
            let tag = Tag::new("name1".into(), TagSuggestion::Addition);
            let yaml = emit(&tag);

            assert_eq!(tag.name(), yaml);
        }

        #[test]
        fn should_emit_map_if_there_is_a_condition() {
            let tag =
                Tag::new("name1".into(), TagSuggestion::Removal).with_condition("condition".into());
            let yaml = emit(&tag);

            assert_eq!("name: -name1\ncondition: 'condition'", yaml);
        }
    }
}
