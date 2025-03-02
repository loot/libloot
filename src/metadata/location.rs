use saphyr::{MarkedYaml, YamlData};

use crate::error::{GeneralError, YamlParseError};

use super::yaml::{YamlObjectType, get_required_string_value};

/// Represents a URL at which the parent plugin can be found.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Location {
    url: String,
    name: Option<String>,
}

impl Location {
    /// Construct a [Location] with the given URL.
    #[must_use]
    pub fn new(url: String) -> Self {
        Location {
            url,
            ..Default::default()
        }
    }

    /// Set a name for the URL, eg. the page or site name.
    #[must_use]
    pub fn with_name(mut self, name: String) -> Self {
        self.name = Some(name);
        self
    }

    /// Get the URL.
    pub fn url(&self) -> &str {
        &self.url
    }

    /// Get the descriptive name of this location.
    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }
}

impl TryFrom<&MarkedYaml> for Location {
    type Error = GeneralError;

    fn try_from(value: &MarkedYaml) -> Result<Self, Self::Error> {
        match &value.data {
            YamlData::String(s) => Ok(Location {
                url: s.clone(),
                name: None,
            }),
            YamlData::Hash(h) => {
                let link = get_required_string_value(
                    value.span.start,
                    h,
                    "link",
                    YamlObjectType::Location,
                )?;
                let name = get_required_string_value(
                    value.span.start,
                    h,
                    "name",
                    YamlObjectType::Location,
                )?;

                Ok(Location {
                    url: link.to_string(),
                    name: Some(name.to_string()),
                })
            }
            _ => Err(YamlParseError::new(
                value.span.start,
                "'tag' object must be a map or string".into(),
            )
            .into()),
        }
    }
}
