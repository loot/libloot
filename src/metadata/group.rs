use saphyr::MarkedYaml;

use super::yaml::{
    YamlObjectType, get_as_hash, get_required_string_value, get_string_value, get_strings_vec_value,
};
use crate::error::YamlParseError;

/// Represents a group to which plugin metadata objects can belong.
#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Group {
    name: String,
    description: Option<String>,
    after_groups: Vec<String>,
}

impl Group {
    /// Construct a [Group] with the given name.
    #[must_use]
    pub fn new(name: String) -> Self {
        Self {
            name,
            ..Default::default()
        }
    }

    /// Set a description for the group.
    #[must_use]
    pub fn with_description(mut self, description: String) -> Self {
        self.description = Some(description);
        self
    }

    /// Set the names of the groups that this group loads after.
    #[must_use]
    pub fn with_after_groups(mut self, after_groups: Vec<String>) -> Self {
        self.after_groups = after_groups;
        self
    }

    /// The name of the group to which all plugins belong by default.
    pub const DEFAULT_NAME: &'static str = "default";

    /// Get the name of the group.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get the description of the group.
    pub fn description(&self) -> Option<&str> {
        self.description.as_deref()
    }

    /// Get the names of the groups that this group loads after.
    pub fn after_groups(&self) -> &[String] {
        &self.after_groups
    }
}

impl std::default::Default for Group {
    /// Construct a Group with the default name and an empty set of groups to
    /// load after.
    #[must_use]
    fn default() -> Self {
        Self {
            name: Group::DEFAULT_NAME.to_string(),
            description: Default::default(),
            after_groups: Default::default(),
        }
    }
}

impl TryFrom<&MarkedYaml> for Group {
    type Error = YamlParseError;

    fn try_from(value: &MarkedYaml) -> Result<Self, Self::Error> {
        let hash = get_as_hash(value, YamlObjectType::Group)?;

        let name =
            get_required_string_value(value.span.start, hash, "name", YamlObjectType::Group)?;

        let description = get_string_value(hash, "description", YamlObjectType::Group)?;

        let after = get_strings_vec_value(hash, "after", YamlObjectType::Group)?;

        Ok(Group {
            name: name.to_string(),
            description: description.map(|d| d.to_string()),
            after_groups: after.iter().map(|a| a.to_string()).collect(),
        })
    }
}
