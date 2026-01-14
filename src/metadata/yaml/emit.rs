use std::collections::{HashMap, HashSet};

use crate::metadata::{Message, MessageContent};

pub(in crate::metadata) trait EmitYaml {
    fn is_scalar(&self) -> bool {
        false
    }

    fn has_written_anchor(&self, _: &YamlAnchors) -> bool {
        false
    }

    fn emit_yaml(&self, emitter: &mut YamlEmitter);
}

#[derive(Clone, Debug, Eq, PartialEq, Default)]
pub(in crate::metadata) struct YamlAnchors<'a> {
    message: HashMap<&'a Message, String>,
    message_contents: HashMap<&'a [MessageContent], String>,
    file: HashMap<&'a crate::metadata::File, String>,
    condition: HashMap<&'a str, String>,
    written_anchors: HashSet<String>,
}

impl<'a> YamlAnchors<'a> {
    pub(in crate::metadata) fn new() -> Self {
        YamlAnchors {
            message: HashMap::new(),
            message_contents: HashMap::new(),
            file: HashMap::new(),
            condition: HashMap::new(),
            written_anchors: HashSet::new(),
        }
    }

    pub(in crate::metadata) fn set_message_anchors(
        &mut self,
        message_anchors: HashMap<&'a Message, String>,
    ) {
        self.message = message_anchors;
    }

    pub(in crate::metadata) fn set_message_contents_anchors(
        &mut self,
        message_contents_anchors: HashMap<&'a [MessageContent], String>,
    ) {
        self.message_contents = message_contents_anchors;
    }

    pub(in crate::metadata) fn set_file_anchors(
        &mut self,
        file_anchors: HashMap<&'a crate::metadata::File, String>,
    ) {
        self.file = file_anchors;
    }

    pub(in crate::metadata) fn set_condition_anchors(
        &mut self,
        condition_anchors: HashMap<&'a str, String>,
    ) {
        self.condition = condition_anchors;
    }

    pub(in crate::metadata) fn record_written_anchor(&mut self, anchor_name: String) {
        self.written_anchors.insert(anchor_name);
    }

    pub(in crate::metadata) fn message_anchor(&self, message: &Message) -> Option<&String> {
        self.message.get(message)
    }

    pub(in crate::metadata) fn message_contents_anchor(
        &self,
        message_contents: &[MessageContent],
    ) -> Option<&String> {
        self.message_contents.get(message_contents)
    }

    pub(in crate::metadata) fn file_anchor(&self, file: &crate::metadata::File) -> Option<&String> {
        self.file.get(file)
    }

    pub(in crate::metadata) fn condition_anchor(&self, condition: &str) -> Option<&String> {
        self.condition.get(condition)
    }

    pub(in crate::metadata) fn is_anchor_written(&self, anchor_name: &str) -> bool {
        self.written_anchors.contains(anchor_name)
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(in crate::metadata) struct YamlEmitter<'a> {
    buffer: String,
    scope: Vec<YamlBlock>,
    style: YamlStyle,
    is_first_line_of_map: bool,
    pub(in crate::metadata) anchors: YamlAnchors<'a>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
enum YamlBlock {
    Array,
    Map,
    Anchor,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
enum YamlStyle {
    /// YAML flow style
    Flow,
    /// YAML block style
    ///
    /// This is only respected for sequences. Mappings and scalars are always
    /// emitted in flow style.
    Block,
}

impl<'a> YamlEmitter<'a> {
    const INDENT_UNIT: &'static str = "  ";
    const ARRAY_ELEMENT_PREFIX: &'static str = "- ";

    pub(in crate::metadata) fn new(anchors: YamlAnchors<'a>) -> Self {
        Self {
            buffer: String::new(),
            scope: vec![],
            style: YamlStyle::Block,
            is_first_line_of_map: false,
            anchors,
        }
    }

    fn set_style(&mut self, style: YamlStyle) {
        self.style = style;
    }

    pub(in crate::metadata) fn into_string(self) -> String {
        self.buffer
    }

    fn begin_anchored_element(&mut self, anchor_name: &str) {
        self.write_prefix();

        self.write("&");
        self.write(anchor_name);

        self.anchors.record_written_anchor(anchor_name.to_owned());
        self.scope.push(YamlBlock::Anchor);
    }

    fn end_anchored_element(&mut self) {
        if self.scope.last() == Some(&YamlBlock::Anchor) {
            self.scope.pop();
        }
    }

    /// Writes an anchor if the anchor hasn't yet been written, otherwise writes
    /// an alias of that anchor.
    fn write_alias(&mut self, anchor_name: &str) {
        self.write_prefix();

        self.write("*");
        self.write(anchor_name);
    }

    pub(in crate::metadata) fn write_anchored_value<F, W>(
        &mut self,
        get_anchor: F,
        write_element: W,
    ) where
        F: Fn(&YamlAnchors) -> Option<String>,
        W: Fn(&mut Self),
    {
        if let Some(anchor) = get_anchor(&self.anchors) {
            let is_anchor_written = self.anchors.is_anchor_written(&anchor);
            if is_anchor_written {
                self.write_alias(&anchor);
            } else {
                self.begin_anchored_element(&anchor);
                write_element(self);
                self.end_anchored_element();
            }
        } else {
            write_element(self);
        }
    }

    pub(in crate::metadata) fn write_condition(&mut self, condition: &str) {
        self.write_anchored_value(
            |a| a.condition_anchor(condition).cloned(),
            |e| e.write_single_quoted_str(condition),
        );
    }

    pub(in crate::metadata) fn write_unquoted_str(&mut self, value: &str) {
        self.write_string(value, true);
    }

    pub(in crate::metadata) fn write_single_quoted_str(&mut self, value: &str) {
        self.write_string(value, false);
    }

    fn write_string(&mut self, value: &str, prefer_unquoted: bool) {
        self.write_prefix();

        if prefer_unquoted && can_emit_unquoted(value, self.style) {
            self.write(value);
        } else if can_single_quote(value) {
            self.write(&single_quote(value));
        } else {
            self.write(&double_quote(value));
        }
    }

    pub(in crate::metadata) fn write_u32(&mut self, value: u32) {
        self.write_prefix();

        self.write(&value.to_string());
    }

    pub(in crate::metadata) fn begin_map(&mut self) {
        if self.scope.last() == Some(&YamlBlock::Array) {
            self.write_array_element_prefix();
        }

        let in_anchored_element = self.scope.last() == Some(&YamlBlock::Anchor);

        self.scope.push(YamlBlock::Map);

        self.is_first_line_of_map = !in_anchored_element;
    }

    pub(in crate::metadata) fn end_map(&mut self) {
        if self.scope.last() == Some(&YamlBlock::Map) {
            self.scope.pop();
        }

        self.is_first_line_of_map = false;
    }

    /// This assumes that the given key is valid to be written as an unquoted
    /// string, and expects a string literal so that it's obvious that a given
    /// value is valid.
    pub(in crate::metadata) fn write_map_key(&mut self, key: &'static str) {
        if !self.is_first_line_of_map {
            self.write_end_of_line();
            self.write_indent();
        }

        self.write(key);
        self.write(":");

        self.is_first_line_of_map = false;
    }

    pub(in crate::metadata) fn begin_array(&mut self) {
        if self.style == YamlStyle::Flow {
            if self.scope.last() == Some(&YamlBlock::Map) {
                self.write(" ");
            }
            self.write("[");
        }

        self.scope.push(YamlBlock::Array);
    }

    pub(in crate::metadata) fn end_array(&mut self) {
        if self.scope.last() == Some(&YamlBlock::Array) {
            self.scope.pop();
        }

        if self.style == YamlStyle::Flow {
            self.write("]");
        }
    }

    pub(in crate::metadata) fn write_end_of_line(&mut self) {
        self.write("\n");
    }

    fn write_indent(&mut self) {
        if !self.scope.is_empty() {
            let scope_count = self
                .scope
                .iter()
                .filter(|s| **s != YamlBlock::Anchor)
                .skip(1) // Top-level scope needs no indentation.
                .count();

            for _ in 0..scope_count {
                self.write(Self::INDENT_UNIT);
            }
        }
    }

    fn write_prefix(&mut self) {
        match self.scope.last() {
            Some(&YamlBlock::Array) => self.write_array_element_prefix(),
            Some(&YamlBlock::Map | &YamlBlock::Anchor) => self.write(" "),
            None => self.write_indent(),
        }
    }

    fn write_array_element_prefix(&mut self) {
        if self.style == YamlStyle::Block {
            self.write_end_of_line();
            self.write_indent();
            self.write(Self::ARRAY_ELEMENT_PREFIX);
        }
    }

    fn write(&mut self, value: &str) {
        self.buffer += value;
    }
}

fn is_yaml_whitespace(c: char) -> bool {
    // <https://yaml.org/spec/1.2.2/#rule-s-white>
    c == ' ' || c == '\t'
}

fn is_flow_indicator(c: char) -> bool {
    // <https://yaml.org/spec/1.2.2/#rule-c-flow-indicator>
    matches!(c, '[' | ']' | '{' | '}' | ',')
}

fn should_escape(c: char) -> bool {
    // This isn't defined by the YAML spec, but is based on guidance in
    // <https://yaml.org/spec/1.2.2/#51-character-set>, plus a extra few
    // characters (\t, \r, \n, \x7F, \x85 and \uFEFF).
    // Surrogates are not represented because Rust's char type is defined to not
    // hold them.
    matches!(c, '\x00'..='\x1F' | '\x7F' | '\u{0080}'..='\u{009F}' | '\u{FEFF}' | '\u{FFFE}' | '\u{FFFF}')
}

/// This disallows multi-line unquoted strings, which YAML does allow in some
/// contexts, but there's no expectation of such strings coming out of libloot.
/// It also disallows strings containing tabs (\t), DEL (\x7F) and NEL (\x85),
/// which YAML does allow.
fn can_emit_unquoted(value: &str, style: YamlStyle) -> bool {
    // <https://yaml.org/spec/1.2.2/#733-plain-style>
    if value.is_empty()
        || value.starts_with(is_yaml_whitespace)
        || value.ends_with(is_yaml_whitespace)
    {
        return false;
    }

    if value.starts_with(|c| {
        matches!(
            c,
            ',' | '['
                | ']'
                | '{'
                | '}'
                | '#'
                | '&'
                | '*'
                | '!'
                | '|'
                | '>'
                | '\''
                | '"'
                | '%'
                | '@'
                | '`'
        )
    }) {
        return false;
    }

    if value.starts_with("? ")
        || value.starts_with("?\t")
        || value.starts_with("- ")
        || value.starts_with("-\t")
    {
        return false;
    }

    if value.contains(": ")
        || value.contains(":\t")
        || value.contains(" #")
        || value.contains("\t#")
    {
        return false;
    }

    if style == YamlStyle::Flow && value.contains(is_flow_indicator) {
        return false;
    }

    !value.chars().any(should_escape)
}

/// This disallows line breaks, which are allowed by YAML, but they can't be
/// escaped in single-quoted strings and the rules about emitting multi-line
/// YAML strings are relatively complicated so just avoid having to deal with
/// them. A couple of other characters are disallowed that YAML allows (e.g.
/// tab and the BOM).
fn can_single_quote(value: &str) -> bool {
    // Single-quoted strings are restricted to printable characters
    // <https://yaml.org/spec/1.2.2/#732-single-quoted-style>
    !value.chars().any(should_escape)
}

fn single_quote(value: &str) -> String {
    // Single-quoted strings need single quotes escaped by repeating them.
    // <https://yaml.org/spec/1.2.2/#732-single-quoted-style>
    format!("'{}'", value.replace('\'', "''"))
}

fn double_quote(value: &str) -> String {
    // <https://yaml.org/spec/1.2.2/#731-double-quoted-style>
    let escaped: String = value
        .chars()
        .map(|c| match c {
            '\x00' => "\\0".to_owned(),
            '\x07' => "\\a".to_owned(),
            '\x08' => "\\b".to_owned(),
            '\x09' => "\\t".to_owned(),
            '\x0A' => "\\n".to_owned(),
            '\x0B' => "\\v".to_owned(),
            '\x0C' => "\\f".to_owned(),
            '\x0D' => "\\r".to_owned(),
            '\x1B' => "\\e".to_owned(),
            // Don't escape spaces, it's only necessary to force them on
            // the start or end of lines of strings that are written
            // over multiple lines of YAML, but we're escaping the line breaks.
            // '\x20' => "\\x20".to_owned(),
            '"' => "\\\"".to_owned(),
            // Escaping forward slashes is supported by YAML but unnecessary and
            // noisy.
            // '/' => "\\/".to_owned(),
            '\\' => "\\\\".to_owned(),
            '\u{0085}' => "\\N".to_owned(),
            '\u{00A0}' => "\\_".to_owned(),
            '\u{2028}' => "\\L".to_owned(),
            '\u{2029}' => "\\P".to_owned(),
            '\u{00}'..='\u{FF}' if should_escape(c) => format!("\\x{:02X}", u32::from(c)),
            '\u{0100}'..='\u{FFFF}' if should_escape(c) => format!("\\u{:04X}", u32::from(c)),
            c if should_escape(c) => format!("\\U{:08X}", u32::from(c)),
            _ => c.to_string(),
        })
        .collect();

    format!("\"{escaped}\"")
}

impl<T: EmitYaml> EmitYaml for &[T] {
    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        match self {
            [] => {}
            [element] if element.is_scalar() || element.has_written_anchor(&emitter.anchors) => {
                emitter.set_style(YamlStyle::Flow);
                emitter.begin_array();
                emitter.write(" ");
                element.emit_yaml(emitter);
                emitter.write(" ");
                emitter.end_array();
                emitter.set_style(YamlStyle::Block);
            }
            elements => {
                emitter.begin_array();

                for element in *elements {
                    element.emit_yaml(emitter);
                }

                emitter.end_array();
            }
        }
    }
}

impl<T: EmitYaml> EmitYaml for Vec<T> {
    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        self.as_slice().emit_yaml(emitter);
    }
}

impl<T: EmitYaml> EmitYaml for Box<[T]> {
    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        self.as_ref().emit_yaml(emitter);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    mod yaml_emitter {
        use super::*;

        mod write_unquoted_str {
            use super::*;

            fn emit(str: &str) -> String {
                let mut emitter = YamlEmitter::new(YamlAnchors::new());
                emitter.write_unquoted_str(str);
                emitter.into_string()
            }

            #[test]
            fn should_emit_string_as_given() {
                let value = "hello world";

                assert_eq!(value, emit(value));
            }

            #[test]
            fn should_fall_back_to_quoting_string_if_it_cannot_be_emitted_unquoted() {
                assert_eq!("''", emit(""));
                assert_eq!("' a'", emit(" a"));
                assert_eq!("'a '", emit("a "));
                assert_eq!("',a'", emit(",a"));
                assert_eq!("'[a'", emit("[a"));
                assert_eq!("']a'", emit("]a"));
                assert_eq!("'{a'", emit("{a"));
                assert_eq!("'}a'", emit("}a"));
                assert_eq!("'#a'", emit("#a"));
                assert_eq!("'&a'", emit("&a"));
                assert_eq!("'*a'", emit("*a"));
                assert_eq!("'!a'", emit("!a"));
                assert_eq!("'|a'", emit("|a"));
                assert_eq!("'>a'", emit(">a"));
                assert_eq!("'''a'", emit("'a"));
                assert_eq!("'\"a'", emit("\"a"));
                assert_eq!("'%a'", emit("%a"));
                assert_eq!("'@a'", emit("@a"));
                assert_eq!("'`a'", emit("`a"));
                assert_eq!("'? a'", emit("? a"));
                assert_eq!("\"?\\ta\"", emit("?\ta"));
                assert_eq!("'- a'", emit("- a"));
                assert_eq!("\"-\\ta\"", emit("-\ta"));
                assert_eq!("'a: b'", emit("a: b"));
                assert_eq!("\"a:\\tb\"", emit("a:\tb"));
                assert_eq!("'a #b'", emit("a #b"));
                assert_eq!("\"a\\t#b\"", emit("a\t#b"));
            }

            #[test]
            fn should_fall_back_to_single_quoting_string_that_contains_a_flow_indicator_when_style_is_flow()
             {
                fn emit_flow(str: &str) -> String {
                    let mut emitter = YamlEmitter::new(YamlAnchors::new());
                    emitter.set_style(YamlStyle::Flow);
                    emitter.write_unquoted_str(str);
                    emitter.into_string()
                }

                assert_eq!("a[b", emit("a[b"));
                assert_eq!("a]b", emit("a]b"));
                assert_eq!("a{b", emit("a{b"));
                assert_eq!("a}b", emit("a}b"));
                assert_eq!("a,b", emit("a,b"));

                assert_eq!("'a[b'", emit_flow("a[b"));
                assert_eq!("'a]b'", emit_flow("a]b"));
                assert_eq!("'a{b'", emit_flow("a{b"));
                assert_eq!("'a}b'", emit_flow("a}b"));
                assert_eq!("'a,b'", emit_flow("a,b"));
            }

            #[test]
            fn should_fall_back_to_double_quoting_string_if_it_cannot_be_unquoted_or_single_quoted()
            {
                assert_eq!(
                    "\"\\e[1mhello world\\e[0m\"",
                    emit("\x1B[1mhello world\x1B[0m")
                );
            }

            #[test]
            fn should_fall_back_to_double_quoting_if_string_contains_a_c0_control_character() {
                assert_eq!("\"\\0\"", emit("\x00"));
                assert_eq!("\"\\x01\"", emit("\x01"));
                assert_eq!("\"\\x02\"", emit("\x02"));
                assert_eq!("\"\\x03\"", emit("\x03"));
                assert_eq!("\"\\x04\"", emit("\x04"));
                assert_eq!("\"\\x05\"", emit("\x05"));
                assert_eq!("\"\\x06\"", emit("\x06"));
                assert_eq!("\"\\a\"", emit("\x07"));
                assert_eq!("\"\\b\"", emit("\x08"));
                assert_eq!("\"\\t\"", emit("\x09"));
                assert_eq!("\"\\n\"", emit("\x0A"));
                assert_eq!("\"\\v\"", emit("\x0B"));
                assert_eq!("\"\\f\"", emit("\x0C"));
                assert_eq!("\"\\r\"", emit("\x0D"));
                assert_eq!("\"\\x0E\"", emit("\x0E"));
                assert_eq!("\"\\x0F\"", emit("\x0F"));
                assert_eq!("\"\\x10\"", emit("\x10"));
                assert_eq!("\"\\x11\"", emit("\x11"));
                assert_eq!("\"\\x12\"", emit("\x12"));
                assert_eq!("\"\\x13\"", emit("\x13"));
                assert_eq!("\"\\x14\"", emit("\x14"));
                assert_eq!("\"\\x15\"", emit("\x15"));
                assert_eq!("\"\\x16\"", emit("\x16"));
                assert_eq!("\"\\x17\"", emit("\x17"));
                assert_eq!("\"\\x18\"", emit("\x18"));
                assert_eq!("\"\\x19\"", emit("\x19"));
                assert_eq!("\"\\x1A\"", emit("\x1A"));
                assert_eq!("\"\\e\"", emit("\x1B"));
                assert_eq!("\"\\x1C\"", emit("\x1C"));
                assert_eq!("\"\\x1D\"", emit("\x1D"));
                assert_eq!("\"\\x1E\"", emit("\x1E"));
                assert_eq!("\"\\x1F\"", emit("\x1F"));
            }

            #[test]
            fn should_fall_back_to_double_quoting_if_string_contains_a_del_character() {
                assert_eq!("\"\\x7F\"", emit("\x7F"));
            }

            #[test]
            fn should_fall_back_to_double_quoting_if_string_contains_a_c1_control_character() {
                assert_eq!("\"\\x80\"", emit("\u{0080}"));
                assert_eq!("\"\\x81\"", emit("\u{0081}"));
                assert_eq!("\"\\x82\"", emit("\u{0082}"));
                assert_eq!("\"\\x83\"", emit("\u{0083}"));
                assert_eq!("\"\\x84\"", emit("\u{0084}"));
                assert_eq!("\"\\N\"", emit("\u{0085}"));
                assert_eq!("\"\\x86\"", emit("\u{0086}"));
                assert_eq!("\"\\x87\"", emit("\u{0087}"));
                assert_eq!("\"\\x88\"", emit("\u{0088}"));
                assert_eq!("\"\\x89\"", emit("\u{0089}"));
                assert_eq!("\"\\x8A\"", emit("\u{008A}"));
                assert_eq!("\"\\x8B\"", emit("\u{008B}"));
                assert_eq!("\"\\x8C\"", emit("\u{008C}"));
                assert_eq!("\"\\x8D\"", emit("\u{008D}"));
                assert_eq!("\"\\x8E\"", emit("\u{008E}"));
                assert_eq!("\"\\x8F\"", emit("\u{008F}"));
                assert_eq!("\"\\x90\"", emit("\u{0090}"));
                assert_eq!("\"\\x91\"", emit("\u{0091}"));
                assert_eq!("\"\\x92\"", emit("\u{0092}"));
                assert_eq!("\"\\x93\"", emit("\u{0093}"));
                assert_eq!("\"\\x94\"", emit("\u{0094}"));
                assert_eq!("\"\\x95\"", emit("\u{0095}"));
                assert_eq!("\"\\x96\"", emit("\u{0096}"));
                assert_eq!("\"\\x97\"", emit("\u{0097}"));
                assert_eq!("\"\\x98\"", emit("\u{0098}"));
                assert_eq!("\"\\x99\"", emit("\u{0099}"));
                assert_eq!("\"\\x9A\"", emit("\u{009A}"));
                assert_eq!("\"\\x9B\"", emit("\u{009B}"));
                assert_eq!("\"\\x9C\"", emit("\u{009C}"));
                assert_eq!("\"\\x9D\"", emit("\u{009D}"));
                assert_eq!("\"\\x9E\"", emit("\u{009E}"));
                assert_eq!("\"\\x9F\"", emit("\u{009F}"));
            }

            #[test]
            fn should_fall_back_to_double_quoting_if_string_contains_a_bom() {
                assert_eq!("\"\\uFEFF\"", emit("\u{FEFF}"));
            }

            #[test]
            fn should_fall_back_to_double_quoting_if_string_contains_unicode_fffe_or_ffff() {
                assert_eq!("\"\\uFFFE\"", emit("\u{FFFE}"));
                assert_eq!("\"\\uFFFF\"", emit("\u{FFFF}"));
            }

            #[test]
            fn should_escape_double_quote_in_double_quoted_string() {
                assert_eq!("\"\\\"\\n\"", emit("\"\n"));
            }

            #[test]
            fn should_escape_backslash_in_double_quoted_string() {
                assert_eq!("\"\\\\\\n\"", emit("\\\n"));
            }

            #[test]
            fn should_escape_certain_unicode_whitespace_characters_in_double_quoted_string() {
                assert_eq!(r#""\N\_\L\P""#, emit("\u{85}\u{A0}\u{2028}\u{2029}"));
            }
        }

        mod write_single_quoted_str {
            use super::*;

            #[test]
            fn single_quoted_str_should_emit_string_wrapped_in_single_quotes_and_with_single_quotes_doubled()
             {
                let value = "hello 'world'";
                let mut emitter = YamlEmitter::new(YamlAnchors::new());
                emitter.write_single_quoted_str(value);

                assert_eq!("'hello ''world'''", emitter.into_string());
            }

            #[test]
            fn single_quoted_str_should_fall_back_to_double_quoting_string_if_it_contains_non_printable_characters()
             {
                let value = "\x1B[1mhello world\x1B[0m";
                let mut emitter = YamlEmitter::new(YamlAnchors::new());
                emitter.write_single_quoted_str(value);

                assert_eq!("\"\\e[1mhello world\\e[0m\"", emitter.into_string());
            }
        }
    }

    mod slice_emit_yaml {
        use super::*;

        use crate::metadata::{MessageType, emit, emit_with_anchors};

        #[test]
        fn should_emit_a_flow_style_message_list_if_it_has_only_one_element_that_can_use_an_alias()
        {
            let messages = vec![Message::new(MessageType::Say, "message".into())];

            let mut anchors = YamlAnchors::new();
            anchors.set_message_anchors(HashMap::from([(&messages[0], "message1".to_owned())]));
            anchors.record_written_anchor("message1".to_owned());

            let yaml = emit_with_anchors(&messages, anchors);

            assert_eq!("[ *message1 ]", yaml);
        }

        #[test]
        fn should_emit_a_block_style_message_list_if_it_has_multiple_elements() {
            let messages = vec![
                Message::new(MessageType::Say, "message 1".into()),
                Message::new(MessageType::Say, "message 2".into()),
            ];

            let yaml = emit(&messages);

            assert_eq!(
                format!(
                    "\n- type: say\n  content: '{}'\n- type: say\n  content: '{}'",
                    messages[0].content()[0].text(),
                    messages[1].content()[0].text()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_a_block_style_message_list_if_it_has_multiple_elements_and_one_alias() {
            let messages = vec![
                Message::new(MessageType::Say, "message 1".into()),
                Message::new(MessageType::Say, "message 2".into()),
            ];

            let mut anchors = YamlAnchors::new();
            anchors.set_message_anchors(HashMap::from([(&messages[0], "message1".to_owned())]));
            anchors.record_written_anchor("message1".to_owned());

            let yaml = emit_with_anchors(&messages, anchors);

            assert_eq!(
                format!(
                    "\n- *message1\n- type: say\n  content: '{}'",
                    messages[1].content()[0].text()
                ),
                yaml
            );
        }
    }
}
