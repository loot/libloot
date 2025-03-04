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

fn evaluate_condition(
    condition: Option<&str>,
    state: &loot_condition_interpreter::State,
) -> Result<bool, loot_condition_interpreter::Error> {
    if let Some(condition) = condition {
        Expression::from_str(condition).and_then(|e| e.eval(state))
    } else {
        Ok(true)
    }
}

pub fn filter_map_on_condition<T: Clone>(
    item: &T,
    condition: Option<&str>,
    state: &loot_condition_interpreter::State,
) -> Option<Result<T, loot_condition_interpreter::Error>> {
    evaluate_condition(condition, state)
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
        .collect::<Result<Vec<_>, _>>()
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
        .collect::<Result<Vec<_>, _>>()
}
