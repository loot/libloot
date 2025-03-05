use saphyr::MarkedYaml;

use super::error::ParseMetadataError;
use super::yaml::{
    YamlObjectType, get_as_hash, get_required_string_value, get_string_value, get_strings_vec_value,
};
use super::yaml_emit::{EmitYaml, YamlEmitter};

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
    type Error = ParseMetadataError;

    fn try_from(value: &MarkedYaml) -> Result<Self, Self::Error> {
        let hash = get_as_hash(value, YamlObjectType::Group)?;

        let name =
            get_required_string_value(value.span.start, hash, "name", YamlObjectType::Group)?;

        let description = get_string_value(hash, "description", YamlObjectType::Group)?;

        let after = get_strings_vec_value(hash, "after", YamlObjectType::Group)?;

        Ok(Group {
            name: name.to_string(),
            description: description.map(|d| d.1.to_string()),
            after_groups: after.iter().map(|a| a.to_string()).collect(),
        })
    }
}

impl EmitYaml for Group {
    fn is_scalar(&self) -> bool {
        false
    }

    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        emitter.begin_map();

        emitter.map_key("name");
        emitter.single_quoted_str(&self.name);

        if let Some(description) = &self.description {
            emitter.map_key("description");
            emitter.single_quoted_str(description);
        }

        if !self.after_groups.is_empty() {
            emitter.map_key("after");
            emitter.begin_array();

            for after in &self.after_groups {
                emitter.unquoted_str(after);
            }

            emitter.end_array();
        }

        emitter.end_map();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    mod emit_yaml {
        use super::*;
        use crate::metadata::emit;

        #[test]
        fn should_omit_description_and_after_keys_if_their_fields_are_empty() {
            let group = Group::new("name".into());
            let yaml = emit(&group);

            assert_eq!(format!("name: '{}'", group.name), yaml);
        }

        #[test]
        fn should_include_description_key_if_a_description_is_set() {
            let group = Group::new("name".into()).with_description("desc".into());
            let yaml = emit(&group);

            assert_eq!(
                format!(
                    "name: '{}'\ndescription: '{}'",
                    group.name,
                    group.description.unwrap()
                ),
                yaml
            );
        }

        #[test]
        fn should_include_after_key_if_after_groups_is_not_empty() {
            let group =
                Group::new("name".into()).with_after_groups(vec!["after1".into(), "after2".into()]);

            let yaml = emit(&group);

            assert_eq!(
                format!(
                    "name: '{}'\nafter:\n  - {}\n  - {}",
                    group.name, group.after_groups[0], group.after_groups[1]
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_all_fields_set() {
            let group = Group::new("name".into())
                .with_description("desc".into())
                .with_after_groups(vec!["after1".into(), "after2".into()]);

            let yaml = emit(&group);

            assert_eq!(
                format!(
                    "name: '{}'\ndescription: '{}'\nafter:\n  - {}\n  - {}",
                    group.name,
                    group.description.unwrap(),
                    group.after_groups[0],
                    group.after_groups[1]
                ),
                yaml
            );
        }
    }
}
