use std::sync::LazyLock;

use saphyr::{MarkedYaml, YamlData};

use crate::metadata::error::YamlMergeKeyError;

use super::as_string_node;

static MERGE_KEY: LazyLock<MarkedYaml> = LazyLock::new(|| as_string_node("<<"));

pub fn process_merge_keys(mut yaml: MarkedYaml) -> Result<MarkedYaml, YamlMergeKeyError> {
    match yaml.data {
        YamlData::Sequence(a) => {
            yaml.data = merge_array_elements(a).map(YamlData::Sequence)?;
            Ok(yaml)
        }
        YamlData::Mapping(h) => {
            yaml.data = merge_mapping_keys(h).map(YamlData::Mapping)?;
            Ok(yaml)
        }
        _ => Ok(yaml),
    }
}

fn merge_array_elements(
    array: saphyr::AnnotatedSequence<MarkedYaml>,
) -> Result<saphyr::AnnotatedSequence<MarkedYaml>, YamlMergeKeyError> {
    array.into_iter().map(process_merge_keys).collect()
}

fn merge_mapping_keys<'a, 'b>(
    mapping: saphyr::AnnotatedMapping<'a, MarkedYaml<'b>>,
) -> Result<saphyr::AnnotatedMapping<'a, MarkedYaml<'b>>, YamlMergeKeyError> {
    let mut mapping: saphyr::AnnotatedMapping<MarkedYaml> = mapping
        .into_iter()
        .map(|(key, value)| {
            process_merge_keys(key)
                .and_then(|key| process_merge_keys(value).map(|value| (key, value)))
        })
        .collect::<Result<_, _>>()?;

    if let Some(value) = mapping.remove(&MERGE_KEY) {
        merge_into_mapping(mapping, value)
    } else {
        Ok(mapping)
    }
}

fn merge_into_mapping<'a, 'b>(
    mapping: saphyr::AnnotatedMapping<'a, MarkedYaml<'b>>,
    value: MarkedYaml<'b>,
) -> Result<saphyr::AnnotatedMapping<'a, MarkedYaml<'b>>, YamlMergeKeyError> {
    match value.data {
        YamlData::Sequence(a) => a.into_iter().try_fold(mapping, |acc, e| {
            if let YamlData::Mapping(h) = e.data {
                Ok(merge_mappings(acc, h))
            } else {
                Err(YamlMergeKeyError::new(&e))
            }
        }),
        YamlData::Mapping(h) => Ok(merge_mappings(mapping, h)),
        _ => Err(YamlMergeKeyError::new(&value)),
    }
}

fn merge_mappings<'a, 'b>(
    mut mapping1: saphyr::AnnotatedMapping<'a, MarkedYaml<'b>>,
    mapping2: saphyr::AnnotatedMapping<MarkedYaml<'b>>,
) -> saphyr::AnnotatedMapping<'a, MarkedYaml<'b>> {
    for (key, value) in mapping2 {
        mapping1.entry(key).or_insert(value);
    }
    mapping1
}

#[cfg(test)]
mod tests {
    use super::*;

    mod process_merge_keys {
        use crate::metadata::parse;

        use super::*;

        #[test]
        fn should_error_if_merge_key_value_has_a_single_value_that_is_not_a_hash() {
            let yaml = parse(
                "
- &anchor1 test
- <<: *anchor1
  value: test-value-2",
            );

            let error_message = process_merge_keys(yaml).unwrap_err().to_string();

            assert_eq!(
                "invalid YAML merge key value at line 3 column 6: test",
                error_message
            );
        }

        #[test]
        fn should_error_if_merge_key_value_is_an_array_of_non_hash_values() {
            let yaml = parse(
                "
- &anchor1 {key: test-key}
- &anchor2 test
- <<: [*anchor1, *anchor2]
  value: test-value-2",
            );

            let error_message = process_merge_keys(yaml).unwrap_err().to_string();

            assert_eq!(
                "invalid YAML merge key value at line 4 column 17: test",
                error_message
            );
        }
    }
}
