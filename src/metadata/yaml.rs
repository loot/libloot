use saphyr::{AnnotatedArray, AnnotatedHash, MarkedYaml, Marker, Yaml, YamlData};

use crate::error::{GeneralError, YamlParseError};

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
        }
    }
}

pub fn to_yaml(yaml: &MarkedYaml) -> Yaml {
    match &yaml.data {
        saphyr::YamlData::Real(v) => Yaml::Real(v.clone()),
        saphyr::YamlData::Integer(v) => Yaml::Integer(*v),
        saphyr::YamlData::String(v) => Yaml::String(v.clone()),
        saphyr::YamlData::Boolean(v) => Yaml::Boolean(*v),
        saphyr::YamlData::Array(v) => Yaml::Array(to_array(v)),
        saphyr::YamlData::Hash(v) => Yaml::Hash(to_hash(v)),
        saphyr::YamlData::Alias(v) => Yaml::Alias(*v),
        saphyr::YamlData::Null => Yaml::Null,
        saphyr::YamlData::BadValue => Yaml::BadValue,
    }
}

fn to_array(array: &AnnotatedArray<MarkedYaml>) -> saphyr::Array {
    array.iter().map(to_yaml).collect()
}

fn to_hash(hash: &AnnotatedHash<MarkedYaml>) -> saphyr::Hash {
    hash.iter()
        .map(|(key, value)| (to_yaml(key), to_yaml(value)))
        .collect()
}

pub fn as_string_node(value: &str) -> MarkedYaml {
    MarkedYaml {
        span: saphyr_parser::Span::default(),
        data: YamlData::String(value.into()),
    }
}

pub fn get_string_value<'a>(
    hash: &'a AnnotatedHash<MarkedYaml>,
    key: &str,
    yaml_type: YamlObjectType,
) -> Result<Option<&'a str>, YamlParseError> {
    match hash.get(&as_string_node(key)) {
        Some(n) => match n.data.as_str() {
            Some(n) => Ok(Some(n)),
            None => Err(YamlParseError::new(
                n.span.start,
                format!("'{}' key in '{}' map is not a string", key, yaml_type),
            )),
        },
        None => Ok(None),
    }
}

pub fn get_required_string_value<'a>(
    marker: Marker,
    hash: &'a AnnotatedHash<MarkedYaml>,
    key: &str,
    yaml_type: YamlObjectType,
) -> Result<&'a str, YamlParseError> {
    match get_string_value(hash, key, yaml_type)? {
        Some(n) => Ok(n),
        None => Err(YamlParseError::missing_key(marker, key, yaml_type)),
    }
}

pub fn get_strings_vec_value<'a>(
    hash: &'a AnnotatedHash<MarkedYaml>,
    key: &str,
    yaml_type: YamlObjectType,
) -> Result<Vec<&'a str>, YamlParseError> {
    match hash.get(&as_string_node(key)) {
        Some(n) => match n.data.as_vec() {
            Some(n) => n
                .iter()
                .map(|e| match e.data.as_str() {
                    Some(s) => Ok(s),
                    None => Err(YamlParseError::new(
                        e.span.start,
                        "Element in list is not a string".into(),
                    )),
                })
                .collect::<Result<Vec<_>, _>>(),
            None => Err(YamlParseError::new(
                n.span.start,
                format!("'{}' key in '{}' map is not a list", key, yaml_type),
            )),
        },
        None => Ok(Vec::new()),
    }
}

pub fn get_as_hash(
    value: &MarkedYaml,
    yaml_type: YamlObjectType,
) -> Result<&AnnotatedHash<MarkedYaml>, YamlParseError> {
    match value.data.as_hash() {
        Some(h) => Ok(h),
        None => Err(YamlParseError::new(
            value.span.start,
            format!("'{}' object must be a map", yaml_type),
        )),
    }
}

pub fn get_u32_value(
    hash: &AnnotatedHash<MarkedYaml>,
    key: &str,
    yaml_type: YamlObjectType,
) -> Result<Option<u32>, GeneralError> {
    match hash.get(&as_string_node(key)) {
        Some(n) => match n.data.as_i64() {
            Some(n) => Ok(Some(n.try_into()?)),
            None => Err(YamlParseError::new(
                n.span.start,
                format!("'{}' key in '{}' map is not a string", key, yaml_type),
            )
            .into()),
        },
        None => Ok(None),
    }
}

pub fn get_as_slice<'a>(
    hash: &'a saphyr::AnnotatedHash<MarkedYaml>,
    key: &str,
    yaml_type: YamlObjectType,
) -> Result<&'a [MarkedYaml], YamlParseError> {
    if let Some(value) = hash.get(&as_string_node(key)) {
        match value.data.as_vec() {
            Some(n) => Ok(n.as_slice()),
            None => Err(YamlParseError::new(
                value.span.start,
                format!("'{}' key in '{}' map is not an array", key, yaml_type),
            )),
        }
    } else {
        Ok(&[])
    }
}
