use saphyr::{MarkedYaml, Scalar, YamlData};
use unicase::UniCase;

use super::{
    error::{ExpectedType, MultilingualMessageContentsError, ParseMetadataError},
    message::{
        MessageContent, emit_message_contents, parse_message_contents_yaml,
        validate_message_contents,
    },
    yaml::{
        EmitYaml, TryFromYaml, YamlEmitter, YamlObjectType, as_string_node,
        get_required_string_value, get_string_value, parse_condition,
    },
};

/// Represents a file in a game's Data folder, including files in
/// subdirectories.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct File {
    name: Filename,
    display_name: Option<Box<str>>,
    detail: Box<[MessageContent]>,
    condition: Option<Box<str>>,
    constraint: Option<Box<str>>,
}

impl File {
    /// Construct a [File] with the given name. This can also be a relative path.
    #[must_use]
    pub fn new(name: String) -> Self {
        Self {
            name: Filename::new(name),
            ..Default::default()
        }
    }

    /// Set the name to be displayed for the file in messages, formatted using
    /// CommonMark.
    #[must_use]
    pub fn with_display_name(mut self, display_name: String) -> Self {
        self.display_name = Some(display_name.into_boxed_str());
        self
    }

    /// Set the condition string.
    #[must_use]
    pub fn with_condition(mut self, condition: String) -> Self {
        self.condition = Some(condition.into_boxed_str());
        self
    }

    /// Set the detail message content, which may be appended to any messages
    /// generated for this file. If multilingual, one language must be
    /// [MessageContent::DEFAULT_LANGUAGE].
    pub fn with_detail(
        mut self,
        detail: Vec<MessageContent>,
    ) -> Result<Self, MultilingualMessageContentsError> {
        validate_message_contents(&detail)?;
        self.detail = detail.into_boxed_slice();
        Ok(self)
    }

    /// Set the constraint string.
    #[must_use]
    pub fn with_constraint(mut self, constraint: String) -> Self {
        self.constraint = Some(constraint.into_boxed_str());
        self
    }

    /// Gets the name of the file (which may actually be a path).
    pub fn name(&self) -> &Filename {
        &self.name
    }

    /// Get the display name of the file.
    pub fn display_name(&self) -> Option<&str> {
        self.display_name.as_deref()
    }

    /// Get the detail message content of the file.
    ///
    /// If this file causes an error message to be displayed, the detail message
    /// content should be appended to that message, as it provides more detail
    /// about the error (e.g. suggestions for how to resolve it).
    pub fn detail(&self) -> &[MessageContent] {
        &self.detail
    }

    /// Get the condition string.
    pub fn condition(&self) -> Option<&str> {
        self.condition.as_deref()
    }

    /// Get the constraint string.
    pub fn constraint(&self) -> Option<&str> {
        self.constraint.as_deref()
    }
}

/// Represents a case-insensitive filename.
#[derive(Clone, Debug, Default)]
pub struct Filename(Box<str>);

impl Filename {
    /// Construct a Filename using the given string.
    #[must_use]
    pub fn new(s: String) -> Self {
        Filename(s.into())
    }

    /// Get this Filename as a string.
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl PartialEq for Filename {
    fn eq(&self, other: &Self) -> bool {
        unicase::eq(&self.0, &other.0)
    }
}

impl Eq for Filename {}

impl PartialOrd for Filename {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Filename {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        UniCase::new(&self.0).cmp(&UniCase::new(&other.0))
    }
}

impl std::hash::Hash for Filename {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        UniCase::new(&self.0).hash(state);
    }
}

impl AsRef<str> for &Filename {
    fn as_ref(&self) -> &str {
        &self.0
    }
}

impl std::fmt::Display for Filename {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.0.fmt(f)
    }
}

impl TryFromYaml for File {
    fn try_from_yaml(value: &MarkedYaml) -> Result<Self, ParseMetadataError> {
        match &value.data {
            YamlData::Value(Scalar::String(s)) => Ok(File {
                name: Filename::new(s.to_string()),
                display_name: None,
                detail: Box::default(),
                condition: None,
                constraint: None,
            }),
            YamlData::Mapping(h) => {
                let name =
                    get_required_string_value(value.span.start, h, "name", YamlObjectType::File)?;

                let display_name = get_string_value(h, "display", YamlObjectType::File)?;

                let detail = match h.get(&as_string_node("detail")) {
                    Some(n) => parse_message_contents_yaml(
                        n,
                        "detail",
                        YamlObjectType::PluginCleaningData,
                    )?,
                    None => Box::default(),
                };

                let condition = parse_condition(h, "condition", YamlObjectType::File)?;

                let constraint = parse_condition(h, "constraint", YamlObjectType::File)?;

                Ok(File {
                    name: Filename::new(name.to_owned()),
                    display_name: display_name.map(|(_, s)| s.into()),
                    detail,
                    condition,
                    constraint,
                })
            }
            _ => Err(ParseMetadataError::unexpected_type(
                value.span.start,
                YamlObjectType::File,
                ExpectedType::MapOrString,
            )),
        }
    }
}

impl EmitYaml for File {
    fn is_scalar(&self) -> bool {
        self.condition.is_none()
            && self.constraint.is_none()
            && self.detail.is_empty()
            && self.display_name.is_none()
    }

    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        if self.is_scalar() {
            emitter.single_quoted_str(self.name.as_str());
        } else {
            emitter.begin_map();

            emitter.map_key("name");
            emitter.single_quoted_str(self.name.as_str());

            if let Some(display_name) = &self.display_name {
                emitter.map_key("display");
                emitter.single_quoted_str(display_name);
            }

            if let Some(condition) = &self.condition {
                emitter.map_key("condition");
                emitter.single_quoted_str(condition);
            }

            if let Some(constraint) = &self.constraint {
                emitter.map_key("constraint");
                emitter.single_quoted_str(constraint);
            }

            emit_message_contents(&self.detail, emitter, "detail");

            emitter.end_map();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    mod file_eq {
        use super::*;

        #[test]
        fn should_be_case_insensitive_on_name() {
            assert_eq!(File::new("name".into()), File::new("name".into()));
            assert_eq!(File::new("name".into()), File::new("NAME".into()));
            assert_ne!(File::new("name1".into()), File::new("name2".into()));
        }
    }

    mod filename_eq {
        use super::*;

        #[test]
        fn should_be_case_insensitive_on_name() {
            assert_eq!(Filename::new("name".into()), Filename::new("name".into()));
            assert_eq!(Filename::new("name".into()), Filename::new("NAME".into()));
            assert_ne!(Filename::new("name1".into()), Filename::new("name2".into()));
        }
    }

    mod try_from_yaml {
        use crate::metadata::parse;

        use super::*;

        #[test]
        fn should_only_set_name_if_decoding_from_scalar() {
            let yaml = parse("name1");

            let file = File::try_from_yaml(&yaml).unwrap();

            assert_eq!("name1", file.name().as_str());
            assert!(file.display_name().is_none());
            assert!(file.condition().is_none());
            assert!(file.constraint().is_none());
            assert!(file.detail().is_empty());
        }

        #[test]
        fn should_error_if_given_a_list() {
            let yaml = parse("[0, 1, 2]");

            assert!(File::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_error_if_name_is_missing() {
            let yaml = parse("{display: display1}");

            assert!(File::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_error_if_given_an_invalid_condition() {
            let yaml = parse("{name: name1, condition: invalid}");

            assert!(File::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_error_if_given_an_invalid_constraint() {
            let yaml = parse("{name: name1, constraint: invalid}");

            assert!(File::try_from_yaml(&yaml).is_err());
        }

        #[test]
        fn should_set_all_given_fields() {
            let yaml = parse(
                "{name: name1, display: display1, condition: 'file(\"Foo.esp\")', constraint: 'file(\"Bar.esp\")', detail: 'details'}",
            );

            let file = File::try_from_yaml(&yaml).unwrap();

            assert_eq!("name1", file.name().as_str());
            assert_eq!("display1", file.display_name().unwrap());
            assert_eq!("file(\"Foo.esp\")", file.condition().unwrap());
            assert_eq!("file(\"Bar.esp\")", file.constraint().unwrap());
            assert_eq!(&[MessageContent::new("details".into())], file.detail());
        }

        #[test]
        fn should_leave_optional_fields_empty_if_not_present() {
            let yaml = parse("{name: name1}");

            let file = File::try_from_yaml(&yaml).unwrap();

            assert_eq!("name1", file.name().as_str());
            assert!(file.display_name().is_none());
            assert!(file.condition().is_none());
            assert!(file.constraint().is_none());
            assert!(file.detail().is_empty());
        }

        #[test]
        fn should_read_all_listed_detail_message_contents() {
            let yaml = parse(
                "{name: name1, detail: [{text: english, lang: en}, {text: french, lang: fr}]}",
            );

            let file = File::try_from_yaml(&yaml).unwrap();

            assert_eq!(
                &[
                    MessageContent::new("english".into()),
                    MessageContent::new("french".into()).with_language("fr".into())
                ],
                file.detail()
            );
        }

        #[test]
        fn should_not_error_if_one_detail_is_given_and_it_is_not_english() {
            let yaml = parse("name: name1\ndetail:\n  - lang: fr\n    text: content1");

            let file = File::try_from_yaml(&yaml).unwrap();

            assert_eq!(
                &[MessageContent::new("content1".into()).with_language("fr".into())],
                file.detail()
            );
        }

        #[test]
        fn should_error_if_multiple_details_are_given_and_none_are_english() {
            let yaml = parse(
                "name: name1\ndetail:\n  - lang: de\n    text: content1\n  - lang: fr\n    text: content2",
            );

            assert!(File::try_from_yaml(&yaml).is_err());
        }
    }

    mod emit_yaml {
        use crate::metadata::emit;

        use super::*;

        #[test]
        fn should_emit_only_name_scalar_if_other_fields_are_empty() {
            let file = File::new("filename".into());
            let yaml = emit(&file);

            assert_eq!(format!("'{}'", file.name.as_str()), yaml);
        }

        #[test]
        fn should_emit_map_with_display_if_display_name_is_not_empty() {
            let file = File::new("filename".into()).with_display_name("display1".into());
            let yaml = emit(&file);

            assert_eq!(
                format!(
                    "name: '{}'\ndisplay: '{}'",
                    file.name.as_str(),
                    file.display_name.unwrap()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_condition_if_it_is_not_empty() {
            let file = File::new("filename".into()).with_condition("condition1".into());
            let yaml = emit(&file);

            assert_eq!(
                format!(
                    "name: '{}'\ncondition: '{}'",
                    file.name.as_str(),
                    file.condition.unwrap()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_constraint_if_it_is_not_empty() {
            let file = File::new("filename".into()).with_constraint("constraint1".into());
            let yaml = emit(&file);

            assert_eq!(
                format!(
                    "name: '{}'\nconstraint: '{}'",
                    file.name.as_str(),
                    file.constraint.unwrap()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_a_detail_string_if_detail_is_monolingual() {
            let file = File::new("filename".into())
                .with_detail(vec![MessageContent::new("message".into())])
                .unwrap();
            let yaml = emit(&file);

            assert_eq!(
                format!(
                    "name: '{}'\ndetail: '{}'",
                    file.name.as_str(),
                    file.detail[0].text()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_a_detail_array_if_detail_is_multilingual() {
            let file = File::new("filename".into())
                .with_detail(vec![
                    MessageContent::new("english".into()).with_language("en".into()),
                    MessageContent::new("french".into()).with_language("fr".into()),
                ])
                .unwrap();
            let yaml = emit(&file);

            assert_eq!(
                format!(
                    "name: '{}'
detail:
  - lang: {}
    text: '{}'
  - lang: {}
    text: '{}'",
                    file.name.as_str(),
                    file.detail[0].language(),
                    file.detail[0].text(),
                    file.detail[1].language(),
                    file.detail[1].text()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_all_fields_set() {
            let file = File::new("filename".into())
                .with_display_name("display1".into())
                .with_condition("condition1".into())
                .with_constraint("constraint1".into())
                .with_detail(vec![
                    MessageContent::new("english".into()).with_language("en".into()),
                    MessageContent::new("french".into()).with_language("fr".into()),
                ])
                .unwrap();
            let yaml = emit(&file);

            assert_eq!(
                format!(
                    "name: '{}'
display: '{}'
condition: '{}'
constraint: '{}'
detail:
  - lang: {}
    text: '{}'
  - lang: {}
    text: '{}'",
                    file.name.as_str(),
                    file.display_name.unwrap(),
                    file.condition.unwrap(),
                    file.constraint.unwrap(),
                    file.detail[0].language(),
                    file.detail[0].text(),
                    file.detail[1].language(),
                    file.detail[1].text()
                ),
                yaml
            );
        }
    }
}
