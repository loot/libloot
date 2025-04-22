use std::str::FromStr;

use loot_condition_interpreter::Expression;
use saphyr::{AnnotatedMapping, MarkedYaml, Marker, Scalar, Yaml, YamlData};

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

pub fn to_unmarked_yaml<'a>(yaml: &MarkedYaml<'a>) -> Yaml<'a> {
    match &yaml.data {
        YamlData::Value(Scalar::FloatingPoint(v)) => Yaml::Value(Scalar::FloatingPoint(*v)),
        YamlData::Value(Scalar::Integer(v)) => Yaml::Value(Scalar::Integer(*v)),
        YamlData::Value(Scalar::String(v)) => Yaml::Value(Scalar::String(v.clone())),
        YamlData::Value(Scalar::Boolean(v)) => Yaml::Value(Scalar::Boolean(*v)),
        YamlData::Value(Scalar::Null) => Yaml::Value(Scalar::Null),
        YamlData::Sequence(v) => Yaml::Sequence(v.iter().map(to_unmarked_yaml).collect()),
        YamlData::Mapping(v) => Yaml::Mapping(
            v.iter()
                .map(|(key, value)| (to_unmarked_yaml(key), to_unmarked_yaml(value)))
                .collect(),
        ),
        YamlData::Alias(v) => Yaml::Alias(*v),
        YamlData::BadValue => Yaml::BadValue,
        YamlData::Representation(v, s, t) => Yaml::Representation(v.clone(), *s, t.clone()),
    }
}

pub fn as_string_node(value: &str) -> MarkedYaml {
    MarkedYaml {
        span: saphyr_parser::Span::default(),
        data: YamlData::Value(Scalar::String(value.into())),
    }
}

pub fn get_string_value<'a>(
    mapping: &'a AnnotatedMapping<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<Option<(Marker, &'a str)>, ParseMetadataError> {
    match mapping.get(&as_string_node(key)) {
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
    mapping: &'a AnnotatedMapping<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<&'a str, ParseMetadataError> {
    match get_string_value(mapping, key, yaml_type)? {
        Some(n) => Ok(n.1),
        None => Err(ParseMetadataError::missing_key(marker, key, yaml_type)),
    }
}

pub fn get_strings_vec_value<'a>(
    mapping: &'a AnnotatedMapping<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<Vec<&'a str>, ParseMetadataError> {
    match mapping.get(&as_string_node(key)) {
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
                .collect(),
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

pub fn as_mapping<'a, 'b>(
    value: &'a MarkedYaml<'b>,
    yaml_type: YamlObjectType,
) -> Result<&'a AnnotatedMapping<'a, MarkedYaml<'b>>, ParseMetadataError> {
    match value.data.as_mapping() {
        Some(h) => Ok(h),
        None => Err(ParseMetadataError::unexpected_type(
            value.span.start,
            yaml_type,
            ExpectedType::Map,
        )),
    }
}

pub fn get_u32_value(
    mapping: &AnnotatedMapping<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<Option<u32>, ParseMetadataError> {
    match mapping.get(&as_string_node(key)) {
        Some(n) => match n.data.as_integer() {
            Some(i) => i.try_into().map(Some).map_err(|_e| {
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
    mapping: &'a saphyr::AnnotatedMapping<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<&'a [MarkedYaml<'a>], ParseMetadataError> {
    if let Some(value) = mapping.get(&as_string_node(key)) {
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
    mapping: &saphyr::AnnotatedMapping<MarkedYaml>,
    key: &'static str,
    yaml_type: YamlObjectType,
) -> Result<Option<Box<str>>, ParseMetadataError> {
    match get_string_value(mapping, key, yaml_type)? {
        Some((marker, s)) => {
            let s = s.to_owned();
            if let Err(e) = Expression::from_str(&s) {
                return Err(ParseMetadataError::invalid_condition(marker, s, e));
            }
            Ok(Some(s.into_boxed_str()))
        }
        None => Ok(None),
    }
}

/// This is effectively TryFrom<&MarkedYaml>, but implementing it doesn't make
/// MarkedYaml part of the crate's public API.
pub trait TryFromYaml: Sized {
    fn try_from_yaml(value: &MarkedYaml) -> Result<Self, ParseMetadataError>;
}
