use std::sync::LazyLock;

use saphyr::{MarkedYaml, YamlData};

use crate::metadata::error::YamlMergeKeyError;

use super::as_string_node;

static MERGE_KEY: LazyLock<MarkedYaml> = LazyLock::new(|| as_string_node("<<"));

pub fn process_merge_keys(mut yaml: MarkedYaml) -> Result<MarkedYaml, YamlMergeKeyError> {
    match yaml.data {
        YamlData::Array(a) => {
            yaml.data = merge_array_elements(a).map(YamlData::Array)?;
            Ok(yaml)
        }
        YamlData::Hash(h) => {
            yaml.data = merge_hash_keys(h).map(YamlData::Hash)?;
            Ok(yaml)
        }
        _ => Ok(yaml),
    }
}

fn merge_array_elements(
    array: saphyr::AnnotatedArray<MarkedYaml>,
) -> Result<saphyr::AnnotatedArray<MarkedYaml>, YamlMergeKeyError> {
    array.into_iter().map(process_merge_keys).collect()
}

fn merge_hash_keys(
    hash: saphyr::AnnotatedHash<MarkedYaml>,
) -> Result<saphyr::AnnotatedHash<MarkedYaml>, YamlMergeKeyError> {
    let mut hash: saphyr::AnnotatedHash<MarkedYaml> = hash
        .into_iter()
        .map(|(key, value)| {
            process_merge_keys(key)
                .and_then(|key| process_merge_keys(value).map(|value| (key, value)))
        })
        .collect::<Result<_, _>>()?;

    if let Some(value) = hash.remove(&MERGE_KEY) {
        merge_into_hash(hash, value)
    } else {
        Ok(hash)
    }
}

fn merge_into_hash(
    hash: saphyr::AnnotatedHash<MarkedYaml>,
    value: MarkedYaml,
) -> Result<saphyr::AnnotatedHash<MarkedYaml>, YamlMergeKeyError> {
    match value.data {
        YamlData::<MarkedYaml>::Array(a) => a.into_iter().try_fold(hash, |acc, e| {
            if let YamlData::Hash(h) = e.data {
                Ok(merge_hashes(acc, h))
            } else {
                Err(YamlMergeKeyError::new(e))
            }
        }),
        YamlData::<MarkedYaml>::Hash(h) => Ok(merge_hashes(hash, h)),
        _ => Err(YamlMergeKeyError::new(value)),
    }
}

fn merge_hashes(
    mut hash1: saphyr::AnnotatedHash<MarkedYaml>,
    hash2: saphyr::AnnotatedHash<MarkedYaml>,
) -> saphyr::AnnotatedHash<MarkedYaml> {
    for (key, value) in hash2 {
        hash1.entry(key).or_insert(value);
    }
    hash1
}
