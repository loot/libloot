use saphyr::{MarkedYaml, Scalar, YamlData};

use super::{
    error::{ExpectedType, ParseMetadataError},
    yaml::{
        EmitYaml, TryFromYaml, YamlEmitter, YamlObjectType, get_required_string_value,
        parse_condition,
    },
};

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
    name: Box<str>,
    suggestion: TagSuggestion,
    condition: Option<Box<str>>,
}

impl Tag {
    /// Create a [Tag] suggestion for the given tag name.
    #[must_use]
    pub fn new(name: String, suggestion: TagSuggestion) -> Self {
        Self {
            name: name.into_boxed_str(),
            suggestion,
            condition: None,
        }
    }

    /// Set the condition string.
    #[must_use]
    pub fn with_condition(mut self, condition: String) -> Self {
        self.condition = Some(condition.into_boxed_str());
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

impl TryFromYaml for Tag {
    fn try_from_yaml(value: &MarkedYaml) -> Result<Self, ParseMetadataError> {
        match &value.data {
            YamlData::Value(Scalar::String(s)) => {
                let (name, suggestion) = name_and_suggestion(s);
                Ok(Tag {
                    name,
                    suggestion,
                    condition: None,
                })
            }
            YamlData::Mapping(h) => {
                let name =
                    get_required_string_value(value.span.start, h, "name", YamlObjectType::Tag)?;

                let condition = parse_condition(h, "condition", YamlObjectType::Tag)?;

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

fn name_and_suggestion(value: &str) -> (Box<str>, TagSuggestion) {
    if let Some(name) = value.strip_prefix("-") {
        (name.into(), TagSuggestion::Removal)
    } else {
        (value.into(), TagSuggestion::Addition)
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

    mod try_from_yaml {
        use crate::metadata::parse;

        use super::*;

        #[test]
        fn should_only_set_name_and_suggestion_if_decoding_from_scalar() {
            let yaml = parse("Relev");

            let tag = Tag::try_from_yaml(&yaml).unwrap();

            assert_eq!("Relev", tag.name());
            assert!(tag.is_addition());
            assert!(tag.condition().is_none());
        }

        #[test]
        fn should_only_set_name_and_suggestion_if_decoding_from_scalar_with_leading_hyphen() {
            let yaml = parse("-Relev");

            let tag = Tag::try_from_yaml(&yaml).unwrap();

            assert_eq!("Relev", tag.name());
            assert!(!tag.is_addition());
            assert!(tag.condition().is_none());
        }

        #[test]
        fn should_error_if_given_a_list() {
            let yaml = parse("[0, 1, 2]");

            assert!(Tag::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_error_if_name_is_missing() {
            let yaml = parse("{condition: 'file(\"Foo.esp\")'}");

            assert!(Tag::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_error_if_given_an_invalid_condition() {
            let yaml = parse("{name: Relev, condition: invalid}");

            assert!(Tag::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_set_all_fields() {
            let yaml = parse("{name: Relev, condition: 'file(\"Foo.esp\")'}");

            let tag = Tag::try_from_yaml(&yaml).unwrap();

            assert_eq!("Relev", tag.name());
            assert!(tag.is_addition());
            assert_eq!("file(\"Foo.esp\")", tag.condition().unwrap());
        }

        #[test]
        fn should_leave_optional_fields_empty_if_not_present() {
            let yaml = parse("{name: Relev}");

            let tag = Tag::try_from_yaml(&yaml).unwrap();

            assert_eq!("Relev", tag.name());
            assert!(tag.is_addition());
            assert!(tag.condition().is_none());
        }
    }

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
