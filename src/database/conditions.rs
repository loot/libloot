use std::str::FromStr;

use loot_condition_interpreter::Expression;

use crate::metadata::{File, PluginCleaningData, PluginMetadata};

pub fn evaluate_all_conditions(
    mut metadata: PluginMetadata,
    state: &loot_condition_interpreter::State,
) -> Result<Option<PluginMetadata>, loot_condition_interpreter::Error> {
    metadata.set_load_after_files(filter_files_on_conditions(
        metadata.load_after_files(),
        state,
    )?);

    metadata.set_requirements(filter_files_on_conditions(metadata.requirements(), state)?);

    metadata.set_incompatibilities(filter_files_on_conditions(
        metadata.incompatibilities(),
        state,
    )?);

    metadata.set_messages(
        metadata
            .messages()
            .iter()
            .filter_map(|m| filter_map_on_condition(m, m.condition(), state))
            .collect::<Result<Vec<_>, _>>()?,
    );

    metadata.set_tags(
        metadata
            .tags()
            .iter()
            .filter_map(|t| filter_map_on_condition(t, t.condition(), state))
            .collect::<Result<Vec<_>, _>>()?,
    );

    if !metadata.is_regex_plugin() {
        metadata.set_dirty_info(filter_cleaning_data_on_conditions(
            metadata.name(),
            metadata.dirty_info(),
            state,
        )?);

        metadata.set_clean_info(filter_cleaning_data_on_conditions(
            metadata.name(),
            metadata.clean_info(),
            state,
        )?);
    }

    if metadata.has_name_only() {
        Ok(None)
    } else {
        Ok(Some(metadata))
    }
}

pub fn evaluate_condition(
    condition: &str,
    state: &loot_condition_interpreter::State,
) -> Result<bool, loot_condition_interpreter::Error> {
    Expression::from_str(condition).and_then(|e| e.eval(state))
}

fn evaluate_condition_option(
    condition: Option<&str>,
    state: &loot_condition_interpreter::State,
) -> Result<bool, loot_condition_interpreter::Error> {
    if let Some(condition) = condition {
        evaluate_condition(condition, state)
    } else {
        Ok(true)
    }
}

pub fn filter_map_on_condition<T: Clone>(
    item: &T,
    condition: Option<&str>,
    state: &loot_condition_interpreter::State,
) -> Option<Result<T, loot_condition_interpreter::Error>> {
    evaluate_condition_option(condition, state)
        .map(|r| r.then(|| item.clone()))
        .transpose()
}

fn filter_files_on_conditions(
    files: &[File],
    state: &loot_condition_interpreter::State,
) -> Result<Vec<File>, loot_condition_interpreter::Error> {
    files
        .iter()
        .filter_map(|file| filter_map_on_condition(file, file.condition(), state))
        .collect()
}

fn filter_cleaning_data_on_conditions(
    plugin_name: &str,
    cleaning_info: &[PluginCleaningData],
    state: &loot_condition_interpreter::State,
) -> Result<Vec<PluginCleaningData>, loot_condition_interpreter::Error> {
    if plugin_name.is_empty() {
        return Ok(Vec::new());
    }

    cleaning_info
        .iter()
        .filter_map(|i| {
            let condition = format!("checksum(\"{}\", {:08X})", plugin_name, i.crc());

            filter_map_on_condition(i, Some(condition.as_str()), state)
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    mod evaluate_all_conditions {
        use crate::{
            metadata::{Message, MessageType, Tag, TagSuggestion},
            tests::{BLANK_DIFFERENT_ESM, BLANK_ESM, BLANK_ESP, source_plugins_path},
        };

        use super::*;

        #[test]
        fn should_evaluate_all_conditions_on_metadata_in_plugin_metadata_object() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();
            plugin.set_group("group1".into());

            let condition = "file(\"missing.esp\")".to_owned();
            let files = vec![
                File::new(BLANK_ESP.into()),
                File::new(BLANK_DIFFERENT_ESM.into()).with_condition(condition.clone()),
            ];
            plugin.set_load_after_files(files.clone());
            plugin.set_requirements(files.clone());
            plugin.set_incompatibilities(files.clone());

            let message1 = Message::new(MessageType::Say, "content1".into());
            let message2 =
                Message::new(MessageType::Say, "content2".into()).with_condition(condition.clone());
            plugin.set_messages(vec![message1.clone(), message2]);

            let tag1 = Tag::new("Delev".into(), TagSuggestion::Addition);
            let tag2 =
                Tag::new("Relev".into(), TagSuggestion::Addition).with_condition(condition.clone());
            plugin.set_tags(vec![tag1.clone(), tag2]);

            let info1 = PluginCleaningData::new(0x374E_2A6F, "utility1".into());
            let info2 = PluginCleaningData::new(0xDEAD_BEEF, "utility2".into());
            plugin.set_dirty_info(vec![info1.clone(), info2.clone()]);
            plugin.set_clean_info(vec![info1.clone(), info2.clone()]);

            let state = loot_condition_interpreter::State::new(
                loot_condition_interpreter::GameType::Oblivion,
                source_plugins_path(crate::GameType::Oblivion),
            );
            let result = evaluate_all_conditions(plugin, &state).unwrap().unwrap();

            let expected_files = &[files[0].clone()];
            let expected_info = &[info1];
            assert_eq!("group1", result.group().unwrap());
            assert_eq!(expected_files, result.load_after_files());
            assert_eq!(expected_files, result.requirements());
            assert_eq!(expected_files, result.incompatibilities());
            assert_eq!(&[message1], result.messages());
            assert_eq!(&[tag1], result.tags());
            assert_eq!(expected_info, result.dirty_info());
            assert_eq!(expected_info, result.clean_info());
        }

        #[test]
        fn should_return_none_if_evaluated_plugin_metadata_has_name_only() {
            let mut plugin = PluginMetadata::new(BLANK_ESM).unwrap();

            let file = File::new(BLANK_DIFFERENT_ESM.into())
                .with_condition("file(\"missing.esp\")".into());
            plugin.set_load_after_files(vec![file]);

            let state = loot_condition_interpreter::State::new(
                loot_condition_interpreter::GameType::Oblivion,
                source_plugins_path(crate::GameType::Oblivion),
            );
            assert!(evaluate_all_conditions(plugin, &state).unwrap().is_none());
        }
    }
}
