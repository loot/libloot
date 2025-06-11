pub trait EmitYaml {
    fn is_scalar(&self) -> bool {
        false
    }

    fn emit_yaml(&self, emitter: &mut YamlEmitter);
}

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct YamlEmitter {
    buffer: String,
    scope: Vec<YamlBlock>,
    style: YamlStyle,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
enum YamlBlock {
    Array,
    Map,
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

impl YamlEmitter {
    const INDENT_UNIT: &str = "  ";
    const ARRAY_ELEMENT_PREFIX: &str = "- ";

    pub fn new() -> Self {
        Self {
            buffer: String::new(),
            scope: vec![],
            style: YamlStyle::Block,
        }
    }

    pub fn into_string(self) -> String {
        self.buffer
    }

    pub fn unquoted_str(&mut self, value: &str) {
        if self.style == YamlStyle::Block {
            self.write_prefix();
        }

        if can_emit_unquoted(value, self.style) {
            self.write(value);
        } else if can_single_quote(value) {
            self.write(&single_quote(value));
        } else {
            self.write(&double_quote(value));
        }
    }

    pub fn single_quoted_str(&mut self, value: &str) {
        if self.style == YamlStyle::Block {
            self.write_prefix();
        }

        if can_single_quote(value) {
            self.write(&single_quote(value));
        } else {
            self.write(&double_quote(value));
        }
    }

    pub fn u32(&mut self, value: u32) {
        if self.style == YamlStyle::Block {
            self.write_prefix();
        }

        self.write(&value.to_string());
    }

    pub fn begin_map(&mut self) {
        if self.scope.last() == Some(&YamlBlock::Array) {
            self.end_line();
            self.write_indent();
            self.write(Self::ARRAY_ELEMENT_PREFIX);
        }
    }

    pub fn end_map(&mut self) {
        if self.scope.last() == Some(&YamlBlock::Map) {
            self.scope.pop();
        }
    }

    /// This assumes that the given key is valid to be written as an unquoted
    /// string, and expects a string literal so that it's obvious that a given
    /// value is valid.
    pub fn map_key(&mut self, key: &'static str) {
        match self.scope.last() {
            Some(&YamlBlock::Map) => {
                self.end_line();
                self.write_indent();
            }
            _ => self.scope.push(YamlBlock::Map),
        }

        self.write(&format!("{key}:"));
    }

    pub fn begin_array(&mut self) {
        if self.style == YamlStyle::Flow {
            if self.scope.last() == Some(&YamlBlock::Map) {
                self.write(" ");
            }
            self.write("[");
        }

        self.scope.push(YamlBlock::Array);
    }

    pub fn end_array(&mut self) {
        if self.scope.last() == Some(&YamlBlock::Array) {
            self.scope.pop();
        }

        if self.style == YamlStyle::Flow {
            self.write("]");
        }
    }

    pub fn set_flow_style(&mut self) {
        self.style = YamlStyle::Flow;
    }

    pub fn set_block_style(&mut self) {
        self.style = YamlStyle::Block;
    }

    fn end_line(&mut self) {
        self.write("\n");
    }

    fn write_indent(&mut self) {
        // If in a map, no indent is needed, but an array needs an indent, and a
        // map in an array needs an indent.
        if !self.scope.is_empty() {
            for _ in 0..self.scope.len() - 1 {
                self.write(Self::INDENT_UNIT);
            }
        }
    }

    fn write_prefix(&mut self) {
        match self.scope.last() {
            Some(&YamlBlock::Array) => {
                self.end_line();
                self.write_indent();
                self.write(Self::ARRAY_ELEMENT_PREFIX);
            }
            Some(&YamlBlock::Map) => self.write(" "),
            _ => self.write_indent(),
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
        .map(|c| {
            if should_escape(c) {
                match c {
                    '\x00' => "\\0".to_owned(),
                    '\x07' => "\\a".to_owned(),
                    '\x08' => "\\b".to_owned(),
                    '\x09' => "\\t".to_owned(),
                    '\x0A' => "\\n".to_owned(),
                    '\x0B' => "\\v".to_owned(),
                    '\x0C' => "\\f".to_owned(),
                    '\x0D' => "\\r".to_owned(),
                    '\x1B' => "\\e".to_owned(),
                    '\x20' => "\\x20".to_owned(),
                    '"' => "\\\"".to_owned(),
                    '/' => "\\/".to_owned(),
                    '\\' => "\\\\".to_owned(),
                    '\u{0085}' => "\\N".to_owned(),
                    '\u{00A0}' => "\\_".to_owned(),
                    '\u{2028}' => "\\L".to_owned(),
                    '\u{2029}' => "\\P".to_owned(),
                    '\u{00}'..='\u{FF}' => format!("\\x{:02X}", u32::from(c)),
                    '\u{0100}'..='\u{FFFF}' => format!("\\u{:04X}", u32::from(c)),
                    c => format!("\\U{:08X}", u32::from(c)),
                }
            } else {
                c.to_string()
            }
        })
        .collect();

    format!("\"{escaped}\"")
}

impl<T: EmitYaml> EmitYaml for &[T] {
    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        match self {
            [] => {}
            [element] if element.is_scalar() => {
                emitter.set_flow_style();
                emitter.begin_array();
                element.emit_yaml(emitter);
                emitter.end_array();
                emitter.set_block_style();
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

        mod unquoted_str {
            use super::*;

            fn emit(str: &str) -> String {
                let mut emitter = YamlEmitter::new();
                emitter.unquoted_str(str);
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
                    let mut emitter = YamlEmitter::new();
                    emitter.set_flow_style();
                    emitter.unquoted_str(str);
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
        }

        mod single_quoted_str {
            use super::*;

            #[test]
            fn single_quoted_str_should_emit_string_wrapped_in_single_quotes_and_with_single_quotes_doubled()
             {
                let value = "hello 'world'";
                let mut emitter = YamlEmitter::new();
                emitter.single_quoted_str(value);

                assert_eq!("'hello ''world'''", emitter.into_string());
            }

            #[test]
            fn single_quoted_str_should_fall_back_to_double_quoting_string_if_it_contains_non_printable_characters()
             {
                let value = "\x1B[1mhello world\x1B[0m";
                let mut emitter = YamlEmitter::new();
                emitter.single_quoted_str(value);

                assert_eq!("\"\\e[1mhello world\\e[0m\"", emitter.into_string());
            }
        }
    }
}
