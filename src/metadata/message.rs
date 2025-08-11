use std::collections::BTreeSet;

use saphyr::{MarkedYaml, Scalar, YamlData};

use super::{
    error::{
        ExpectedType, MetadataParsingErrorReason, MultilingualMessageContentsError,
        ParseMetadataError,
    },
    yaml::{
        EmitYaml, TryFromYaml, YamlEmitter, YamlObjectType, as_mapping, get_required_string_value,
        get_strings_vec_value, get_value, parse_condition,
    },
};

/// Codes used to indicate the type of a message.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[expect(
    clippy::exhaustive_enums,
    reason = "Adding a new message is a breaking change."
)]
pub enum MessageType {
    /// A notification message that is of no significant severity.
    #[default]
    Say,
    /// A warning message, used to indicate that an issue may be present that
    /// the user may wish to act on.
    Warn,
    /// An error message, used to indicate that an issue that requires user
    /// action is present.
    Error,
}

impl std::fmt::Display for MessageType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MessageType::Say => write!(f, "say"),
            MessageType::Warn => write!(f, "warn"),
            MessageType::Error => write!(f, "error"),
        }
    }
}

/// Represents a message's localised text content.
#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct MessageContent {
    text: Box<str>,
    language: Box<str>,
}

impl MessageContent {
    /// The code for the default language assumed for message content.
    pub const DEFAULT_LANGUAGE: &'static str = "en";

    /// Create a value with the given text in the default language.
    #[must_use]
    pub fn new(text: String) -> Self {
        MessageContent {
            text: text.into_boxed_str(),
            ..Default::default()
        }
    }

    /// Set the language to the given value.
    #[must_use]
    pub fn with_language(mut self, language: String) -> Self {
        self.language = language.into_boxed_str();
        self
    }

    /// Get the message text.
    pub fn text(&self) -> &str {
        &self.text
    }

    /// Get the text's language.
    pub fn language(&self) -> &str {
        &self.language
    }
}

impl std::default::Default for MessageContent {
    /// Create a value with an empty message string and the default language.
    fn default() -> Self {
        Self {
            text: Box::default(),
            language: MessageContent::DEFAULT_LANGUAGE.into(),
        }
    }
}

/// Choose a `MessageContent` object from those given in `content` based on
/// the given `language`.
///
/// Language strings are expected to have the form
/// `[language code]` or `[language code]_[country code]`, where
/// `[language code]` is an ISO 639-1 language code, and `[country code]` is an
/// ISO 3166 country code.
///
/// * If the vector only contains a single element, that element is returned.
/// * If content with a language that exactly matches the given `language` value
///   is present, that content is returned.
/// * If the given `language` value includes a country code and there is no
///   exact match but content for the same language code is present, that
///   content is returned.
/// * If the given `language` value has no country code and there is no exact
///   match but content for the same language code is present, that content is
///   returned.
/// * If no matches are found and content in the default language is present,
///   that content is returned.
/// * Otherwise, an empty `Option` is returned.
pub fn select_message_content<'a>(
    content: &'a [MessageContent],
    language: &str,
) -> Option<&'a MessageContent> {
    if content.is_empty() {
        None
    } else if let [c] = content {
        Some(c)
    } else {
        let language_code = language.split_once('_').map(|p| p.0);

        let mut matched = None;
        let mut english = None;

        for mc in content {
            if mc.language.as_ref() == language {
                return Some(mc);
            } else if matched.is_none() {
                if language_code.is_some_and(|c| c == mc.language.as_ref()) {
                    matched = Some(mc);
                } else if language_code.is_none()
                    && let Some((content_language_code, _)) = mc.language.split_once('_')
                    && content_language_code == language
                {
                    matched = Some(mc);
                }

                if mc.language.as_ref() == MessageContent::DEFAULT_LANGUAGE {
                    english = Some(mc);
                }
            }
        }

        if matched.is_some() {
            matched
        } else if english.is_some() {
            english
        } else {
            None
        }
    }
}

/// Represents a message with localisable text content.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Message {
    level: MessageType,
    content: Box<[MessageContent]>,
    condition: Option<Box<str>>,
}

impl Message {
    /// Create a value with the given type and a content string in the language
    /// given by [`MessageContent::DEFAULT_LANGUAGE`].
    #[must_use]
    pub fn new(message_type: MessageType, content: String) -> Self {
        Self {
            level: message_type,
            content: Box::new([MessageContent::new(content)]),
            condition: None,
        }
    }

    /// Create a value with the given type and content. If more than one
    /// `MessageContent` object is given, one must use
    /// the language given by [`MessageContent::DEFAULT_LANGUAGE`].
    pub fn multilingual(
        message_type: MessageType,
        content: Vec<MessageContent>,
    ) -> Result<Self, MultilingualMessageContentsError> {
        validate_message_contents(&content)?;

        Ok(Self {
            level: message_type,
            content: content.into_boxed_slice(),
            condition: None,
        })
    }

    /// Set the condition string.
    #[must_use]
    pub fn with_condition(mut self, condition: String) -> Self {
        self.condition = Some(condition.into_boxed_str());
        self
    }

    /// Get the message type.
    pub fn message_type(&self) -> MessageType {
        self.level
    }

    /// Get the message content.
    pub fn content(&self) -> &[MessageContent] {
        &self.content
    }

    /// Get the condition string.
    pub fn condition(&self) -> Option<&str> {
        self.condition.as_deref()
    }
}

pub(crate) fn validate_message_contents(
    contents: &[MessageContent],
) -> Result<(), MultilingualMessageContentsError> {
    if contents.len() > 1 {
        let english_string_exists = contents
            .iter()
            .any(|c| c.language.as_ref() == MessageContent::DEFAULT_LANGUAGE);

        if !english_string_exists {
            return Err(MultilingualMessageContentsError {});
        }
    }

    Ok(())
}

impl TryFromYaml for MessageContent {
    fn try_from_yaml(value: &MarkedYaml) -> Result<Self, ParseMetadataError> {
        let mapping = as_mapping(value, YamlObjectType::MessageContent)?;

        let text =
            get_required_string_value(value.span.start, mapping, "text", YamlObjectType::Message)?;

        let language =
            get_required_string_value(value.span.start, mapping, "lang", YamlObjectType::Message)?;

        Ok(MessageContent {
            text: text.into(),
            language: language.into(),
        })
    }
}

pub(crate) fn parse_message_contents_yaml(
    value: &MarkedYaml,
    key: &'static str,
    parent_yaml_type: YamlObjectType,
) -> Result<Box<[MessageContent]>, ParseMetadataError> {
    let contents = match &value.data {
        YamlData::Value(Scalar::String(s)) => Box::new([MessageContent::new(s.to_string())]),
        YamlData::Sequence(a) => a
            .iter()
            .map(MessageContent::try_from_yaml)
            .collect::<Result<Box<[_]>, _>>()?,
        _ => {
            return Err(ParseMetadataError::unexpected_value_type(
                value.span.start,
                key,
                parent_yaml_type,
                ExpectedType::ArrayOrString,
            ));
        }
    };

    if validate_message_contents(&contents).is_err() {
        Err(ParseMetadataError::new(
            value.span.start,
            MetadataParsingErrorReason::InvalidMultilingualMessageContents,
        ))
    } else {
        Ok(contents)
    }
}

impl TryFromYaml for Message {
    fn try_from_yaml(value: &MarkedYaml) -> Result<Self, ParseMetadataError> {
        let mapping = as_mapping(value, YamlObjectType::Message)?;

        let message_type =
            get_required_string_value(value.span.start, mapping, "type", YamlObjectType::Message)?;
        let message_type = match message_type {
            "warn" => MessageType::Warn,
            "error" => MessageType::Error,
            _ => MessageType::Say,
        };

        let mut content = match get_value(mapping, "content") {
            Some(n) => parse_message_contents_yaml(n, "content", YamlObjectType::Message)?,
            None => {
                return Err(ParseMetadataError::missing_key(
                    value.span.start,
                    "content",
                    YamlObjectType::Message,
                ));
            }
        };

        let subs = get_strings_vec_value(mapping, "subs", YamlObjectType::Message)?;

        if !subs.is_empty() {
            for mc in &mut content {
                mc.text = format(&mc.text, &subs)
                    .map_err(|e| ParseMetadataError::new(value.span.start, e))?;
            }
        }

        let condition = parse_condition(mapping, "condition", YamlObjectType::Message)?;

        Ok(Message {
            level: message_type,
            content,
            condition,
        })
    }
}

fn format(text: &str, subs: &[&str]) -> Result<Box<str>, MetadataParsingErrorReason> {
    let mut unused_sub_indexes = BTreeSet::new();
    for i in 0..subs.len() {
        unused_sub_indexes.insert(i);
    }

    let mut new_text = String::new();
    let mut maybe_in_placeholder = false;

    for slice in text.split_inclusive(['{', '}']) {
        if let Some(prefix) = slice.strip_suffix('{') {
            if maybe_in_placeholder {
                if prefix.is_empty() {
                    new_text.push_str(slice);
                } else {
                    new_text.push('{');
                    new_text.push_str(prefix);
                }
            } else {
                new_text.push_str(prefix);
                maybe_in_placeholder = true;
            }
        } else if let Some(prefix) = slice.strip_suffix('}') {
            if maybe_in_placeholder {
                if let Ok(sub_index) = prefix.parse::<usize>() {
                    let Some(sub) = subs.get(sub_index) else {
                        return Err(MetadataParsingErrorReason::MissingSubstitution(format!(
                            "{{{prefix}}}"
                        )));
                    };

                    new_text.push_str(sub);

                    unused_sub_indexes.remove(&sub_index);
                } else {
                    // Not a valid placeholder, treat it as normal text.
                    new_text.push('{');
                    new_text.push_str(slice);
                }

                maybe_in_placeholder = false;
            } else {
                new_text.push_str(slice);
            }
        } else {
            if maybe_in_placeholder {
                new_text.push('{');
                maybe_in_placeholder = false;
            }
            new_text.push_str(slice);
        }
    }

    if maybe_in_placeholder {
        new_text.push('{');
    }

    if let Some(sub_index) = unused_sub_indexes.first()
        && let Some(sub) = subs.get(*sub_index)
    {
        Err(MetadataParsingErrorReason::MissingPlaceholder(
            (*sub).to_owned(),
            *sub_index,
        ))
    } else {
        Ok(new_text.into_boxed_str())
    }
}

impl EmitYaml for MessageContent {
    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        emitter.begin_map();

        emitter.map_key("lang");
        emitter.unquoted_str(&self.language);

        emitter.map_key("text");
        emitter.single_quoted_str(&self.text);

        emitter.end_map();
    }
}

pub(super) fn emit_message_contents(
    slice: &[MessageContent],
    emitter: &mut YamlEmitter,
    key: &'static str,
) {
    match slice {
        [] => {}
        [detail] => {
            emitter.map_key(key);
            emitter.single_quoted_str(detail.text());
        }
        details => {
            emitter.map_key(key);

            details.emit_yaml(emitter);
        }
    }
}

impl EmitYaml for Message {
    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        emitter.begin_map();

        emitter.map_key("type");
        emitter.unquoted_str(&self.level.to_string());

        emit_message_contents(&self.content, emitter, "content");

        if let Some(condition) = &self.condition {
            emitter.map_key("condition");
            emitter.single_quoted_str(condition);
        }

        emitter.end_map();
    }
}

#[cfg(test)]
mod tests {
    use crate::metadata::emit;

    use super::*;

    mod select_message_content {
        use super::*;

        #[test]
        fn should_return_none_if_the_slice_is_empty() {
            let content = select_message_content(&[], MessageContent::DEFAULT_LANGUAGE);

            assert!(content.is_none());
        }

        #[test]
        fn should_return_the_only_element_of_a_single_element_slice() {
            let slice = &[MessageContent::new("test".into()).with_language("de".into())];
            let content = select_message_content(slice, "fr").unwrap();

            assert_eq!(&slice[0], content);
        }

        #[test]
        fn should_return_element_with_exactly_matching_locale_code() {
            let slice = &[
                MessageContent::new("test1".into()).with_language("en".into()),
                MessageContent::new("test2".into()).with_language("de".into()),
                MessageContent::new("test3".into()).with_language("pt".into()),
                MessageContent::new("test4".into()).with_language("pt_PT".into()),
                MessageContent::new("test5".into()).with_language("pt_BR".into()),
            ];

            let content = select_message_content(slice, "pt_BR").unwrap();

            assert_eq!(&slice[4], content);
        }

        #[test]
        fn should_return_element_with_matching_language_code_if_exactly_matching_local_code_is_not_present()
         {
            let slice = &[
                MessageContent::new("test1".into()).with_language("en".into()),
                MessageContent::new("test2".into()).with_language("de".into()),
                MessageContent::new("test3".into()).with_language("pt".into()),
                MessageContent::new("test4".into()).with_language("pt_PT".into()),
            ];

            let content = select_message_content(slice, "pt_BR").unwrap();

            assert_eq!(&slice[2], content);
        }

        #[test]
        fn should_return_element_with_en_language_code_if_no_matching_language_code_is_present() {
            let slice = &[
                MessageContent::new("test1".into()).with_language("en".into()),
                MessageContent::new("test2".into()).with_language("de".into()),
                MessageContent::new("test3".into()).with_language("pt_PT".into()),
            ];

            let content = select_message_content(slice, "pt_BR").unwrap();

            assert_eq!(&slice[0], content);
        }

        #[test]
        fn should_return_element_with_exactly_matching_language_code_if_language_code_is_given() {
            let slice = &[
                MessageContent::new("test1".into()).with_language("en".into()),
                MessageContent::new("test2".into()).with_language("de".into()),
                MessageContent::new("test3".into()).with_language("pt_BR".into()),
                MessageContent::new("test4".into()).with_language("pt".into()),
            ];

            let content = select_message_content(slice, "pt").unwrap();

            assert_eq!(&slice[3], content);
        }

        #[test]
        fn should_return_first_element_with_matching_language_code_if_language_code_is_given_and_no_exact_match_is_present()
         {
            let slice = &[
                MessageContent::new("test1".into()).with_language("en".into()),
                MessageContent::new("test2".into()).with_language("de".into()),
                MessageContent::new("test3".into()).with_language("pt_PT".into()),
                MessageContent::new("test4".into()).with_language("pt_BR".into()),
            ];

            let content = select_message_content(slice, "pt").unwrap();

            assert_eq!(&slice[2], content);
        }

        #[test]
        fn should_return_none_if_there_is_no_match_and_no_english_text() {
            let slice = &[
                MessageContent::new("test2".into()).with_language("de".into()),
                MessageContent::new("test3".into()).with_language("pt_PT".into()),
                MessageContent::new("test4".into()).with_language("pt_BR".into()),
            ];

            assert!(select_message_content(slice, "fr").is_none());
        }
    }

    mod message_content {
        use super::*;

        mod try_from_yaml {
            use crate::metadata::parse;

            use super::*;

            #[test]
            fn should_error_if_given_a_scalar() {
                let yaml = parse("content");

                assert!(MessageContent::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_error_if_given_a_list() {
                let yaml = parse("[0, 1, 2]");

                assert!(MessageContent::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_set_all_given_fields() {
                let yaml = parse("{text: content, lang: fr}");

                let content = MessageContent::try_from_yaml(&yaml).unwrap();

                assert_eq!("content", content.text());
                assert_eq!("fr", content.language());
            }
        }

        mod emit_yaml {
            use super::*;

            #[test]
            fn should_emit_map() {
                let content = MessageContent::new("message".into()).with_language("fr".into());
                let yaml = emit(&content);

                assert_eq!(
                    format!("lang: {}\ntext: '{}'", content.language, content.text),
                    yaml
                );
            }
        }
    }

    mod message {
        use super::*;

        mod try_from_yaml {
            use crate::metadata::parse;

            use super::*;

            #[test]
            fn should_error_if_given_a_scalar() {
                let yaml = parse("content");

                assert!(Message::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_error_if_given_a_list() {
                let yaml = parse("[0, 1, 2]");

                assert!(Message::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_error_if_content_is_missing() {
                let yaml = parse("{type: say}");

                assert!(Message::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_error_if_given_an_invalid_condition() {
                let yaml = parse("{type: say, content: text, condition: invalid}");

                assert!(Message::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_set_all_given_fields() {
                let yaml = parse("{type: say, content: text, condition: 'file(\"Foo.esp\")'}");

                let message = Message::try_from_yaml(&yaml).unwrap();

                assert_eq!(MessageType::Say, message.message_type());
                assert_eq!(&[MessageContent::new("text".into())], message.content());
                assert_eq!("file(\"Foo.esp\")", message.condition().unwrap());
            }

            #[test]
            fn should_leave_optional_fields_empty_if_not_present() {
                let yaml = parse("{type: say, content: text}");

                let message = Message::try_from_yaml(&yaml).unwrap();

                assert_eq!(MessageType::Say, message.message_type());
                assert_eq!(&[MessageContent::new("text".into())], message.content());
                assert!(message.condition().is_none());
            }

            #[test]
            fn should_set_say_warn_and_error_message_types() {
                let yaml = parse("{type: say, content: text}");

                let message = Message::try_from_yaml(&yaml).unwrap();
                assert_eq!(MessageType::Say, message.message_type());

                let yaml = parse("{type: warn, content: text}");

                let message = Message::try_from_yaml(&yaml).unwrap();
                assert_eq!(MessageType::Warn, message.message_type());

                let yaml = parse("{type: error, content: text}");

                let message = Message::try_from_yaml(&yaml).unwrap();
                assert_eq!(MessageType::Error, message.message_type());
            }

            #[test]
            fn should_use_say_if_message_type_is_unrecognised() {
                let yaml = parse("{type: info, content: text}");

                let message = Message::try_from_yaml(&yaml).unwrap();
                assert_eq!(MessageType::Say, message.message_type());
            }

            #[test]
            fn should_read_all_listed_message_contents() {
                let yaml = parse(
                    "{type: say, content: [{text: english, lang: en}, {text: french, lang: fr}]}",
                );

                let message = Message::try_from_yaml(&yaml).unwrap();

                assert_eq!(
                    &[
                        MessageContent::new("english".into()),
                        MessageContent::new("french".into()).with_language("fr".into())
                    ],
                    message.content()
                );
            }

            #[test]
            fn should_not_error_if_one_content_object_is_given_and_it_is_not_english() {
                let yaml = parse("type: say\ncontent:\n  - lang: fr\n    text: content1");

                let message = Message::try_from_yaml(&yaml).unwrap();

                assert_eq!(
                    &[MessageContent::new("content1".into()).with_language("fr".into())],
                    message.content()
                );
            }

            #[test]
            fn should_error_if_multiple_contents_are_given_and_none_are_english() {
                let yaml = parse(
                    "type: say\ncontent:\n  - lang: de\n    text: content1\n  - lang: fr\n    text: content2",
                );

                assert!(Message::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_apply_substitutions_when_there_is_only_one_content_string() {
                let yaml = parse("type: say\ncontent: con{0}tent1\nsubs:\n  - sub1");

                let message = Message::try_from_yaml(&yaml).unwrap();

                assert_eq!("consub1tent1", message.content()[0].text());
            }

            #[test]
            fn should_apply_substitutions_to_all_content_strings() {
                let yaml = parse(
                    "type: say\ncontent:\n  - lang: en\n    text: content1 {0}\n  - lang: fr\n    text: content2 {0}\nsubs:\n  - sub",
                );

                let message = Message::try_from_yaml(&yaml).unwrap();

                assert_eq!("content1 sub", message.content()[0].text());
                assert_eq!("content2 sub", message.content()[1].text());
            }

            #[test]
            fn should_error_if_the_message_has_more_substitutions_than_expected() {
                let yaml = parse("{type: say, content: 'content1', subs: [sub1]}");

                assert!(Message::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_error_if_the_content_string_expects_more_substitutions_than_exist() {
                let yaml = parse("{type: say, content: '{0} {1}', subs: [sub1]}");

                assert!(Message::try_from_yaml(&yaml).is_err());
            }

            #[test]
            fn should_ignore_substution_syntax_if_no_substitutions_exist() {
                let yaml = parse("{type: say, content: 'content {0}'}");

                let message = Message::try_from_yaml(&yaml).unwrap();

                assert_eq!("content {0}", message.content()[0].text());
            }
        }

        mod format {
            use super::*;

            #[test]
            fn should_treat_invalid_placeholders_as_literals() {
                let format_no_subs = |i| format(i, &[]).unwrap();

                let input = "a{";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{a";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{}";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{a}";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a}";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{ }";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{0a}";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{{";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{b{";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a{b{c";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a}}b";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a}b}";
                assert_eq!(input, format_no_subs(input).as_ref());

                let input = "a}b}c";
                assert_eq!(input, format_no_subs(input).as_ref());

                assert_eq!("a{b", format("a{{0}", &["b"]).unwrap().as_ref());

                assert_eq!("ab}", format("a{0}}", &["b"]).unwrap().as_ref());
            }

            #[test]
            fn allows_leading_zeroes_in_placeholders() {
                let input = "a{0}-{01}";
                assert_eq!("ab-c", format(input, &["b", "c"]).unwrap().as_ref());
            }
        }

        mod emit_yaml {
            use super::*;

            #[test]
            fn should_emit_say_message_type_correctly() {
                let message = Message::new(MessageType::Say, "message".into());
                let yaml = emit(&message);

                assert_eq!(
                    format!("type: say\ncontent: '{}'", message.content[0].text),
                    yaml
                );
            }

            #[test]
            fn should_emit_warn_message_type_correctly() {
                let message = Message::new(MessageType::Warn, "message".into());
                let yaml = emit(&message);

                assert_eq!(
                    format!("type: warn\ncontent: '{}'", message.content[0].text),
                    yaml
                );
            }

            #[test]
            fn should_emit_error_message_type_correctly() {
                let message = Message::new(MessageType::Error, "message".into());
                let yaml = emit(&message);

                assert_eq!(
                    format!("type: error\ncontent: '{}'", message.content[0].text),
                    yaml
                );
            }

            #[test]
            fn should_emit_condition_if_it_is_not_empty() {
                let message = Message::new(MessageType::Say, "message".into())
                    .with_condition("condition1".into());
                let yaml = emit(&message);

                assert_eq!(
                    format!(
                        "type: {}\ncontent: '{}'\ncondition: '{}'",
                        message.level,
                        message.content[0].text,
                        message.condition.unwrap()
                    ),
                    yaml
                );
            }

            #[test]
            fn should_emit_a_content_array_if_content_is_multilingual() {
                let message = Message::multilingual(
                    MessageType::Say,
                    vec![
                        MessageContent::new("english".into()).with_language("en".into()),
                        MessageContent::new("french".into()).with_language("fr".into()),
                    ],
                )
                .unwrap();
                let yaml = emit(&message);

                assert_eq!(
                    format!(
                        "type: {}
content:
  - lang: {}
    text: '{}'
  - lang: {}
    text: '{}'",
                        message.level,
                        message.content[0].language(),
                        message.content[0].text(),
                        message.content[1].language(),
                        message.content[1].text()
                    ),
                    yaml
                );
            }
        }
    }
}
