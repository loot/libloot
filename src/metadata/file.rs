use saphyr::{MarkedYaml, YamlData};
use unicase::UniCase;

use super::{
    error::ExpectedType,
    error::{MultilingualMessageContentsError, ParseMetadataError},
    message::{MessageContent, parse_message_contents_yaml, validate_message_contents},
    yaml::{
        YamlObjectType, as_string_node, get_required_string_value, get_string_value,
        parse_condition,
    },
};

/// Represents a file in a game's Data folder, including files in
/// subdirectories.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct File {
    name: Filename,
    display_name: Option<String>,
    detail: Vec<MessageContent>,
    condition: Option<String>,
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
        self.display_name = Some(display_name);
        self
    }

    /// Set the condition string.
    #[must_use]
    pub fn with_condition(mut self, condition: String) -> Self {
        self.condition = Some(condition);
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
        self.detail = detail;
        Ok(self)
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
}

/// Represents a case-insensitive filename.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Filename(UniCase<String>);

impl Filename {
    /// Construct a Filename using the given string.
    #[must_use]
    pub fn new(s: String) -> Self {
        Filename(UniCase::new(s))
    }

    /// Get this Filename as a string.
    pub fn as_str(&self) -> &str {
        &self.0
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

impl TryFrom<&MarkedYaml> for File {
    type Error = ParseMetadataError;

    fn try_from(value: &MarkedYaml) -> Result<Self, Self::Error> {
        match &value.data {
            YamlData::String(s) => Ok(File {
                name: Filename(UniCase::new(s.to_string())),
                display_name: None,
                detail: Vec::new(),
                condition: None,
            }),
            YamlData::Hash(h) => {
                let name =
                    get_required_string_value(value.span.start, h, "name", YamlObjectType::File)?;

                let display_name = get_string_value(h, "display", YamlObjectType::File)?;

                let detail = match h.get(&as_string_node("detail")) {
                    Some(n) => parse_message_contents_yaml(
                        n,
                        "detail",
                        YamlObjectType::PluginCleaningData,
                    )?,
                    None => Vec::new(),
                };

                let condition = parse_condition(h, YamlObjectType::File)?;

                Ok(File {
                    name: Filename(UniCase::new(name.to_string())),
                    display_name: display_name.map(|(_, s)| s.to_string()),
                    detail,
                    condition,
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
