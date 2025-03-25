use std::str::FromStr;

use loot_condition_interpreter::Expression;
use saphyr::{AnnotatedHash, MarkedYaml, Marker, Yaml, YamlData};

use super::super::error::{ExpectedType, MetadataParsingErrorReason, ParseMetadataError};

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum YamlObjectType {
    File,
    Group,
    Location,
    Message,
    MessageContent,
    PluginCleaningData,
    PluginMetadata,
    Tag,
    MetadataDocument,
    BashTagsElement,
}

impl std::fmt::Display for YamlObjectType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            YamlObjectType::File => write!(f, "file"),
            YamlObjectType::Group => write!(f, "group"),
            YamlObjectType::Location => write!(f, "location"),
            YamlObjectType::Message => write!(f, "message"),
            YamlObjectType::MessageContent => write!(f, "message content"),
            YamlObjectType::PluginCleaningData => write!(f, "plugin cleaning data"),
            YamlObjectType::PluginMetadata => write!(f, "plugin metadata"),
            YamlObjectType::Tag => write!(f, "tag"),
            YamlObjectType::MetadataDocument => write!(f, "metadata document"),
            YamlObjectType::BashTagsElement => write!(f, "bash tags"),
        }
    }
}

pub fn to_yaml(yaml: &MarkedYaml) -> Yaml {
    match &yaml.data {
        saphyr::YamlData::Real(v) => Yaml::Real(v.clone()),
        saphyr::YamlData::Integer(v) => Yaml::Integer(*v),
        saphyr::YamlData::String(v) => Yaml::String(v.clone()),
        saphyr::YamlData::Boolean(v) => Yaml::Boolean(*v),
        saphyr::YamlData::Array(v) => Yaml::Array(v.iter().map(to_yaml).collect()),
        saphyr::YamlData::Hash(v) => Yaml::Hash(
            v.iter()
                .map(|(key, value)| (to_yaml(key), to_yaml(value)))
                .collect(),
        ),
        saphyr::YamlData::Alias(v) => Yaml::Alias(*v),
        saphyr::YamlData::Null => Yaml::Null,
        saphyr::YamlData::BadValue => Yaml::BadValue,
    }
}

pub fn as_string_node(value: &str) -> MarkedYaml {
    MarkedYaml {
        span: saphyr_parser::Span::default(),
        data: YamlData::String(value.into()),
    }
}

pub fn get_string_value<'a>(
    hash: &'a AnnotatedHash<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<Option<(Marker, &'a str)>, ParseMetadataError> {
    match hash.get(&as_string_node(key)) {
        Some(n) => match n.data.as_str() {
            Some(s) => Ok(Some((n.span.start, s))),
            None => Err(ParseMetadataError::unexpected_value_type(
                n.span.start,
                key,
                yaml_type,
                ExpectedType::String,
            )),
        },
        None => Ok(None),
    }
}

pub fn get_required_string_value<'a>(
    marker: Marker,
    hash: &'a AnnotatedHash<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<&'a str, ParseMetadataError> {
    match get_string_value(hash, key, yaml_type)? {
        Some(n) => Ok(n.1),
        None => Err(ParseMetadataError::missing_key(marker, key, yaml_type)),
    }
}

pub fn get_strings_vec_value<'a>(
    hash: &'a AnnotatedHash<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<Vec<&'a str>, ParseMetadataError> {
    match hash.get(&as_string_node(key)) {
        Some(n) => match n.data.as_vec() {
            Some(n) => n
                .iter()
                .map(|e| match e.data.as_str() {
                    Some(s) => Ok(s),
                    None => Err(ParseMetadataError::unexpected_value_type(
                        e.span.start,
                        key,
                        yaml_type,
                        ExpectedType::String,
                    )),
                })
                .collect::<Result<Vec<_>, _>>(),
            None => Err(ParseMetadataError::unexpected_value_type(
                n.span.start,
                key,
                yaml_type,
                ExpectedType::Array,
            )),
        },
        None => Ok(Vec::new()),
    }
}

pub fn get_as_hash(
    value: &MarkedYaml,
    yaml_type: YamlObjectType,
) -> Result<&AnnotatedHash<MarkedYaml>, ParseMetadataError> {
    match value.data.as_hash() {
        Some(h) => Ok(h),
        None => Err(ParseMetadataError::unexpected_type(
            value.span.start,
            yaml_type,
            ExpectedType::Map,
        )),
    }
}

pub fn get_u32_value(
    hash: &AnnotatedHash<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<Option<u32>, ParseMetadataError> {
    match hash.get(&as_string_node(key)) {
        Some(n) => match n.data.as_i64() {
            Some(i) => i.try_into().map(Some).map_err(|_| {
                ParseMetadataError::new(n.span.start, MetadataParsingErrorReason::NonU32Number(i))
            }),
            None => Err(ParseMetadataError::unexpected_value_type(
                n.span.start,
                key,
                yaml_type,
                ExpectedType::Number,
            )),
        },
        None => Ok(None),
    }
}

pub fn get_as_slice<'a>(
    hash: &'a saphyr::AnnotatedHash<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<&'a [MarkedYaml], ParseMetadataError> {
    if let Some(value) = hash.get(&as_string_node(key)) {
        match value.data.as_vec() {
            Some(n) => Ok(n.as_slice()),
            None => Err(ParseMetadataError::unexpected_value_type(
                value.span.start,
                key,
                yaml_type,
                ExpectedType::Array,
            )),
        }
    } else {
        Ok(&[])
    }
}

pub fn parse_condition(
    hash: &saphyr::AnnotatedHash<MarkedYaml>,
    yaml_type: YamlObjectType,
) -> Result<Option<String>, ParseMetadataError> {
    match get_string_value(hash, "condition", yaml_type)? {
        Some((marker, s)) => {
            let s = s.to_string();
            if let Err(e) = Expression::from_str(&s) {
                return Err(ParseMetadataError::invalid_condition(marker, s, e));
            }
            Ok(Some(s))
        }
        None => Ok(None),
    }
}
