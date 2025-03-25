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
    Flow,
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

    /// This assumes that the given key is valid to be written as an unquoted string, and expects a string literal so that it's obvious that a given value is valid.
    pub fn map_key(&mut self, key: &'static str) {
        match self.scope.last() {
            Some(&YamlBlock::Map) => {
                self.end_line();
                self.write_indent();
            }
            _ => self.scope.push(YamlBlock::Map),
        }

        self.write(&format!("{}:", key));
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
        // If in a map, no indent is needed, but an array needs an indent, and a map in an array needs an indent.
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

fn can_single_quote(value: &str) -> bool {
    // Single-quoted strings are restricted to printable characters
    // <https://yaml.org/spec/1.2.2/#732-single-quoted-style>
    value.chars().all(is_printable)
}

fn is_printable(c: char) -> bool {
    // <https://yaml.org/spec/1.2.2/#51-character-set>
    matches!(c,
    '\x09' | '\x0A' | '\x0D' | '\x20'..='\x7E' | '\u{0085}' | '\u{00A0}'..='\u{D7FF}' | '\u{E000}'..='\u{FFFD}' | '\u{010000}'..='\u{10FFFF}'
    )
}

fn is_yaml_whitespace(c: char) -> bool {
    c == ' ' || c == '\t'
}

fn is_flow_indicator(c: char) -> bool {
    matches!(c, '[' | ']' | '{' | '}' | ',')
}

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

    if style == YamlStyle::Flow {
        !value.contains(is_flow_indicator)
    } else {
        true
    }
}

fn single_quote(value: &str) -> String {
    // Single-quoted strings need single quotes escaped by repeating them.
    // <https://yaml.org/spec/1.2.2/#732-single-quoted-style>
    format!("'{}'", value.replace('\'', "''"))
}

fn double_quote(value: &str) -> String {
    // <https://yaml.org/spec/1.2.2/#731-double-quoted-style>
    value
        .chars()
        .map(|c| {
            if is_printable(c) {
                c.to_string()
            } else {
                match c {
                    '\x00' => "\\0".to_string(),
                    '\x07' => "\\a".to_string(),
                    '\x08' => "\\b".to_string(),
                    '\x09' => "\\t".to_string(),
                    '\x0A' => "\\n".to_string(),
                    '\x0B' => "\\v".to_string(),
                    '\x0C' => "\\f".to_string(),
                    '\x0D' => "\\r".to_string(),
                    '\x1B' => "\\e".to_string(),
                    '\x20' => "\\x20".to_string(),
                    '"' => "\\\"".to_string(),
                    '/' => "\\/".to_string(),
                    '\\' => "\\\\".to_string(),
                    '\u{0085}' => "\\N".to_string(),
                    '\u{00A0}' => "\\_".to_string(),
                    '\u{2028}' => "\\L".to_string(),
                    '\u{2029}' => "\\P".to_string(),
                    '\u{00}'..='\u{FF}' => format!("\\x{:2X}", u32::from(c)),
                    '\u{0100}'..='\u{FFFF}' => format!("\\u{:4X}", u32::from(c)),
                    c => format!("\\U{:8X}", u32::from(c)),
                }
            }
        })
        .collect()
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

                for element in elements.iter() {
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
