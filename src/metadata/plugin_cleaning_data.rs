use saphyr::MarkedYaml;

use super::{
    error::{MultilingualMessageContentsError, ParseMetadataError},
    message::{
        MessageContent, emit_message_contents, parse_message_contents_yaml,
        validate_message_contents,
    },
    yaml::{YamlObjectType, as_string_node, get_as_hash, get_required_string_value, get_u32_value},
    yaml_emit::{EmitYaml, YamlEmitter},
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
    ) -> Result<Self, MultilingualMessageContentsError> {
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
    type Error = ParseMetadataError;

    fn try_from(value: &MarkedYaml) -> Result<Self, Self::Error> {
        let hash = get_as_hash(value, YamlObjectType::PluginCleaningData)?;

        let crc = match get_u32_value(hash, "crc", YamlObjectType::PluginCleaningData)? {
            Some(n) => n,
            None => {
                return Err(ParseMetadataError::missing_key(
                    value.span.start,
                    "crc",
                    YamlObjectType::PluginCleaningData,
                ));
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

impl EmitYaml for PluginCleaningData {
    fn is_scalar(&self) -> bool {
        false
    }

    fn emit_yaml(&self, emitter: &mut YamlEmitter) {
        emitter.begin_map();

        emitter.map_key("crc");
        emitter.unquoted_str(&format!("0x{:08X}", self.crc));

        emitter.map_key("util");
        emitter.single_quoted_str(&self.cleaning_utility);

        if self.itm_count > 0 {
            emitter.map_key("itm");
            emitter.u32(self.itm_count);
        }

        if self.deleted_reference_count > 0 {
            emitter.map_key("udr");
            emitter.u32(self.deleted_reference_count);
        }

        if self.deleted_navmesh_count > 0 {
            emitter.map_key("nav");
            emitter.u32(self.deleted_navmesh_count);
        }

        emit_message_contents(&self.detail, emitter, "detail");

        emitter.end_map();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    mod try_from_yaml {
        use crate::metadata::parse;

        use super::*;

        #[test]
        fn should_error_if_given_a_scalar() {
            let yaml = parse("0x12345678");

            assert!(PluginCleaningData::try_from(&yaml).is_err());
        }

        #[test]
        fn should_error_if_given_a_list() {
            let yaml = parse("[0, 1, 2]");

            assert!(PluginCleaningData::try_from(&yaml).is_err());
        }

        #[test]
        fn should_error_if_crc_is_missing() {
            let yaml = parse("{util: cleaner}");

            assert!(PluginCleaningData::try_from(&yaml).is_err());
        }

        #[test]
        fn should_error_util_is_missing() {
            let yaml = parse("{crc: 0x12345678}");

            assert!(PluginCleaningData::try_from(&yaml).is_err());
        }

        #[test]
        fn should_set_all_given_fields() {
            let yaml =
                parse("{crc: 0x12345678, util: cleaner, detail: info, itm: 2, udr: 10, nav: 30}");

            let data = PluginCleaningData::try_from(&yaml).unwrap();

            assert_eq!(0x12345678, data.crc());
            assert_eq!("cleaner", data.cleaning_utility());
            assert_eq!(&[MessageContent::new("info".into())], data.detail());
            assert_eq!(2, data.itm_count());
            assert_eq!(10, data.deleted_reference_count());
            assert_eq!(30, data.deleted_navmesh_count());
        }

        #[test]
        fn should_leave_optional_fields_at_defaults_if_not_present() {
            let yaml = parse("{crc: 0x12345678, util: cleaner}");

            let data = PluginCleaningData::try_from(&yaml).unwrap();

            assert_eq!(0x12345678, data.crc());
            assert_eq!("cleaner", data.cleaning_utility());
            assert!(data.detail().is_empty());
            assert_eq!(0, data.itm_count());
            assert_eq!(0, data.deleted_reference_count());
            assert_eq!(0, data.deleted_navmesh_count());
        }

        #[test]
        fn should_read_all_listed_detail_message_contents() {
            let yaml = parse(
                "{crc: 0x12345678, util: cleaner, detail: [{text: english, lang: en}, {text: french, lang: fr}]}",
            );

            let data = PluginCleaningData::try_from(&yaml).unwrap();

            assert_eq!(
                &[
                    MessageContent::new("english".into()),
                    MessageContent::new("french".into()).with_language("fr".into())
                ],
                data.detail()
            );
        }

        #[test]
        fn should_not_error_if_one_detail_is_given_and_it_is_not_english() {
            let yaml =
                parse("crc: 0x12345678\nutil: cleaner\ndetail:\n  - lang: fr\n    text: content1");

            let data = PluginCleaningData::try_from(&yaml).unwrap();

            assert_eq!(
                &[MessageContent::new("content1".into()).with_language("fr".into())],
                data.detail()
            );
        }

        #[test]
        fn should_error_if_multiple_details_are_given_and_none_are_english() {
            let yaml = parse(
                "crc: 0x12345678\nutil: cleaner\ndetail:\n  - lang: de\n    text: content1\n  - lang: fr\n    text: content2",
            );

            assert!(PluginCleaningData::try_from(&yaml).is_err());
        }
    }

    mod emit_yaml {
        use crate::metadata::emit;

        use super::*;

        #[test]
        fn should_omit_zero_counts() {
            let data = PluginCleaningData::new(0xDEADBEEF, "TES5Edit".into());
            let yaml = emit(&data);

            assert_eq!("crc: 0xDEADBEEF\nutil: 'TES5Edit'", yaml);
        }

        #[test]
        fn should_emit_non_zero_counts() {
            let data = PluginCleaningData::new(0xDEADBEEF, "TES5Edit".into())
                .with_itm_count(1)
                .with_deleted_reference_count(2)
                .with_deleted_navmesh_count(3);
            let yaml = emit(&data);

            assert_eq!(
                "crc: 0xDEADBEEF\nutil: 'TES5Edit'\nitm: 1\nudr: 2\nnav: 3",
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_a_detail_string_if_detail_is_monolingual() {
            let data = PluginCleaningData::new(0xDEADBEEF, "TES5Edit".into())
                .with_detail(vec![MessageContent::new("message".into())])
                .unwrap();
            let yaml = emit(&data);

            assert_eq!(
                format!(
                    "crc: 0xDEADBEEF\nutil: 'TES5Edit'\ndetail: '{}'",
                    data.detail[0].text()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_a_detail_array_if_detail_is_multilingual() {
            let data = PluginCleaningData::new(0xDEADBEEF, "TES5Edit".into())
                .with_detail(vec![
                    MessageContent::new("english".into()).with_language("en".into()),
                    MessageContent::new("french".into()).with_language("fr".into()),
                ])
                .unwrap();
            let yaml = emit(&data);

            assert_eq!(
                format!(
                    "crc: 0xDEADBEEF
util: 'TES5Edit'
detail:
  - lang: {}
    text: '{}'
  - lang: {}
    text: '{}'",
                    data.detail[0].language(),
                    data.detail[0].text(),
                    data.detail[1].language(),
                    data.detail[1].text()
                ),
                yaml
            );
        }

        #[test]
        fn should_emit_map_with_all_fields_set() {
            let data = PluginCleaningData::new(0xDEADBEEF, "TES5Edit".into())
                .with_itm_count(1)
                .with_deleted_reference_count(2)
                .with_deleted_navmesh_count(3)
                .with_detail(vec![
                    MessageContent::new("english".into()).with_language("en".into()),
                    MessageContent::new("french".into()).with_language("fr".into()),
                ])
                .unwrap();
            let yaml = emit(&data);

            assert_eq!(
                format!(
                    "crc: 0xDEADBEEF
util: '{}'
itm: {}
udr: {}
nav: {}
detail:
  - lang: {}
    text: '{}'
  - lang: {}
    text: '{}'",
                    data.cleaning_utility,
                    data.itm_count,
                    data.deleted_reference_count,
                    data.deleted_navmesh_count,
                    data.detail[0].language(),
                    data.detail[0].text(),
                    data.detail[1].language(),
                    data.detail[1].text()
                ),
                yaml
            );
        }
    }
}
