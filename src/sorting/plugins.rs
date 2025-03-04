use std::{
    collections::{HashMap, HashSet},
    rc::Rc,
};

use log::log_enabled;
use petgraph::{
    Graph,
    graph::{EdgeReference, NodeIndex},
    visit::EdgeRef,
};

use crate::{
    EdgeType, Plugin,
    metadata::{File, Group, PluginMetadata},
    plugin::error::PluginDataError,
    sorting::error::{CyclicInteractionError, PathfindingError, SortingError, UndefinedGroupError},
    sorting::groups::{get_default_group_node, sorted_group_nodes},
};

use super::{
    dfs::{BidirBfsVisitor, DfsVisitor, bidirectional_bfs, depth_first_search, find_cycle},
    groups::GroupsGraph,
    validate::{validate_plugin_groups, validate_specific_and_hardcoded_edges},
};

#[derive(Debug)]
pub struct PluginSortingData<'a> {
    plugin: &'a Plugin,
    pub(super) is_master: bool,
    override_record_count: usize,

    load_order_index: usize,

    pub(super) group: String,
    group_is_user_metadata: bool,
    pub(super) masterlist_load_after: Vec<String>,
    pub(super) user_load_after: Vec<String>,
    pub(super) masterlist_req: Vec<String>,
    pub(super) user_req: Vec<String>,
}

impl<'a> PluginSortingData<'a> {
    pub fn new(
        plugin: &'a Plugin,
        masterlist_metadata: Option<&PluginMetadata>,
        user_metadata: Option<&PluginMetadata>,
        load_order_index: usize,
    ) -> Result<Self, PluginDataError> {
        let override_record_count = plugin.override_record_count()?;

        Ok(Self {
            plugin,
            is_master: plugin.is_master(),
            override_record_count,
            load_order_index,
            group: user_metadata
                .and_then(|m| m.group())
                .or_else(|| masterlist_metadata.and_then(|m| m.group()))
                .unwrap_or(Group::DEFAULT_NAME)
                .to_string(),
            group_is_user_metadata: user_metadata.and_then(|m| m.group()).is_some(),
            masterlist_load_after: masterlist_metadata
                .map(|m| to_filenames(m.load_after_files()))
                .unwrap_or_default(),
            user_load_after: user_metadata
                .map(|m| to_filenames(m.load_after_files()))
                .unwrap_or_default(),
            masterlist_req: masterlist_metadata
                .map(|m| to_filenames(m.requirements()))
                .unwrap_or_default(),
            user_req: user_metadata
                .map(|m| to_filenames(m.requirements()))
                .unwrap_or_default(),
        })
    }

    pub(super) fn name(&self) -> &str {
        self.plugin.name()
    }

    fn is_blueprint_master(&self) -> bool {
        self.is_master && self.plugin.is_blueprint_plugin()
    }

    fn asset_count(&self) -> usize {
        self.plugin.asset_count()
    }

    pub(super) fn masters(&self) -> Result<Vec<String>, PluginDataError> {
        self.plugin.masters()
    }

    fn do_records_overlap(&self, other: &PluginSortingData) -> Result<bool, PluginDataError> {
        self.plugin.do_records_overlap(other.plugin)
    }

    fn do_assets_overlap(&self, other: &PluginSortingData) -> bool {
        self.plugin.do_assets_overlap(other.plugin)
    }
}

fn to_filenames(files: &[File]) -> Vec<String> {
    files
        .iter()
        .map(|f| f.name().as_str().to_string())
        .collect()
}

type InnerPluginsGraph<'a> = Graph<Rc<PluginSortingData<'a>>, EdgeType>;

#[derive(Debug, Default)]
struct PluginsGraph<'a> {
    // Put the sorting data in Rc so that it can be held onto while mutating the graph.
    inner: InnerPluginsGraph<'a>,
    paths_cache: HashMap<NodeIndex, HashSet<NodeIndex>>,
}

impl<'a> PluginsGraph<'a> {
    fn new() -> Self {
        PluginsGraph::default()
    }

    fn add_node(&mut self, plugin: PluginSortingData<'a>) {
        self.inner.add_node(Rc::new(plugin));
    }

    fn add_edge(&mut self, from: NodeIndex, to: NodeIndex, edge_type: EdgeType) {
        if self.is_path_cached(from, to) {
            return;
        }

        log::debug!(
            "Adding {} edge from \"{}\" to \"{}\".",
            edge_type,
            self.inner[from].name(),
            self.inner[to].name()
        );

        self.inner.add_edge(from, to, edge_type);

        self.cache_path(from, to);
    }

    fn node_indices(&self) -> petgraph::graph::NodeIndices {
        self.inner.node_indices()
    }

    fn add_specific_edges(&mut self) -> Result<(), SortingError> {
        log::trace!("Adding edges based on plugin data and non-group metadata...");

        let mut node_index_iter = self.node_indices();
        while let Some(node_index) = node_index_iter.next() {
            let plugin = Rc::clone(&self[node_index]);

            // This loop should have no effect now that master-flagged and
            // non-master-flagged plugins are sorted separately, but is kept
            // as a safety net.
            for other_node_index in node_index_iter.clone() {
                let other_plugin = &self[other_node_index];

                if plugin.is_master == other_plugin.is_master {
                    continue;
                }

                if other_plugin.is_master {
                    self.add_edge(other_node_index, node_index, EdgeType::MasterFlag);
                } else {
                    self.add_edge(node_index, other_node_index, EdgeType::MasterFlag);
                }
            }

            for master in plugin.masters()? {
                if let Some(other_node_index) = self.node_index_by_name(&master) {
                    self.add_edge(other_node_index, node_index, EdgeType::Master);
                }
            }

            for file in &plugin.masterlist_req {
                if let Some(other_node_index) = self.node_index_by_name(file) {
                    self.add_edge(
                        other_node_index,
                        node_index,
                        EdgeType::MasterlistRequirement,
                    );
                }
            }

            for file in &plugin.user_req {
                if let Some(other_node_index) = self.node_index_by_name(file) {
                    self.add_edge(other_node_index, node_index, EdgeType::UserRequirement);
                }
            }

            for file in &plugin.masterlist_load_after {
                if let Some(other_node_index) = self.node_index_by_name(file) {
                    self.add_edge(other_node_index, node_index, EdgeType::MasterlistLoadAfter);
                }
            }

            for file in &plugin.user_load_after {
                if let Some(other_node_index) = self.node_index_by_name(file) {
                    self.add_edge(other_node_index, node_index, EdgeType::UserLoadAfter);
                }
            }
        }

        Ok(())
    }

    fn add_early_loading_plugin_edges(&mut self, early_loading_plugins: &[String]) {
        log::trace!(
            "Adding edges for implicitly active plugins and plugins with hardcoded positions..."
        );

        if early_loading_plugins.is_empty() {
            return;
        }

        let mut early_loading_plugin_indices: HashMap<&str, NodeIndex> = HashMap::new();
        let mut other_plugin_indices = Vec::new();
        for node_index in self.node_indices() {
            let plugin = &self[node_index];
            if let Some(p) = early_loading_plugins
                .iter()
                .find(|e| unicase::eq(e.as_str(), plugin.name()))
            {
                early_loading_plugin_indices.insert(p.as_str(), node_index);
            } else {
                other_plugin_indices.push(node_index);
            }
        }

        if early_loading_plugin_indices.is_empty() {
            return;
        }

        let mut last_early_loading_plugin_index = None;
        for window in early_loading_plugins.windows(2) {
            if let [from, to] = window {
                let from_index = early_loading_plugin_indices.get(from.as_str());
                let to_index = early_loading_plugin_indices.get(to.as_str());

                if to_index.is_some() {
                    last_early_loading_plugin_index = to_index;
                } else if from_index.is_some() {
                    last_early_loading_plugin_index = from_index;
                }

                if let (Some(from_index), Some(to_index)) = (from_index, to_index) {
                    self.add_edge(*from_index, *to_index, EdgeType::Hardcoded);
                }
            }
        }

        if let Some(from_index) = last_early_loading_plugin_index {
            for to_index in other_plugin_indices {
                self.add_edge(*from_index, to_index, EdgeType::Hardcoded);
            }
        }
    }

    fn check_for_cycles(&mut self) -> Result<(), CyclicInteractionError> {
        if let Some(cycle) = find_cycle(&self.inner, |node| node.name().to_string()) {
            Err(CyclicInteractionError::new(cycle))
        } else {
            Ok(())
        }
    }

    fn add_group_edges(&mut self, groups_graph: &GroupsGraph) -> Result<(), UndefinedGroupError> {
        log::trace!("Adding edges based on plugin group memberships...");

        // First build a map from groups to the plugins in those groups.
        let plugins_in_groups = get_plugins_in_groups(&self.inner);

        // Get the default group's vertex because it's needed for the DFSes.
        let default_group_node = get_default_group_node(groups_graph)?;

        // Keep a record of which vertices have already been fully explored to avoid
        // adding edges from their plugins more than once.
        let mut finished_nodes = HashSet::new();
        // Now loop over the vertices in the groups graph.
        // The vertex sort order prioritises resolving potential cycles in
        // favour of earlier-loading groups. It does not guarantee that the
        // longest paths will be walked first, because a root vertex may be in
        // more than one path and the vertex sort order here does not influence
        // which path the DFS takes.
        for group_node in sorted_group_nodes(groups_graph) {
            // Run a DFS from each vertex in the group graph, adding edges except from
            // plugins in the default group. This could be run only on the root
            // vertices, except that the DFS only visits each vertex once, so a branch
            // and merge inside a given root's DAG would result in plugins from one of
            // the branches not being carried forwards past the point at which the
            // branches merge.
            let mut visitor = GroupsPathVisitor::new(
                self,
                groups_graph,
                &plugins_in_groups,
                &mut finished_nodes,
                Some(default_group_node),
            );

            depth_first_search(groups_graph, &mut HashMap::new(), group_node, &mut visitor);
        }

        // Now do one last DFS starting from the default group and not ignoring its
        // plugins.
        let mut visitor = GroupsPathVisitor::new(
            self,
            groups_graph,
            &plugins_in_groups,
            &mut finished_nodes,
            None,
        );

        depth_first_search(
            groups_graph,
            &mut HashMap::new(),
            default_group_node,
            &mut visitor,
        );

        Ok(())
    }

    fn add_overlap_edges(&mut self) -> Result<(), SortingError> {
        log::trace!("Adding edges for overlapping plugins...");

        let mut node_index_iter = self.node_indices();
        while let Some(node_index) = node_index_iter.next() {
            let plugin = Rc::clone(&self[node_index]);
            let plugin_asset_count = plugin.asset_count();

            if plugin.override_record_count == 0 && plugin_asset_count == 0 {
                log::debug!(
                    "Skipping vertex for \"{}\": the plugin contains no override records and loads no assets",
                    plugin.name()
                );
                continue;
            }

            // This loop should have no effect now that master-flagged and
            // non-master-flagged plugins are sorted separately, but is kept
            // as a safety net.
            for other_node_index in node_index_iter.clone() {
                let other_plugin = &self[other_node_index];

                // Don't add an edge between these two plugins if one already
                // exists (only check direct edges and not paths for efficiency).
                if self.inner.contains_edge(node_index, other_node_index)
                    || self.inner.contains_edge(other_node_index, node_index)
                {
                    continue;
                }

                // Two plugins can overlap due to overriding the same records,
                // or by loading assets from BSAs/BA2s that have the same path.
                // If records overlap, the plugin that overrides more records
                // should load earlier.
                // If assets overlap, the plugin that loads more assets should
                // load earlier.
                // If two plugins have overlapping records and assets and one
                // overrides more records but loads fewer assets than the other,
                // the fact it overrides more records should take precedence
                // (records are more significant than assets).
                // I.e. if two plugins don't have overlapping records, check their
                // assets, otherwise only check their assets if their override
                // record counts are equal.

                let outer_plugin_loads_first;
                let edge_type;

                if plugin.override_record_count == other_plugin.override_record_count
                    || !plugin.do_records_overlap(other_plugin)?
                {
                    // Records don't overlap, or override the same number of records,
                    // check assets.
                    // No records overlap, check assets.
                    let other_plugin_asset_count = other_plugin.asset_count();
                    if plugin_asset_count == other_plugin_asset_count
                        || !plugin.do_assets_overlap(other_plugin)
                    {
                        // Assets don't overlap or both plugins load the same number of
                        // assets, don't add an edge.
                        continue;
                    } else {
                        outer_plugin_loads_first = plugin_asset_count > other_plugin_asset_count;
                        edge_type = EdgeType::AssetOverlap;
                    }
                } else {
                    // Records overlap and override different numbers of records.
                    // Load this plugin first if it overrides more records.
                    outer_plugin_loads_first =
                        plugin.override_record_count > other_plugin.override_record_count;
                    edge_type = EdgeType::RecordOverlap
                }

                let (from_index, to_index) = if outer_plugin_loads_first {
                    (node_index, other_node_index)
                } else {
                    (other_node_index, node_index)
                };

                if !self.is_path_cached(from_index, to_index) {
                    if !self.path_exists(to_index, from_index) {
                        self.add_edge(from_index, to_index, edge_type);
                    } else {
                        log::debug!(
                            "Skipping {} edge from \"{}\" to \"{}\" as it would create a cycle.",
                            edge_type,
                            self[from_index].name(),
                            self[to_index].name()
                        );
                    }
                }
            }
        }

        Ok(())
    }

    fn add_tie_break_edges(&mut self) -> Result<(), PathfindingError> {
        log::trace!("Adding edges to break ties between plugins...");

        // In order for the sort to be performed stably, there must be only one
        // possible result. This can be enforced by adding edges between all vertices
        // that aren't already linked. Use existing load order to decide the direction
        // of these edges, and only add an edge if it won't cause a cycle.
        //
        // Brute-forcing this by adding an edge between every pair of vertices
        // (unless it would cause a cycle) works but scales terribly, as before each
        // edge is added a bidirectional search needs to be done for a path in the
        // other direction (to detect a potential cycle). This search takes more time
        // as the number of edges involves increases, so adding tie breaks gets slower
        // as they get added.
        //
        // The point of adding these tie breaks is to ensure that there's a
        // Hamiltonian path through the graph and therefore only one possible
        // topological sort result.
        //
        // Instead of trying to brute-force this, iterate over the graph's vertices in
        // their existing load order (each vertex represents a plugin, so the two
        // terms are used interchangeably), and add an edge going from the earlier to
        // the later for each consecutive pair of plugins (e.g. for [A, B, C], add
        // edges A->B, B->C), unless adding the edge would cause a cycle. If sorting
        // has made no changes to the load order, then it'll be possible to add all
        // those edges and only N - 1 bidirectional searches will be needed when there
        // are N vertices.
        //
        // If it's not possible to add such an edge for a pair of plugins [A, B], that
        // means that LOOT thinks A needs to load after B, i.e. the sorted load order
        // will be different. If the existing path between A and B is B -> C -> D -> A
        // then walk back through the load order to find a plugin that B will load
        // after without causing a cycle, and add an edge going from that plugin to B.
        // Then do the same for each subsequent plugin in the path between A and B so
        // that every plugin in the existing load order until A has a path to each of
        // the plugins in the path from B to A, and that there is only one path that
        // will visit all plugins until A. Keep a record of this path, because that's
        // the load order that needs to be walked back through whenever the existing
        // relative positions of plugins can't be used (if the existing load order was
        // used, the process would miss out on plugins introduced in previous backward
        // walks, and so you'd end up with multiple paths that don't necessarily touch
        // all plugins).

        // Storage for the load order as it evolves.
        let mut new_load_order: Vec<NodeIndex> = Vec::new();

        // Holds nodes that have already been put into new_load_order.
        let mut processed_nodes = HashSet::new();

        // First get the graph vertices and sort them into the current load order.
        let mut nodes: Vec<_> = self.node_indices().collect();
        nodes.sort_by_key(|a| self[*a].load_order_index);

        for window in nodes.windows(2) {
            let (current, next) = match window {
                [a, b] => (*a, *b),
                _ => panic!("Window length should be 2, got {}", window.len()),
            };

            match self.find_path(next, current)? {
                None => {
                    // There's no path from next to current, so it's OK to add
                    // an edge going in the other direction, meaning that next can
                    // load after current.
                    self.add_edge(current, next, EdgeType::TieBreak);

                    // next now loads after current. If current hasn't
                    // already been added to the load order, append it. It might have already
                    // been added if it was part of a path going from next and
                    // current in a previous loop (i.e. for different values of
                    // next and current).
                    if !processed_nodes.contains(&current) {
                        new_load_order.push(current);
                        processed_nodes.insert(current);

                        log::debug!(
                            "The plugin \"{}\" loads at the end of the new load order so far.",
                            self[current].name()
                        );
                    } else if new_load_order.last() != Some(&current) {
                        log::trace!(
                            "The plugin \"{}\" has already been processed and is not the last in the new load order, determining where to place \"{}\".",
                            self[current].name(),
                            self[next].name()
                        );

                        // If current was already processed and not the last vertex
                        // in new_load_order then next also needs to be pinned in place or
                        // it may not have a defined position relative to all the
                        // vertices following current in new_load_order undefined, so
                        // there wouldn't be a unique path through them.
                        //
                        // We're using new_load_order.rend() as the last iterator position because
                        // we don't know current's position.
                        self.pin_node_position(&mut processed_nodes, &mut new_load_order, next, 0);
                    }
                }
                Some(mut path_from_next_node) => {
                    // Each vertex in pathFromNextVertex (besides the last, which is
                    // currentVertex) needs to be positioned relative to a vertex that has
                    // already been iterated over (i.e. in what begins as the old load
                    // order) so that there is a single path between all vertices.
                    //
                    // If currentVertex is the first in the iteration order, then
                    // nextVertex is simply the earliest known plugin in the new load order
                    // so far.
                    if nodes.first() == Some(&current) {
                        // Record the path as the start of the new load order.
                        // Don't need to add any edges because there's nothing for nextVertex
                        // to load after at this point.
                        if log_enabled!(log::Level::Debug) {
                            log::debug!(
                                "The path ends with the first plugin checked, treating the following path as the start of the load order: {}",
                                path_to_string(&self.inner, &path_from_next_node)
                            );
                        }

                        for node in path_from_next_node {
                            new_load_order.push(node);
                            processed_nodes.insert(node);
                        }
                        continue;
                    }

                    // Ignore the last vertex in the path because it's currentVertex and
                    // will just be appended to the load order so doesn't need special
                    // processing.
                    path_from_next_node.pop();

                    // This is used to keep track of when to stop searching for a
                    // vertex to load after, as a minor optimisation.
                    let mut range_start = 0;

                    // Iterate over the path going from nextVertex towards currentVertex
                    // (which got chopped off the end of the path).
                    for node in path_from_next_node {
                        // Update reverseEndIt to reduce the scope of the search in the
                        // next loop (if there is one).
                        range_start = self.pin_node_position(
                            &mut processed_nodes,
                            &mut new_load_order,
                            node,
                            range_start,
                        );
                    }

                    // Add current to the end of the new_load_order - do this after processing the other vertices in the path so that involves less work.
                    if !processed_nodes.contains(&current) {
                        new_load_order.push(current);
                        processed_nodes.insert(current);
                    }
                }
            }
        }

        Ok(())
    }

    fn pin_node_position(
        &mut self,
        processed_nodes: &mut HashSet<NodeIndex>,
        new_load_order: &mut Vec<NodeIndex>,
        node_index: NodeIndex,
        range_start: usize,
    ) -> usize {
        // It's possible that this vertex has already been pinned in place,
        // e.g. because it was visited earlier in the old load order or
        // as part of a path that was processed. In that case just skip it.
        if processed_nodes.contains(&node_index) {
            log::debug!(
                "The plugin \"{}\" has already been processed, skipping it.",
                self[node_index].name()
            );
            return range_start;
        }

        // Otherwise, this vertex needs to be inserted into the path that includes
        // all other vertices that have been processed so far. This can be done by
        // searching for the last vertex in the "new load order" path for which
        // there is not a path going from this vertex to that vertex. I.e. find the
        // last plugin that this one can load after. We could instead find the last
        // plugin that this one *must* load after, but it turns out that's
        // significantly slower because it generally involves going further back
        // along the "new load order" path.
        let previous_node_position = new_load_order
            .get(range_start..)
            .expect("last_pos is within the new_load_order vec")
            .iter()
            .rposition(|ni| !self.path_exists(node_index, *ni));

        // Add an edge going from the found vertex to this one, in case it
        // doesn't exist (we only know there's not a path going the other way).
        if let Some(preceding_node_index) =
            previous_node_position.and_then(|p| new_load_order.get(p))
        {
            self.add_edge(*preceding_node_index, node_index, EdgeType::TieBreak);
        }

        // Insert position is just after the found vertex, and a forward iterator
        // points to the element one after the element pointed to by the
        // corresponding reverse iterator.
        let insert_position = previous_node_position.map(|i| i + 1).unwrap_or(0);

        // Add an edge going from this vertex to the next one in the "new load
        // order" path, in case there isn't already one.
        if let Some(following_node_index) = new_load_order.get(insert_position) {
            self.add_edge(node_index, *following_node_index, EdgeType::TieBreak);
        }

        // Now update newLoadOrder with the vertex's new position.
        new_load_order.insert(insert_position, node_index);

        if log_enabled!(log::Level::Debug) {
            if let Some(next_node_index) = new_load_order.get(insert_position + 1) {
                log::debug!(
                    "The plugin \"{}\" loads before \"{}\" in the new load order.",
                    self[node_index].name(),
                    self[*next_node_index].name()
                );
            } else {
                log::debug!(
                    "The plugin \"{}\" loads at the end of the new load order so far.",
                    self[node_index].name()
                );
            }
        }

        // Return a new value for reverseEndIt, pointing to the newly
        // inserted vertex, as if it was not the last vertex in a path
        // being processed the next vertex in the path by definition
        // cannot load before this one, so we can save an unnecessary
        // check by using this new reverseEndIt value when pinning the
        // next vertex.
        insert_position + 1
    }

    fn topological_sort(&self) -> Result<Vec<NodeIndex>, SortingError> {
        petgraph::algo::toposort(&self.inner, None)
            .map_err(|e| SortingError::CycleInvolving(self[e.node_id()].name().to_string()))
    }

    fn is_hamiltonian_path(&mut self, path: &[NodeIndex]) -> Option<(NodeIndex, NodeIndex)> {
        log::trace!("Checking uniqueness of path through plugin graph...");

        path.windows(2).find_map(|slice| match slice {
            [a, b] => self.inner.contains_edge(*a, *b).then_some((*a, *b)),
            _ => None,
        })
    }

    fn cache_path(&mut self, from: NodeIndex, to: NodeIndex) {
        self.paths_cache.entry(from).or_default().insert(to);
    }

    fn is_path_cached(&self, from: NodeIndex, to: NodeIndex) -> bool {
        self.paths_cache
            .get(&from)
            .map(|s| s.contains(&to))
            .unwrap_or(false)
    }

    fn node_index_by_name(&self, name: &str) -> Option<NodeIndex> {
        self.node_indices()
            .find(|i| unicase::eq(self[*i].name(), name))
    }

    fn path_exists(&mut self, from: NodeIndex, to: NodeIndex) -> bool {
        if self.is_path_cached(from, to) {
            return true;
        }

        let mut visitor = PathCacher::new(&mut self.paths_cache, from, to);

        bidirectional_bfs(&self.inner, from, to, &mut visitor)
    }

    fn find_path(
        &mut self,
        from: NodeIndex,
        to: NodeIndex,
    ) -> Result<Option<Vec<NodeIndex>>, PathfindingError> {
        let mut path_finder = PathFinder::new(&self.inner, &mut self.paths_cache, from, to);

        if bidirectional_bfs(&self.inner, from, to, &mut path_finder) {
            path_finder.path()
        } else {
            Ok(None)
        }
    }
}

impl<'a> std::ops::Index<NodeIndex> for PluginsGraph<'a> {
    type Output = Rc<PluginSortingData<'a>>;

    fn index(&self, index: NodeIndex) -> &Self::Output {
        &self.inner[index]
    }
}

pub fn sort_plugins(
    mut plugins_sorting_data: Vec<PluginSortingData>,
    groups_graph: &GroupsGraph,
    early_loading_plugins: &[String],
) -> Result<Vec<String>, SortingError> {
    if plugins_sorting_data.is_empty() {
        return Ok(Vec::new());
    }

    validate_plugin_groups(&plugins_sorting_data, groups_graph)?;

    // Sort the plugins according to the lexicographical order of their names.
    // This ensures a consistent iteration order for vertices given the same
    // input data. The vertex iteration order can affect what edges get added
    // and so the final sorting result, so consistency is important. This order
    // needs to be independent of any state (e.g. the current load order) so
    // that sorting and applying the result doesn't then produce a different
    // result if you then sort again.
    plugins_sorting_data.sort_by(|a, b| a.name().cmp(b.name()));

    // Some parts of sorting are O(N^2) for N plugins, and master flags cause
    // O(M*N) edges to be added for M masters and N non-masters, which can be
    // two thirds of all edges added. The cost of each bidirectional search
    // scales with the number of edges, so reducing edges makes searches
    // faster.
    // Similarly, blueprint plugins load after all others.
    // As such, sort plugins using three separate graphs for masters,
    // non-masters and blueprint plugins. This means that any edges that go from a
    // non-master to a master are effectively ignored, so won't cause cyclic
    // interaction errors. Edges going the other way will also effectively be
    // ignored, but that shouldn't have a noticeable impact.
    let (masters, non_masters): (Vec<_>, Vec<_>) =
        plugins_sorting_data.into_iter().partition(|p| p.is_master);

    let (masters, blueprint_masters): (Vec<_>, Vec<_>) =
        masters.into_iter().partition(|p| !p.is_blueprint_master());

    validate_specific_and_hardcoded_edges(
        &masters,
        &blueprint_masters,
        &non_masters,
        early_loading_plugins,
    )?;

    let mut masters_load_order =
        sort_plugins_partition(masters, groups_graph, early_loading_plugins)?;

    let blueprint_masters_load_order =
        sort_plugins_partition(blueprint_masters, groups_graph, early_loading_plugins)?;

    let non_masters_load_order =
        sort_plugins_partition(non_masters, groups_graph, early_loading_plugins)?;

    masters_load_order.extend(non_masters_load_order);
    masters_load_order.extend(blueprint_masters_load_order);

    Ok(masters_load_order)
}

fn sort_plugins_partition(
    plugins_sorting_data: Vec<PluginSortingData>,
    groups_graph: &GroupsGraph,
    early_loading_plugins: &[String],
) -> Result<Vec<String>, SortingError> {
    let mut graph = PluginsGraph::new();

    for plugin in plugins_sorting_data {
        graph.add_node(plugin);
    }

    graph.add_specific_edges()?;
    graph.add_early_loading_plugin_edges(early_loading_plugins);

    // Check for cycles now because from this point on edges are only added if
    // they don't cause cycles, and adding overlap and tie-break edges is
    // relatively slow, so checking now provides quicker feedback if there is an
    // issue.
    graph.check_for_cycles()?;

    graph.add_group_edges(groups_graph)?;
    graph.add_overlap_edges()?;
    graph.add_tie_break_edges()?;

    // Check for cycles again, just in case there's a bug that lets some occur.
    // The check doesn't take a significant amount of time.
    graph.check_for_cycles()?;

    let sorted_nodes = graph.topological_sort()?;

    if let Some((first, second)) = graph.is_hamiltonian_path(&sorted_nodes) {
        log::error!(
            "The path is not unique. No edge exists between {} and {}",
            graph[first].name(),
            graph[second].name()
        );
    }

    let sorted_plugin_names = sorted_nodes
        .into_iter()
        .map(|i| graph[i].name().to_string())
        .collect();

    Ok(sorted_plugin_names)
}

fn path_to_string(graph: &InnerPluginsGraph, path: &[NodeIndex]) -> String {
    path.iter()
        .map(|i| graph[*i].name())
        .collect::<Vec<_>>()
        .join(", ")
}

#[derive(Debug)]
struct PathFinder<'a, 'b> {
    graph: &'a InnerPluginsGraph<'b>,
    cache: &'a mut HashMap<NodeIndex, HashSet<NodeIndex>>,
    from_node_index: NodeIndex,
    to_node_index: NodeIndex,
    forward_parents: HashMap<NodeIndex, NodeIndex>,
    reverse_children: HashMap<NodeIndex, NodeIndex>,
    intersection_node: Option<NodeIndex>,
}

impl<'a, 'b> PathFinder<'a, 'b> {
    fn new(
        graph: &'a InnerPluginsGraph<'b>,
        cache: &'a mut HashMap<NodeIndex, HashSet<NodeIndex>>,
        from_node_index: NodeIndex,
        to_node_index: NodeIndex,
    ) -> Self {
        Self {
            graph,
            cache,
            from_node_index,
            to_node_index,
            forward_parents: HashMap::new(),
            reverse_children: HashMap::new(),
            intersection_node: None,
        }
    }

    fn cache_path(&mut self, from: NodeIndex, to: NodeIndex) {
        self.cache.entry(from).or_default().insert(to);
    }

    fn path(&self) -> Result<Option<Vec<NodeIndex>>, PathfindingError> {
        match self.intersection_node {
            None => Ok(None),
            Some(intersection_node) => {
                let mut current_node = intersection_node;
                let mut path = vec![current_node];

                while current_node != self.from_node_index {
                    if let Some(next) = self.forward_parents.get(&current_node) {
                        path.push(*next);
                        current_node = *next;
                    } else {
                        log::error!(
                            "Could not find parent vertex of {}. Path so far is {}",
                            self.graph[current_node].name(),
                            path_to_string(self.graph, &path)
                        );
                        return Err(PathfindingError::PrecedingNodeNotFound(
                            self.graph[current_node].name().to_string(),
                        ));
                    }
                }

                // The path currently runs backwards, so reverse it.
                path.reverse();

                current_node = intersection_node;

                while current_node != self.to_node_index {
                    if let Some(next) = self.reverse_children.get(&current_node) {
                        path.push(*next);
                        current_node = *next;
                    } else {
                        log::error!(
                            "Could not find child vertex of {}. Path so far is {}",
                            self.graph[current_node].name(),
                            path_to_string(self.graph, &path)
                        );
                        return Err(PathfindingError::FollowingNodeNotFound(
                            self.graph[current_node].name().to_string(),
                        ));
                    }
                }

                Ok(Some(path))
            }
        }
    }
}

impl BidirBfsVisitor for PathFinder<'_, '_> {
    fn visit_forward_bfs_edge(&mut self, source: NodeIndex, target: NodeIndex) {
        self.cache_path(self.from_node_index, target);

        self.forward_parents.insert(target, source);
    }

    fn visit_reverse_bfs_edge(&mut self, source: NodeIndex, target: NodeIndex) {
        self.cache_path(source, self.to_node_index);

        self.reverse_children.insert(source, target);
    }

    fn visit_intersection_node(&mut self, node: NodeIndex) {
        self.intersection_node = Some(node)
    }
}

#[derive(Debug)]
struct PathCacher<'a> {
    cache: &'a mut HashMap<NodeIndex, HashSet<NodeIndex>>,
    from_node_index: NodeIndex,
    to_node_index: NodeIndex,
}

fn get_plugins_in_groups(graph: &InnerPluginsGraph) -> HashMap<String, Vec<NodeIndex>> {
    let mut plugins_in_groups: HashMap<String, Vec<NodeIndex>> = HashMap::new();

    for node in graph.node_indices() {
        let group_name = graph[node].group.clone();

        plugins_in_groups.entry(group_name).or_default().push(node);
    }

    plugins_in_groups
}

impl<'a> PathCacher<'a> {
    fn new(
        cache: &'a mut HashMap<NodeIndex, HashSet<NodeIndex>>,
        from_node_index: NodeIndex,
        to_node_index: NodeIndex,
    ) -> Self {
        Self {
            cache,
            from_node_index,
            to_node_index,
        }
    }

    fn cache_path(&mut self, from: NodeIndex, to: NodeIndex) {
        self.cache.entry(from).or_default().insert(to);
    }
}

impl BidirBfsVisitor for PathCacher<'_> {
    fn visit_forward_bfs_edge(&mut self, _: NodeIndex, target: NodeIndex) {
        self.cache_path(self.from_node_index, target);
    }

    fn visit_reverse_bfs_edge(&mut self, source: NodeIndex, _: NodeIndex) {
        self.cache_path(source, self.to_node_index);
    }

    fn visit_intersection_node(&mut self, _: NodeIndex) {}
}

// Use type aliases to make intent clearer without the complications of introducing newtypes.
type PluginNodeIndex = NodeIndex;
type GroupNodeIndex = NodeIndex;

struct GroupsPathVisitor<'a, 'b, 'c, 'd, 'e> {
    plugins_graph: &'a mut PluginsGraph<'b>,
    groups_graph: &'e GroupsGraph,
    groups_plugins: &'c HashMap<String, Vec<PluginNodeIndex>>,
    finished_group_vertices: &'d mut HashSet<GroupNodeIndex>,
    group_node_to_ignore_as_source: Option<GroupNodeIndex>,
    edge_stack: Vec<(EdgeReference<'e, EdgeType>, &'c [PluginNodeIndex])>,
    unfinishable_nodes: HashSet<GroupNodeIndex>,
}

impl<'a, 'b, 'c, 'd, 'e> GroupsPathVisitor<'a, 'b, 'c, 'd, 'e> {
    fn new(
        plugins_graph: &'a mut PluginsGraph<'b>,
        groups_graph: &'e GroupsGraph,
        groups_plugins: &'c HashMap<String, Vec<PluginNodeIndex>>,
        finished_group_vertices: &'d mut HashSet<GroupNodeIndex>,
        group_node_to_ignore_as_source: Option<GroupNodeIndex>,
    ) -> Self {
        Self {
            plugins_graph,
            groups_graph,
            groups_plugins,
            finished_group_vertices,
            group_node_to_ignore_as_source,
            edge_stack: Vec::new(),
            unfinishable_nodes: HashSet::new(),
        }
    }

    fn should_ignore_source_node(&self, node_index: GroupNodeIndex) -> bool {
        self.group_node_to_ignore_as_source == Some(node_index)
            || self.finished_group_vertices.contains(&node_index)
    }

    fn find_plugins_in_group(&self, node_index: GroupNodeIndex) -> &'c [PluginNodeIndex] {
        self.groups_plugins
            .get(&self.groups_graph[node_index])
            .map(|v| v.as_slice())
            .unwrap_or_default()
    }

    fn add_plugin_graph_edges(
        &mut self,
        edge_stack_index: usize,
        target_plugins: &[PluginNodeIndex],
    ) {
        let from_plugins = self.edge_stack[edge_stack_index].1;

        let path_involves_user_metadata = self.edge_stack[edge_stack_index..]
            .iter()
            .any(|p| *p.0.weight() == EdgeType::UserLoadAfter);

        for from_plugin in from_plugins {
            self.add_edges_from_plugin(*from_plugin, target_plugins, path_involves_user_metadata);
        }
    }

    fn add_edges_from_plugin(
        &mut self,
        from_plugin: PluginNodeIndex,
        to_plugins: &[PluginNodeIndex],
        path_involves_user_metadata: bool,
    ) {
        if to_plugins.is_empty() {
            return;
        }

        for to_plugin in to_plugins {
            if !self.plugins_graph.is_path_cached(from_plugin, *to_plugin) {
                if !self.plugins_graph.path_exists(*to_plugin, from_plugin) {
                    let involves_user_metadata = path_involves_user_metadata
                        || self.plugins_graph[from_plugin].group_is_user_metadata
                        || self.plugins_graph[*to_plugin].group_is_user_metadata;

                    let edge_type = if involves_user_metadata {
                        EdgeType::UserGroup
                    } else {
                        EdgeType::MasterlistGroup
                    };

                    self.plugins_graph
                        .add_edge(from_plugin, *to_plugin, edge_type);
                } else {
                    log::debug!(
                        "Skipping group edge from \"{}\" to \"{}\" as it would create a cycle.",
                        self.plugins_graph[from_plugin].name(),
                        self.plugins_graph[*to_plugin].name()
                    );
                }
            }
        }
    }
}

impl<'e> DfsVisitor<'e> for GroupsPathVisitor<'_, '_, '_, '_, 'e> {
    fn visit_tree_edge(&mut self, edge_ref: EdgeReference<'e, EdgeType>) {
        let source = edge_ref.source();
        let target = edge_ref.target();

        // Add the edge to the stack so that its providence can be taken into
        // account when adding edges from this source group and previous groups'
        // plugins.
        // Also record the plugins in the edge's source group, unless the source
        // group should be ignored (e.g. because the visitor has been configured
        // to ignore the default group's plugins as sources).
        let edge_plugins = if self.should_ignore_source_node(source) {
            &[]
        } else {
            self.find_plugins_in_group(source)
        };
        self.edge_stack.push((edge_ref, edge_plugins));

        // Find the plugins in the target group.
        let target_plugins = self.find_plugins_in_group(target);

        // Add edges going from all the plugins in the groups in the path being
        // currently walked, to the plugins in the current target group's plugins.
        for i in 0..self.edge_stack.len() {
            self.add_plugin_graph_edges(i, target_plugins);
        }
    }

    fn visit_forward_or_cross_edge(&mut self, edge_ref: EdgeReference<'e, EdgeType>) {
        // Mark the source vertex as unfinishable, because none of the plugins in
        // in the path so far can have edges added to plugins past the target
        // vertex.
        self.unfinishable_nodes.insert(edge_ref.source());
    }

    fn visit_back_edge(&mut self, _: EdgeReference<'e, EdgeType>) {}

    fn discover_node(&mut self, _: GroupNodeIndex) {}

    fn finish_node(&mut self, node_index: GroupNodeIndex) {
        // Now that this vertex's DFS-tree has been fully explored, mark it as
        // finished so that it won't have edges added from its plugins again in a
        // different DFS that uses the same finished vertices set.
        if self.group_node_to_ignore_as_source != Some(node_index)
            && !self.unfinishable_nodes.contains(&node_index)
        {
            self.finished_group_vertices.insert(node_index);
        }

        // Since this vertex has been fully explored, pop the edge stack to remove
        // the edge that has this vertex as its target.
        self.edge_stack.pop();
    }
}
