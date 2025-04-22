use std::collections::HashSet;

use unicase::UniCase;

use crate::{
    EdgeType, Vertex, logging,
    sorting::error::{CyclicInteractionError, PluginGraphValidationError, UndefinedGroupError},
};

use super::{
    groups::GroupsGraph,
    plugins::{PluginSortingData, SortingPlugin},
};

pub fn validate_plugin_groups<T: SortingPlugin>(
    plugins_sorting_data: &[PluginSortingData<'_, T>],
    groups_graph: &GroupsGraph,
) -> Result<(), UndefinedGroupError> {
    let group_names: HashSet<&str> = groups_graph
        .node_indices()
        .map(|i| groups_graph[i].as_ref())
        .collect();

    for plugin in plugins_sorting_data {
        if !group_names.contains(plugin.group.as_ref()) {
            return Err(UndefinedGroupError::new(plugin.group.clone().into_string()));
        }
    }

    Ok(())
}

pub fn validate_specific_and_hardcoded_edges<T: SortingPlugin>(
    masters: &[PluginSortingData<'_, T>],
    blueprint_masters: &[PluginSortingData<'_, T>],
    non_masters: &[PluginSortingData<'_, T>],
    early_loading_plugins: &[String],
) -> Result<(), PluginGraphValidationError> {
    logging::trace!("Validating specific and early-loading plugin edges...");

    let non_masters_set: HashSet<UniCase<&str>> =
        non_masters.iter().map(|p| UniCase::new(p.name())).collect();
    let blueprint_masters_set: HashSet<UniCase<&str>> = blueprint_masters
        .iter()
        .map(|p| UniCase::new(p.name()))
        .collect();

    validate_masters(masters, &non_masters_set, &blueprint_masters_set)?;

    validate_non_masters(non_masters, &blueprint_masters_set)?;

    // There's at least one master, check that there are no hardcoded
    // non-masters.
    validate_early_loading_plugins(early_loading_plugins, masters, &non_masters_set)?;

    Ok(())
}

fn validate_masters<T: SortingPlugin>(
    masters: &[PluginSortingData<'_, T>],
    non_masters: &HashSet<UniCase<&str>>,
    blueprint_masters: &HashSet<UniCase<&str>>,
) -> Result<(), PluginGraphValidationError> {
    logging::trace!(
        "Validating specific and early-loading plugin edges for non-blueprint master files..."
    );
    masters
        .iter()
        .try_for_each(|m| validate_plugin(m, non_masters, blueprint_masters))
}

fn validate_non_masters<T: SortingPlugin>(
    non_masters: &[PluginSortingData<'_, T>],
    blueprint_masters: &HashSet<UniCase<&str>>,
) -> Result<(), PluginGraphValidationError> {
    logging::trace!("Validating specific and early-loading plugin edges for non-master files...");

    // Pass an empty set of non-masters so that the non-masters don't get validated against themselves.
    let empty_set = HashSet::new();

    non_masters
        .iter()
        .try_for_each(|p| validate_plugin(p, &empty_set, blueprint_masters))
}

fn validate_plugin<T: SortingPlugin>(
    plugin: &PluginSortingData<'_, T>,
    non_masters: &HashSet<UniCase<&str>>,
    blueprint_masters: &HashSet<UniCase<&str>>,
) -> Result<(), PluginGraphValidationError> {
    for master in plugin.masters()? {
        let key = UniCase::new(master.as_str());
        if non_masters.contains(&key) {
            return Err(CyclicInteractionError::new(vec![
                Vertex::new(master).with_out_edge_type(EdgeType::Master),
                Vertex::new(plugin.name().to_owned()).with_out_edge_type(EdgeType::MasterFlag),
            ])
            .into());
        }

        if blueprint_masters.contains(&key) {
            // Log a warning instead of throwing an exception because the game will
            // just ignore this master, and the issue can't be fixed without
            // editing the plugin and the blueprint master may not actually have
            // any of its records overridden.
            let plugin_type = if plugin.is_master {
                "master"
            } else {
                "non-master"
            };
            logging::warn!(
                "The {} plugin \"{}\" has the blueprint master \"{}\" as one of its masters",
                plugin_type,
                plugin.name(),
                master
            );
        }
    }

    validate_files(
        &plugin.masterlist_req,
        plugin.name(),
        non_masters,
        blueprint_masters,
        EdgeType::MasterlistRequirement,
    )?;

    validate_files(
        &plugin.user_req,
        plugin.name(),
        non_masters,
        blueprint_masters,
        EdgeType::UserRequirement,
    )?;

    validate_files(
        &plugin.masterlist_load_after,
        plugin.name(),
        non_masters,
        blueprint_masters,
        EdgeType::MasterlistLoadAfter,
    )?;

    validate_files(
        &plugin.user_load_after,
        plugin.name(),
        non_masters,
        blueprint_masters,
        EdgeType::UserLoadAfter,
    )?;

    Ok(())
}

fn validate_files(
    files: &[String],
    plugin_name: &str,
    non_masters: &HashSet<UniCase<&str>>,
    blueprint_masters: &HashSet<UniCase<&str>>,
    edge_type: EdgeType,
) -> Result<(), CyclicInteractionError> {
    for file in files {
        let key = UniCase::new(file.as_str());
        if non_masters.contains(&key) {
            return Err(CyclicInteractionError::new(vec![
                Vertex::new(file.clone()).with_out_edge_type(edge_type),
                Vertex::new(plugin_name.to_owned()).with_out_edge_type(EdgeType::MasterFlag),
            ]));
        }

        if blueprint_masters.contains(&key) {
            return Err(CyclicInteractionError::new(vec![
                Vertex::new(file.clone()).with_out_edge_type(edge_type),
                Vertex::new(plugin_name.to_owned()).with_out_edge_type(EdgeType::BlueprintMaster),
            ]));
        }
    }

    Ok(())
}

fn validate_early_loading_plugins<T: SortingPlugin>(
    early_loading_plugins: &[String],
    masters: &[PluginSortingData<'_, T>],
    non_masters: &HashSet<UniCase<&str>>,
) -> Result<(), CyclicInteractionError> {
    if let Some(master) = masters.first() {
        for plugin in early_loading_plugins {
            let key = UniCase::new(plugin.as_str());
            if non_masters.contains(&key) {
                // Just report the cycle to the first master.
                return Err(CyclicInteractionError::new(vec![
                    Vertex::new(plugin.to_string()).with_out_edge_type(EdgeType::Hardcoded),
                    Vertex::new(master.name().to_owned()).with_out_edge_type(EdgeType::MasterFlag),
                ]));
            }
        }
    }

    Ok(())
}
