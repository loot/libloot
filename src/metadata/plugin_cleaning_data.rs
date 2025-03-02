use saphyr::MarkedYaml;

use crate::error::{GeneralError, InvalidMultilingualMessageContents, YamlParseError};

use super::{
    message::{MessageContent, parse_message_contents_yaml, validate_message_contents},
    yaml::{YamlObjectType, as_string_node, get_as_hash, get_required_string_value, get_u32_value},
};

/// Represents data identifying the plugin under which it is stored as dirty or
/// clean.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct PluginCleaningData {
    crc: u32,
    itm_count: u32,
    deleted_reference_count: u32,
    deleted_navmesh_count: u32,
    cleaning_utility: String,
    detail: Vec<MessageContent>,
}

impl PluginCleaningData {
    /// Construct a [PluginCleaningData] object with the given CRC and cleaning
    /// utility, no detail and the ITM, deleted reference and deleted navmesh
    /// counts set to zero.
    #[must_use]
    pub fn new(crc: u32, cleaning_utility: String) -> Self {
        Self {
            crc,
            cleaning_utility,
            ..Default::default()
        }
    }

    /// Set the number of Identical To Master records found in the plugin.
    #[must_use]
    pub fn with_itm_count(mut self, itm_count: u32) -> Self {
        self.itm_count = itm_count;
        self
    }

    /// Set the number of deleted references found in the plugin.
    #[must_use]
    pub fn with_deleted_reference_count(mut self, deleted_reference_count: u32) -> Self {
        self.deleted_reference_count = deleted_reference_count;
        self
    }

    /// Set the number of deleted navmeshes found in the plugin.
    #[must_use]
    pub fn with_deleted_navmesh_count(mut self, deleted_navmesh_count: u32) -> Self {
        self.deleted_navmesh_count = deleted_navmesh_count;
        self
    }

    /// Set the detail message content, which may be appended to any messages
    /// generated for this cleaning data. If multilingual, one language must be
    /// [MessageContent::DEFAULT_LANGUAGE].
    pub fn with_detail(
        mut self,
        detail: Vec<MessageContent>,
    ) -> Result<Self, InvalidMultilingualMessageContents> {
        validate_message_contents(&detail)?;
        self.detail = detail;
        Ok(self)
    }

    /// Get the CRC that identifies the plugin that the cleaning data is for.
    pub fn crc(&self) -> u32 {
        self.crc
    }

    /// Get the number of Identical To Master records found in the plugin.
    pub fn itm_count(&self) -> u32 {
        self.itm_count
    }

    /// Get the number of deleted references found in the plugin.
    pub fn deleted_reference_count(&self) -> u32 {
        self.deleted_reference_count
    }

    /// Get the number of deleted navmeshes found in the plugin.
    pub fn deleted_navmesh_count(&self) -> u32 {
        self.deleted_navmesh_count
    }

    /// Get the cleaning utility that was used to check the plugin.
    ///
    /// The string may include a cleaning utility name, possibly related
    /// information such as a version number and/or a CommonMark-formatted URL
    /// to the utility's download location.
    pub fn cleaning_utility(&self) -> &str {
        &self.cleaning_utility
    }

    /// Get any additional informative message content supplied with the
    /// cleaning data, eg. a link to a cleaning guide or information on wild
    /// edits or manual cleaning steps.
    pub fn detail(&self) -> &[MessageContent] {
        &self.detail
    }
}

impl TryFrom<&MarkedYaml> for PluginCleaningData {
    type Error = GeneralError;

    fn try_from(value: &MarkedYaml) -> Result<Self, Self::Error> {
        let hash = get_as_hash(value, YamlObjectType::PluginCleaningData)?;

        let crc = match get_u32_value(hash, "crc", YamlObjectType::PluginCleaningData)? {
            Some(n) => n,
            None => {
                return Err(YamlParseError::missing_key(
                    value.span.start,
                    "crc",
                    YamlObjectType::PluginCleaningData,
                )
                .into());
            }
        };

        let util = get_required_string_value(
            value.span.start,
            hash,
            "util",
            YamlObjectType::PluginCleaningData,
        )?;

        let itm = get_u32_value(hash, "itm", YamlObjectType::PluginCleaningData)?.unwrap_or(0);
        let udr = get_u32_value(hash, "udr", YamlObjectType::PluginCleaningData)?.unwrap_or(0);
        let nav = get_u32_value(hash, "nav", YamlObjectType::PluginCleaningData)?.unwrap_or(0);

        let detail = match hash.get(&as_string_node("detail")) {
            Some(n) => {
                parse_message_contents_yaml(n, "detail", YamlObjectType::PluginCleaningData)?
            }
            None => Vec::new(),
        };

        Ok(PluginCleaningData {
            crc,
            itm_count: itm,
            deleted_reference_count: udr,
            deleted_navmesh_count: nav,
            cleaning_utility: util.to_string(),
            detail,
        })
    }
}
