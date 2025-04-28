use std::rc::Rc;

use petgraph::{
    Graph,
    graph::{EdgeReference, NodeIndex},
    visit::EdgeRef,
};
use rustc_hash::{FxHashMap as HashMap, FxHashSet as HashSet};

use crate::{
    EdgeType, LogLevel, Plugin,
    logging::{self, is_log_enabled},
    metadata::{File, Group, PluginMetadata},
    plugin::error::PluginDataError,
    sorting::{
        error::{CyclicInteractionError, PathfindingError, SortingError, UndefinedGroupError},
        groups::{get_default_group_node, sorted_group_nodes},
    },
};

use super::{
    dfs::{BidirBfsVisitor, DfsVisitor, bidirectional_bfs, depth_first_search, find_cycle},
    groups::GroupsGraph,
    validate::{validate_plugin_groups, validate_specific_and_hardcoded_edges},
};

#[derive(Debug)]
pub struct PluginSortingData<'a, T: SortingPlugin> {
    plugin: &'a T,
    pub(super) is_master: bool,
    override_record_count: usize,

    load_order_index: usize,

    pub(super) group: Box<str>,
    group_is_user_metadata: bool,
    pub(crate) masterlist_load_after: Box<[String]>,
    pub(crate) user_load_after: Box<[String]>,
    pub(crate) masterlist_req: Box<[String]>,
    pub(crate) user_req: Box<[String]>,
}

impl<'a, T: SortingPlugin> PluginSortingData<'a, T> {
    pub fn new(
        plugin: &'a T,
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
                .into(),
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

    fn do_records_overlap(&self, other: &Self) -> Result<bool, PluginDataError> {
        self.plugin.do_records_overlap(other.plugin)
    }

    fn do_assets_overlap(&self, other: &Self) -> bool {
        self.plugin.do_assets_overlap(other.plugin)
    }
}

pub trait SortingPlugin {
    fn name(&self) -> &str;
    fn is_master(&self) -> bool;
    fn is_blueprint_plugin(&self) -> bool;
    fn masters(&self) -> Result<Vec<String>, PluginDataError>;
    fn override_record_count(&self) -> Result<usize, PluginDataError>;
    fn asset_count(&self) -> usize;
    fn do_records_overlap(&self, other: &Self) -> Result<bool, PluginDataError>;
    fn do_assets_overlap(&self, other: &Self) -> bool;
}

impl SortingPlugin for Plugin {
    fn name(&self) -> &str {
        self.name()
    }
    fn is_master(&self) -> bool {
        self.is_master()
    }

    fn is_blueprint_plugin(&self) -> bool {
        self.is_blueprint_plugin()
    }

    fn masters(&self) -> Result<Vec<String>, PluginDataError> {
        self.masters()
    }
    fn override_record_count(&self) -> Result<usize, PluginDataError> {
        self.override_record_count()
    }
    fn asset_count(&self) -> usize {
        self.asset_count()
    }

    fn do_records_overlap(&self, other: &Self) -> Result<bool, PluginDataError> {
        self.do_records_overlap(other)
    }
    fn do_assets_overlap(&self, other: &Self) -> bool {
        self.do_assets_overlap(other)
    }
}

fn to_filenames(files: &[File]) -> Box<[String]> {
    files.iter().map(|f| f.name().as_str().to_owned()).collect()
}

type InnerPluginsGraph<'a, T> = Graph<Rc<PluginSortingData<'a, T>>, EdgeType>;

#[derive(Debug)]
struct PluginsGraph<'a, T: SortingPlugin> {
    // Put the sorting data in Rc so that it can be held onto while mutating the graph.
    inner: InnerPluginsGraph<'a, T>,
    paths_cache: HashMap<NodeIndex, HashSet<NodeIndex>>,
}

impl<'a, T: SortingPlugin> PluginsGraph<'a, T> {
    fn new() -> Self {
        PluginsGraph::default()
    }

    fn add_node(&mut self, plugin: PluginSortingData<'a, T>) -> NodeIndex {
        self.inner.add_node(Rc::new(plugin))
    }

    fn add_edge(&mut self, from: NodeIndex, to: NodeIndex, edge_type: EdgeType) {
        if self.is_path_cached(from, to) {
            return;
        }

        logging::debug!(
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
        logging::trace!("Adding edges based on plugin data and non-group metadata...");

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
        logging::trace!(
            "Adding edges for implicitly active plugins and plugins with hardcoded positions..."
        );

        if early_loading_plugins.is_empty() {
            return;
        }

        let mut early_loader_indices = Vec::new();
        let mut other_plugin_indices = Vec::new();
        for node_index in self.node_indices() {
            let plugin = &self[node_index];
            if let Some(i) = early_loading_plugins
                .iter()
                .position(|e| unicase::eq(e.as_str(), plugin.name()))
            {
                early_loader_indices.push((i, node_index));
            } else {
                other_plugin_indices.push(node_index);
            }
        }

        early_loader_indices.sort_by_key(|e| e.0);

        for window in early_loader_indices.windows(2) {
            if let [(_, from_index), (_, to_index)] = *window {
                self.add_edge(from_index, to_index, EdgeType::Hardcoded);
            }
        }

        if let Some((_, from_index)) = early_loader_indices.last() {
            for to_index in other_plugin_indices {
                self.add_edge(*from_index, to_index, EdgeType::Hardcoded);
            }
        }
    }

    fn check_for_cycles(&mut self) -> Result<(), CyclicInteractionError> {
        if let Some(cycle) = find_cycle(&self.inner, |node| node.name().to_owned()) {
            Err(CyclicInteractionError::new(cycle))
        } else {
            Ok(())
        }
    }

    fn add_group_edges(&mut self, groups_graph: &GroupsGraph) -> Result<(), UndefinedGroupError> {
        logging::trace!("Adding edges based on plugin group memberships...");

        // First build a map from groups to the plugins in those groups.
        let plugins_in_groups = get_plugins_in_groups(&self.inner);

        // Get the default group's vertex because it's needed for the DFSes.
        let default_group_node = get_default_group_node(groups_graph)?;

        // Keep a record of which vertices have already been fully explored to avoid
        // adding edges from their plugins more than once.
        let mut finished_nodes = HashSet::default();
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

            depth_first_search(
                groups_graph,
                &mut HashMap::default(),
                group_node,
                &mut visitor,
            );
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
            &mut HashMap::default(),
            default_group_node,
            &mut visitor,
        );

        Ok(())
    }

    fn add_overlap_edges(&mut self) -> Result<(), SortingError> {
        logging::trace!("Adding edges for overlapping plugins...");

        let mut node_index_iter = self.node_indices();
        while let Some(node_index) = node_index_iter.next() {
            let plugin = Rc::clone(&self[node_index]);
            let plugin_asset_count = plugin.asset_count();

            if plugin.override_record_count == 0 && plugin_asset_count == 0 {
                logging::debug!(
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
                    }

                    outer_plugin_loads_first = plugin_asset_count > other_plugin_asset_count;
                    edge_type = EdgeType::AssetOverlap;
                } else {
                    // Records overlap and override different numbers of records.
                    // Load this plugin first if it overrides more records.
                    outer_plugin_loads_first =
                        plugin.override_record_count > other_plugin.override_record_count;
                    edge_type = EdgeType::RecordOverlap;
                }

                let (from_index, to_index) = if outer_plugin_loads_first {
                    (node_index, other_node_index)
                } else {
                    (other_node_index, node_index)
                };

                if !self.is_path_cached(from_index, to_index) {
                    if self.path_exists(to_index, from_index) {
                        logging::debug!(
                            "Skipping {} edge from \"{}\" to \"{}\" as it would create a cycle.",
                            edge_type,
                            self[from_index].name(),
                            self[to_index].name()
                        );
                    } else {
                        self.add_edge(from_index, to_index, edge_type);
                    }
                }
            }
        }

        Ok(())
    }

    fn add_tie_break_edges(&mut self) -> Result<(), PathfindingError> {
        logging::trace!("Adding edges to break ties between plugins...");

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
        let mut processed_nodes = HashSet::default();

        // First get the graph vertices and sort them into the current load order.
        let mut nodes: Vec<_> = self.node_indices().collect();
        nodes.sort_by_key(|a| self[*a].load_order_index);

        for window in nodes.windows(2) {
            let [current, next] = *window else {
                // This should never happen.
                logging::error!("Unexpectedly encountered a window length that was not 2");
                continue;
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

                        logging::debug!(
                            "The plugin \"{}\" loads at the end of the new load order so far.",
                            self[current].name()
                        );
                    } else if new_load_order.last() != Some(&current) {
                        logging::trace!(
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
                        if is_log_enabled(LogLevel::Debug) {
                            logging::debug!(
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
            logging::debug!(
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
            .iter()
            .skip(range_start)
            .rposition(|ni| !self.path_exists(node_index, *ni))
            .map(|p| range_start + p);

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
        let insert_position = previous_node_position.map_or(range_start, |i| i + 1);

        // Add an edge going from this vertex to the next one in the "new load
        // order" path, in case there isn't already one.
        if let Some(following_node_index) = new_load_order.get(insert_position) {
            self.add_edge(node_index, *following_node_index, EdgeType::TieBreak);
        }

        // Now update newLoadOrder with the vertex's new position.
        new_load_order.insert(insert_position, node_index);
        processed_nodes.insert(node_index);

        if is_log_enabled(LogLevel::Debug) {
            if let Some(next_node_index) = new_load_order.get(insert_position + 1) {
                logging::debug!(
                    "The plugin \"{}\" loads before \"{}\" in the new load order.",
                    self[node_index].name(),
                    self[*next_node_index].name()
                );
            } else {
                logging::debug!(
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
            .map_err(|e| SortingError::CycleInvolving(self[e.node_id()].name().to_owned()))
    }

    /// Returns the first pair of consecutive nodes that don't have an edge joining them.
    fn check_path_is_hamiltonian(&mut self, path: &[NodeIndex]) -> Option<(NodeIndex, NodeIndex)> {
        use std::ops::Not;

        logging::trace!("Checking uniqueness of path through plugin graph...");

        path.windows(2).find_map(|slice| match *slice {
            [a, b] => self.inner.contains_edge(a, b).not().then_some((a, b)),
            _ => None,
        })
    }

    fn cache_path(&mut self, from: NodeIndex, to: NodeIndex) {
        self.paths_cache.entry(from).or_default().insert(to);
    }

    fn is_path_cached(&self, from: NodeIndex, to: NodeIndex) -> bool {
        self.paths_cache.get(&from).is_some_and(|s| s.contains(&to))
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

// The derive macro for Default requires T: Default, but it's not actually necessary.
impl<T: SortingPlugin> std::default::Default for PluginsGraph<'_, T> {
    fn default() -> Self {
        Self {
            inner: Graph::default(),
            paths_cache: HashMap::default(),
        }
    }
}

impl<'a, T: SortingPlugin> std::ops::Index<NodeIndex> for PluginsGraph<'a, T> {
    type Output = Rc<PluginSortingData<'a, T>>;

    fn index(&self, index: NodeIndex) -> &Self::Output {
        &self.inner[index]
    }
}

pub fn sort_plugins<T: SortingPlugin>(
    mut plugins_sorting_data: Vec<PluginSortingData<T>>,
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

fn sort_plugins_partition<T: SortingPlugin>(
    plugins_sorting_data: Vec<PluginSortingData<T>>,
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

    if let Some((first, second)) = graph.check_path_is_hamiltonian(&sorted_nodes) {
        logging::error!(
            "The path is not unique. No edge exists between {} and {}",
            graph[first].name(),
            graph[second].name()
        );
    }

    let sorted_plugin_names = sorted_nodes
        .into_iter()
        .map(|i| graph[i].name().to_owned())
        .collect();

    Ok(sorted_plugin_names)
}

fn path_to_string<T: SortingPlugin>(graph: &InnerPluginsGraph<T>, path: &[NodeIndex]) -> String {
    path.iter()
        .map(|i| graph[*i].name())
        .collect::<Vec<_>>()
        .join(", ")
}

#[derive(Debug)]
struct PathFinder<'a, 'b, T: SortingPlugin> {
    graph: &'a InnerPluginsGraph<'b, T>,
    cache: &'a mut HashMap<NodeIndex, HashSet<NodeIndex>>,
    from_node_index: NodeIndex,
    to_node_index: NodeIndex,
    forward_parents: HashMap<NodeIndex, NodeIndex>,
    reverse_children: HashMap<NodeIndex, NodeIndex>,
    intersection_node: Option<NodeIndex>,
}

impl<'a, 'b, T: SortingPlugin> PathFinder<'a, 'b, T> {
    fn new(
        graph: &'a InnerPluginsGraph<'b, T>,
        cache: &'a mut HashMap<NodeIndex, HashSet<NodeIndex>>,
        from_node_index: NodeIndex,
        to_node_index: NodeIndex,
    ) -> Self {
        Self {
            graph,
            cache,
            from_node_index,
            to_node_index,
            forward_parents: HashMap::default(),
            reverse_children: HashMap::default(),
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
                        logging::error!(
                            "Could not find parent vertex of {}. Path so far is {}",
                            self.graph[current_node].name(),
                            path_to_string(self.graph, &path)
                        );
                        return Err(PathfindingError::PrecedingNodeNotFound(
                            self.graph[current_node].name().to_owned(),
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
                        logging::error!(
                            "Could not find child vertex of {}. Path so far is {}",
                            self.graph[current_node].name(),
                            path_to_string(self.graph, &path)
                        );
                        return Err(PathfindingError::FollowingNodeNotFound(
                            self.graph[current_node].name().to_owned(),
                        ));
                    }
                }

                Ok(Some(path))
            }
        }
    }
}

impl<T: SortingPlugin> BidirBfsVisitor for PathFinder<'_, '_, T> {
    fn visit_forward_bfs_edge(&mut self, source: NodeIndex, target: NodeIndex) {
        self.cache_path(self.from_node_index, target);

        self.forward_parents.insert(target, source);
    }

    fn visit_reverse_bfs_edge(&mut self, source: NodeIndex, target: NodeIndex) {
        self.cache_path(source, self.to_node_index);

        self.reverse_children.insert(source, target);
    }

    fn visit_intersection_node(&mut self, node: NodeIndex) {
        self.intersection_node = Some(node);
    }
}

#[derive(Debug)]
struct PathCacher<'a> {
    cache: &'a mut HashMap<NodeIndex, HashSet<NodeIndex>>,
    from_node_index: NodeIndex,
    to_node_index: NodeIndex,
}

fn get_plugins_in_groups<T: SortingPlugin>(
    graph: &InnerPluginsGraph<T>,
) -> HashMap<Box<str>, Vec<NodeIndex>> {
    let mut plugins_in_groups: HashMap<Box<str>, Vec<NodeIndex>> = HashMap::default();

    for node in graph.node_indices() {
        let group_name = graph[node].group.clone();

        plugins_in_groups.entry(group_name).or_default().push(node);
    }

    if is_log_enabled(LogLevel::Debug) {
        logging::debug!("Found the following plugins in groups:");
        let mut keys = plugins_in_groups.keys().collect::<Vec<_>>();
        keys.sort();
        for key in keys {
            let plugin_names: Vec<_> = plugins_in_groups
                .get(key)
                .into_iter()
                .flatten()
                .map(|i| format!("\"{}\"", graph[*i].name()))
                .collect();
            logging::debug!("\t{}: {}", key, plugin_names.join(", "));
        }
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

struct GroupsPathVisitor<'a, 'b, 'c, 'd, 'e, T: SortingPlugin> {
    plugins_graph: &'a mut PluginsGraph<'b, T>,
    groups_graph: &'e GroupsGraph,
    groups_plugins: &'c HashMap<Box<str>, Vec<PluginNodeIndex>>,
    finished_group_vertices: &'d mut HashSet<GroupNodeIndex>,
    group_node_to_ignore_as_source: Option<GroupNodeIndex>,
    edge_stack: Vec<(EdgeReference<'e, EdgeType>, &'c [PluginNodeIndex])>,
    unfinishable_nodes: HashSet<GroupNodeIndex>,
}

impl<'a, 'b, 'c, 'd, 'e, T: SortingPlugin> GroupsPathVisitor<'a, 'b, 'c, 'd, 'e, T> {
    fn new(
        plugins_graph: &'a mut PluginsGraph<'b, T>,
        groups_graph: &'e GroupsGraph,
        groups_plugins: &'c HashMap<Box<str>, Vec<PluginNodeIndex>>,
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
            unfinishable_nodes: HashSet::default(),
        }
    }

    fn should_ignore_source_node(&self, node_index: GroupNodeIndex) -> bool {
        self.group_node_to_ignore_as_source == Some(node_index)
            || self.finished_group_vertices.contains(&node_index)
    }

    fn find_plugins_in_group(&self, node_index: GroupNodeIndex) -> &'c [PluginNodeIndex] {
        self.groups_plugins
            .get(self.groups_graph[node_index].as_ref())
            .map(Vec::as_slice)
            .unwrap_or_default()
    }

    fn add_plugin_graph_edges(
        &mut self,
        edge_stack_index: usize,
        target_plugins: &[PluginNodeIndex],
    ) {
        use std::fmt::Write;

        let Some([from_edge, edges @ ..]) = self.edge_stack.get(edge_stack_index..) else {
            if is_log_enabled(LogLevel::Error) {
                logging::error!(
                    "Unexpected invalid edge stack index {} for edge stack [{}]",
                    edge_stack_index,
                    self.edge_stack
                        .iter()
                        .map(|e| e.0.weight())
                        .fold(String::new(), |mut a, b| if a.is_empty() {
                            b.to_string()
                        } else {
                            let _e = write!(a, ", {b}");
                            a
                        })
                );
            }
            return;
        };

        let path_involves_user_metadata = std::iter::once(from_edge)
            .chain(edges.iter())
            .any(|p| *p.0.weight() == EdgeType::UserLoadAfter);

        for from_plugin in from_edge.1 {
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
                let involves_user_metadata = path_involves_user_metadata
                    || self.plugins_graph[from_plugin].group_is_user_metadata
                    || self.plugins_graph[*to_plugin].group_is_user_metadata;

                let edge_type = if involves_user_metadata {
                    EdgeType::UserGroup
                } else {
                    EdgeType::MasterlistGroup
                };

                if self.plugins_graph.path_exists(*to_plugin, from_plugin) {
                    logging::debug!(
                        "Skipping a \"{}\" edge from \"{}\" to \"{}\" as it would create a cycle.",
                        edge_type,
                        self.plugins_graph[from_plugin].name(),
                        self.plugins_graph[*to_plugin].name()
                    );
                } else {
                    self.plugins_graph
                        .add_edge(from_plugin, *to_plugin, edge_type);
                }
            }
        }
    }
}

impl<'e, T: SortingPlugin> DfsVisitor<'e> for GroupsPathVisitor<'_, '_, '_, '_, 'e, T> {
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
        // Mark the source vertex and all edges in the current stack as
        // unfinishable, because none of the plugins in the path so far can have
        // edges added to plugins past the target vertex.

        logging::debug!(
            "Found groups graph forward or cross \"{}\" edge going from \"{}\" to \"{}\"",
            edge_ref.weight(),
            self.groups_graph[edge_ref.source()],
            self.groups_graph[edge_ref.target()]
        );

        let iter = self
            .edge_stack
            .iter()
            .map(|e| e.0.source())
            .chain(std::iter::once(edge_ref.source()));

        for source in iter {
            let inserted = self.unfinishable_nodes.insert(source);

            if inserted {
                logging::debug!("Treating \"{}\" as unfinishable", self.groups_graph[source]);
            }
        }
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
            let inserted = self.finished_group_vertices.insert(node_index);
            if inserted {
                logging::debug!(
                    "Recorded groups graph vertex \"{}\" as finished",
                    self.groups_graph[node_index]
                );
            }
        }

        // Since this vertex has been fully explored, pop the edge stack to remove
        // the edge that has this vertex as its target.
        self.edge_stack.pop();
    }
}

#[cfg(test)]
mod tests {
    #![allow(clippy::many_single_char_names)]
    use super::*;

    use crate::sorting::{groups::build_groups_graph, test::TestPlugin};

    const PLUGIN_A: &str = "A.esp";
    const PLUGIN_B: &str = "B.esp";

    struct Fixture {
        groups_graph: GroupsGraph,
        plugins: HashMap<String, (TestPlugin, usize)>,
    }

    impl Fixture {
        fn with_plugins(plugin_names: &[&str]) -> Self {
            let masterlist = &[
                Group::new("A".into()),
                Group::new("B".into()).with_after_groups(vec!["A".into()]),
                Group::new("C".into()),
                Group::new("default".into()).with_after_groups(vec!["C".into()]),
                Group::new("E".into()).with_after_groups(vec!["default".into()]),
                Group::new("F".into()).with_after_groups(vec!["E".into()]),
            ];
            let userlist = &[Group::new("C".into()).with_after_groups(vec!["B".into()])];
            let groups_graph = build_groups_graph(masterlist, userlist).unwrap();

            Self {
                groups_graph,
                plugins: plugin_names
                    .iter()
                    .enumerate()
                    .map(|(i, n)| ((*n).to_owned(), (TestPlugin::new(n), i)))
                    .collect(),
            }
        }

        fn get_plugin(&self, name: &str) -> &(TestPlugin, usize) {
            &self.plugins[name]
        }

        fn get_plugin_mut(&mut self, name: &str) -> &mut TestPlugin {
            &mut self.plugins.get_mut(name).unwrap().0
        }

        fn sorting_data<'a>(&'a self, name: &str) -> PluginSortingData<'a, TestPlugin> {
            let (plugin, index) = self.get_plugin(name);

            PluginSortingData::new(plugin, None, None, *index).unwrap()
        }

        fn group_sorting_data<'a>(
            &'a self,
            name: &str,
            group_name: &str,
        ) -> PluginSortingData<'a, TestPlugin> {
            let (plugin, index) = self.get_plugin(name);

            let mut metadata = PluginMetadata::new(name).unwrap();
            metadata.set_group(group_name.into());

            PluginSortingData::new(plugin, Some(&metadata), None, *index).unwrap()
        }

        fn user_group_sorting_data<'a>(
            &'a self,
            name: &str,
            group_name: &str,
        ) -> PluginSortingData<'a, TestPlugin> {
            let (plugin, index) = self.get_plugin(name);

            let mut metadata = PluginMetadata::new(name).unwrap();
            metadata.set_group(group_name.into());

            PluginSortingData::new(plugin, None, Some(&metadata), *index).unwrap()
        }
    }

    mod plugin_sorting_data {
        use crate::tests::BLANK_ESM;

        use super::*;

        #[test]
        fn is_blueprint_master_should_be_true_if_a_plugin_is_a_master_and_a_blueprint_plugin() {
            let mut master = TestPlugin::new(BLANK_ESM);
            master.is_master = true;
            let mut blueprint_plugin = TestPlugin::new(BLANK_ESM);
            blueprint_plugin.is_blueprint_plugin = true;
            let mut blueprint_master = TestPlugin::new(BLANK_ESM);
            blueprint_master.is_master = true;
            blueprint_master.is_blueprint_plugin = true;

            let plugin = PluginSortingData::new(&master, None, None, 0).unwrap();
            assert!(!plugin.is_blueprint_master());

            let plugin = PluginSortingData::new(&blueprint_plugin, None, None, 0).unwrap();
            assert!(!plugin.is_blueprint_master());

            let plugin = PluginSortingData::new(&blueprint_master, None, None, 0).unwrap();
            assert!(plugin.is_blueprint_master());
        }
    }

    mod plugins_graph {
        use super::*;

        use crate::Vertex;

        const PLUGIN_C: &str = "C.esp";
        const PLUGIN_D: &str = "D.esp";
        const PLUGIN_E: &str = "E.esp";

        fn edge_type(
            graph: &PluginsGraph<'_, TestPlugin>,
            from: NodeIndex,
            to: NodeIndex,
        ) -> EdgeType {
            *graph
                .inner
                .edge_weight(graph.inner.find_edge(from, to).unwrap())
                .unwrap()
        }

        mod check_for_cycles {
            use super::*;

            #[test]
            fn should_succeed_if_there_is_no_cycle() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let mut graph = PluginsGraph::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_edge(a, b, EdgeType::Master);

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_error_if_there_is_a_cycle() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C]);

                let mut graph = PluginsGraph::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_edge(a, b, EdgeType::Master);
                graph.add_edge(b, a, EdgeType::Master);

                let cycle = graph.check_for_cycles().unwrap_err().into_cycle();

                assert_eq!(
                    &[
                        Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::Master),
                        Vertex::new(PLUGIN_B.into()).with_out_edge_type(EdgeType::Master),
                    ],
                    cycle.as_slice()
                );
            }

            #[test]
            fn should_only_give_plugins_that_are_part_of_the_cycle() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C]);

                let mut graph = PluginsGraph::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));
                let c = graph.add_node(fixture.sorting_data(PLUGIN_C));

                graph.add_edge(a, b, EdgeType::Master);
                graph.add_edge(b, c, EdgeType::Master);
                graph.add_edge(b, a, EdgeType::MasterFlag);

                let cycle = graph.check_for_cycles().unwrap_err().into_cycle();

                assert_eq!(
                    &[
                        Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::Master),
                        Vertex::new(PLUGIN_B.into()).with_out_edge_type(EdgeType::MasterFlag),
                    ],
                    cycle.as_slice()
                );
            }
        }

        #[test]
        fn topological_sort_should_return_empty_list_if_there_are_no_plugins() {
            let graph = PluginsGraph::<TestPlugin>::new();
            let sorted = graph.topological_sort().unwrap();

            assert!(sorted.is_empty());
        }

        mod add_early_loading_plugin_edges {
            use super::*;

            #[test]
            fn should_add_no_edges_if_there_are_no_early_loading_plugins() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));
                let c = graph.add_node(fixture.sorting_data(PLUGIN_C));

                graph.add_early_loading_plugin_edges(&[]);

                assert!(!graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(a, c));
                assert!(!graph.inner.contains_edge(b, a));
                assert!(!graph.inner.contains_edge(b, c));
                assert!(!graph.inner.contains_edge(c, a));
                assert!(!graph.inner.contains_edge(c, b));
            }

            #[test]
            fn should_add_edges_between_consecutive_early_loaders_skipping_missing_plugins() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_C, PLUGIN_D]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let c = graph.add_node(fixture.sorting_data(PLUGIN_C));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));

                graph.add_early_loading_plugin_edges(&[
                    PLUGIN_A.into(),
                    PLUGIN_B.into(),
                    PLUGIN_C.into(),
                    PLUGIN_D.into(),
                ]);

                assert!(graph.inner.contains_edge(a, c));
                assert!(graph.inner.contains_edge(c, d));
                assert!(!graph.inner.contains_edge(a, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edges_from_only_the_last_installed_early_loader_to_all_non_early_loader_plugins()
             {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_D, PLUGIN_E]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));
                let e = graph.add_node(fixture.sorting_data(PLUGIN_E));

                graph.add_early_loading_plugin_edges(&[
                    PLUGIN_A.into(),
                    PLUGIN_B.into(),
                    PLUGIN_C.into(),
                ]);

                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(b, d));
                assert!(graph.inner.contains_edge(b, e));
                assert!(!graph.inner.contains_edge(a, d));
                assert!(!graph.inner.contains_edge(a, e));

                assert!(graph.check_for_cycles().is_ok());
            }
        }

        mod add_group_edges {
            use super::*;

            const PLUGIN_A1: &str = "A1.esp";
            const PLUGIN_A2: &str = "A2.esp";
            const PLUGIN_B1: &str = "B1.esp";
            const PLUGIN_B2: &str = "B2.esp";
            const PLUGIN_C1: &str = "C1.esp";
            const PLUGIN_C2: &str = "C2.esp";
            const PLUGIN_D1: &str = "D1.esp";
            const PLUGIN_D2: &str = "D2.esp";
            const PLUGIN_D3: &str = "D3.esp";
            const PLUGIN_F: &str = "F.esp";

            #[test]
            fn should_add_user_group_edge_if_source_plugin_is_in_group_due_to_user_metadata() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.user_group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert_eq!(EdgeType::UserGroup, edge_type(&graph, a, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_user_group_edge_if_target_plugin_is_in_group_due_to_user_metadata() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.user_group_sorting_data(PLUGIN_B, "B"));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert_eq!(EdgeType::UserGroup, edge_type(&graph, a, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_user_group_edge_if_group_path_starts_with_user_metadata() {
                let fixture = Fixture::with_plugins(&[PLUGIN_B, PLUGIN_D]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert_eq!(EdgeType::UserGroup, edge_type(&graph, b, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_user_group_edge_if_group_path_ends_with_user_metadata() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_C]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert_eq!(EdgeType::UserGroup, edge_type(&graph, a, c));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_user_group_edge_if_group_path_involves_user_metadata() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_D]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert_eq!(EdgeType::UserGroup, edge_type(&graph, a, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_masterlist_group_edge_if_no_user_metadata_is_involved() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert_eq!(EdgeType::MasterlistGroup, edge_type(&graph, a, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edges_between_plugins_in_indirectly_connected_groups_when_an_intermediate_plugin_edge_is_skipped()
             {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A1, PLUGIN_A2, PLUGIN_B1, PLUGIN_B2, PLUGIN_C1, PLUGIN_C2,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a1 = graph.add_node(fixture.group_sorting_data(PLUGIN_A1, "A"));
                let a2 = graph.add_node(fixture.group_sorting_data(PLUGIN_A2, "A"));
                let b1 = graph.add_node(fixture.group_sorting_data(PLUGIN_B1, "B"));
                let b2 = graph.add_node(fixture.group_sorting_data(PLUGIN_B2, "B"));
                let c1 = graph.add_node(fixture.group_sorting_data(PLUGIN_C1, "C"));
                let c2 = graph.add_node(fixture.group_sorting_data(PLUGIN_C2, "C"));

                graph.add_edge(b1, a1, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be A2.esp -> B1.esp -> A1.esp -> B2.esp -> C1.esp
                //                                                -> C2.esp
                assert!(graph.inner.contains_edge(b1, a1));
                assert!(graph.inner.contains_edge(a1, b2));
                assert!(graph.inner.contains_edge(a2, b1));
                assert!(graph.inner.contains_edge(a2, b2));
                assert!(graph.inner.contains_edge(b1, c1));
                assert!(graph.inner.contains_edge(b1, c2));
                assert!(graph.inner.contains_edge(b2, c1));
                assert!(graph.inner.contains_edge(b2, c2));
                assert!(graph.inner.contains_edge(a1, c1));
                assert!(graph.inner.contains_edge(a1, c2));
                assert!(!graph.inner.contains_edge(c1, c2));
                assert!(!graph.inner.contains_edge(c2, c1));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edges_across_empty_groups() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_C]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be A.esp -> C.esp
                assert!(graph.inner.contains_edge(a, c));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edges_across_the_non_empty_default_group() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_D, PLUGIN_E]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be A.esp -> D.esp -> E.esp
                //                 ---------->
                assert!(graph.inner.contains_edge(a, d));
                assert!(graph.inner.contains_edge(d, e));
                assert!(graph.inner.contains_edge(a, e));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_skip_an_edge_that_would_cause_a_cycle() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_C]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));

                graph.add_edge(c, a, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert!(graph.inner.contains_edge(c, a));
                assert!(!graph.inner.contains_edge(a, c));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_skip_an_edge_that_would_cause_a_cycle_involving_other_non_default_groups() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));

                graph.add_edge(c, a, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                assert!(graph.inner.contains_edge(c, a));
                assert!(graph.inner.contains_edge(a, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_skip_only_edges_to_the_target_group_plugins_that_would_cause_a_cycle() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_C1, PLUGIN_C2]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let c1 = graph.add_node(fixture.group_sorting_data(PLUGIN_C1, "C"));
                let c2 = graph.add_node(fixture.group_sorting_data(PLUGIN_C2, "C"));

                graph.add_edge(c1, a, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be C1.esp -> A.esp -> C2.esp
                assert!(graph.inner.contains_edge(c1, a));
                assert!(graph.inner.contains_edge(a, c2));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_skip_only_edges_from_ancestors_to_the_target_group_plugins_that_would_cause_a_cycle()
             {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_B, PLUGIN_C, PLUGIN_D1, PLUGIN_D2, PLUGIN_D3]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d1 = graph.add_node(fixture.sorting_data(PLUGIN_D1));
                let d2 = graph.add_node(fixture.sorting_data(PLUGIN_D2));
                let d3 = graph.add_node(fixture.sorting_data(PLUGIN_D3));

                graph.add_edge(d1, b, EdgeType::Master);
                graph.add_edge(d2, b, EdgeType::Master);
                graph.add_edge(c, b, EdgeType::Master);
                graph.add_edge(c, d2, EdgeType::Master);
                graph.add_edge(c, d3, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be: C.esp -> D2.esp -> B.esp -> D3.esp
                //                  -> D1.esp ->
                //                  -------------------->
                //                  ----------->
                assert!(graph.inner.contains_edge(d1, b));
                assert!(graph.inner.contains_edge(d2, b));
                assert!(graph.inner.contains_edge(c, b));
                assert!(graph.inner.contains_edge(c, d2));
                assert!(graph.inner.contains_edge(c, d3));
                assert!(graph.inner.contains_edge(b, d3));
                assert!(graph.inner.contains_edge(c, d1));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_plugin_edges_across_a_successor_if_at_least_one_edge_to_the_successor_group_was_skipped_with_successive_depths()
             {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A1, PLUGIN_A2, PLUGIN_B1, PLUGIN_B2, PLUGIN_C1, PLUGIN_C2,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a1 = graph.add_node(fixture.group_sorting_data(PLUGIN_A1, "A"));
                let a2 = graph.add_node(fixture.group_sorting_data(PLUGIN_A2, "A"));
                let b1 = graph.add_node(fixture.group_sorting_data(PLUGIN_B1, "B"));
                let b2 = graph.add_node(fixture.group_sorting_data(PLUGIN_B2, "B"));
                let c1 = graph.add_node(fixture.group_sorting_data(PLUGIN_C1, "C"));
                let c2 = graph.add_node(fixture.group_sorting_data(PLUGIN_C2, "C"));

                graph.add_edge(b1, a1, EdgeType::Master);
                graph.add_edge(c1, b2, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be A2.esp -> B1.esp -> A1.esp -> C1.esp -> B2.esp -> C2.esp
                assert!(graph.inner.contains_edge(b1, a1));
                assert!(graph.inner.contains_edge(c1, b2));
                assert!(graph.inner.contains_edge(a1, b2));
                assert!(graph.inner.contains_edge(a1, c1));
                assert!(graph.inner.contains_edge(a1, c2));
                assert!(graph.inner.contains_edge(a2, b1));
                assert!(graph.inner.contains_edge(a2, b2));
                assert!(graph.inner.contains_edge(b1, c1));
                assert!(graph.inner.contains_edge(b1, c2));
                assert!(graph.inner.contains_edge(b2, c2));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_plugin_edges_across_a_successor_if_at_least_one_edge_to_the_successor_group_was_skipped_with_successive_depths_and_a_different_order()
             {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A1, PLUGIN_A2, PLUGIN_B1, PLUGIN_B2, PLUGIN_C1, PLUGIN_C2,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a1 = graph.add_node(fixture.group_sorting_data(PLUGIN_A1, "A"));
                let a2 = graph.add_node(fixture.group_sorting_data(PLUGIN_A2, "A"));
                let b1 = graph.add_node(fixture.group_sorting_data(PLUGIN_B1, "B"));
                let b2 = graph.add_node(fixture.group_sorting_data(PLUGIN_B2, "B"));
                let c1 = graph.add_node(fixture.group_sorting_data(PLUGIN_C1, "C"));
                let c2 = graph.add_node(fixture.group_sorting_data(PLUGIN_C2, "C"));

                graph.add_edge(b1, a1, EdgeType::Master);
                graph.add_edge(c1, b1, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be A2.esp -> C1.esp -> B1.esp -> A1.esp -> B2.esp -> C2.esp
                assert!(graph.inner.contains_edge(b1, a1));
                assert!(graph.inner.contains_edge(c1, b1));
                assert!(graph.inner.contains_edge(a1, b2));
                assert!(graph.inner.contains_edge(a1, c2));
                assert!(graph.inner.contains_edge(a2, b1));
                assert!(graph.inner.contains_edge(a2, b2));
                assert!(graph.inner.contains_edge(a2, c1));
                assert!(graph.inner.contains_edge(b1, c2));
                assert!(graph.inner.contains_edge(b2, c2));
                assert!(!graph.inner.contains_edge(b2, c1));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edge_from_ancestor_to_successor_if_none_of_a_groups_plugins_can() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B1, PLUGIN_B2, PLUGIN_C]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b1 = graph.add_node(fixture.group_sorting_data(PLUGIN_B1, "B"));
                let b2 = graph.add_node(fixture.group_sorting_data(PLUGIN_B2, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));

                graph.add_edge(c, b1, EdgeType::Master);
                graph.add_edge(c, b2, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be A.esp -> C1.esp -> B1.esp
                //                           -> B2.esp
                assert!(graph.inner.contains_edge(a, b1));
                assert!(graph.inner.contains_edge(a, b2));
                assert!(graph.inner.contains_edge(c, b1));
                assert!(graph.inner.contains_edge(c, b2));
                assert!(graph.inner.contains_edge(a, c));
                assert!(!graph.inner.contains_edge(b1, b2));
                assert!(!graph.inner.contains_edge(b2, b1));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edge_from_ancestor_to_successor_if_none_of_a_groups_plugins_can_with_edges_across_the_skipped_group()
             {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A1, PLUGIN_A2, PLUGIN_B1, PLUGIN_B2, PLUGIN_C1, PLUGIN_C2, PLUGIN_D1,
                    PLUGIN_D2,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a1 = graph.add_node(fixture.group_sorting_data(PLUGIN_A1, "A"));
                let a2 = graph.add_node(fixture.group_sorting_data(PLUGIN_A2, "A"));
                let b1 = graph.add_node(fixture.group_sorting_data(PLUGIN_B1, "B"));
                let b2 = graph.add_node(fixture.group_sorting_data(PLUGIN_B2, "B"));
                let c1 = graph.add_node(fixture.group_sorting_data(PLUGIN_C1, "C"));
                let c2 = graph.add_node(fixture.group_sorting_data(PLUGIN_C2, "C"));
                let d1 = graph.add_node(fixture.sorting_data(PLUGIN_D1));
                let d2 = graph.add_node(fixture.sorting_data(PLUGIN_D2));

                graph.add_edge(b1, a1, EdgeType::Master);
                graph.add_edge(c1, b1, EdgeType::Master);
                graph.add_edge(d1, c1, EdgeType::Master);
                graph.add_edge(d2, c1, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be:
                // A2.esp -> D1.esp -> C1.esp -> B1.esp -> A1.esp -> B2.esp -> C2.esp
                //        -> D2.esp ->
                assert!(graph.inner.contains_edge(b1, a1));
                assert!(graph.inner.contains_edge(c1, b1));
                assert!(graph.inner.contains_edge(d1, c1));
                assert!(graph.inner.contains_edge(d2, c1));
                assert!(graph.inner.contains_edge(a1, b2));
                assert!(graph.inner.contains_edge(a2, b1));
                assert!(graph.inner.contains_edge(a2, b2));
                assert!(graph.inner.contains_edge(a1, c2));
                assert!(graph.inner.contains_edge(b1, c2));
                assert!(graph.inner.contains_edge(a2, c1));
                assert!(graph.inner.contains_edge(a2, d1));
                assert!(graph.inner.contains_edge(a2, d2));
                assert!(!graph.inner.contains_edge(b2, c1));
                assert!(!graph.inner.contains_edge(d1, d2));
                assert!(!graph.inner.contains_edge(d2, d1));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_deprioritise_edges_from_default_group_plugins_with_default_last() {
                let fixture = Fixture::with_plugins(&[PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));

                graph.add_edge(d, b, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be D.esp -> B.esp -> C.esp
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(d, b));
                assert!(!graph.inner.contains_edge(c, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_deprioritise_edges_from_default_group_plugins_with_default_first() {
                let fixture = Fixture::with_plugins(&[PLUGIN_D, PLUGIN_E, PLUGIN_F]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_F, "F"));

                graph.add_edge(f, d, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be E.esp -> F.esp -> D.esp
                assert!(graph.inner.contains_edge(e, f));
                assert!(graph.inner.contains_edge(f, d));
                assert!(!graph.inner.contains_edge(d, e));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_deprioritise_edges_from_default_group_plugins_across_skipped_intermediate_groups()
             {
                let fixture = Fixture::with_plugins(&[PLUGIN_D, PLUGIN_E, PLUGIN_F]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_F, "F"));

                graph.add_edge(e, d, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be E.esp -> D.esp -> F.esp
                assert!(graph.inner.contains_edge(e, d));
                assert!(graph.inner.contains_edge(d, f));
                assert!(!graph.inner.contains_edge(f, e));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_deprioritise_edges_from_default_group_plugins_with_d1_first_d2_last() {
                let fixture = Fixture::with_plugins(&[PLUGIN_D1, PLUGIN_D2, PLUGIN_E, PLUGIN_F]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let d1 = graph.add_node(fixture.sorting_data(PLUGIN_D1));
                let d2 = graph.add_node(fixture.sorting_data(PLUGIN_D2));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_F, "F"));

                graph.add_edge(f, d2, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be D1.esp -> E.esp -> F.esp -> D2.esp
                assert!(graph.inner.contains_edge(e, f));
                assert!(graph.inner.contains_edge(f, d2));
                assert!(graph.inner.contains_edge(d1, e));
                assert!(!graph.inner.contains_edge(d2, d1));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_deprioritise_edges_from_default_group_plugins_with_no_ideal_result() {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E, PLUGIN_F]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_F, "F"));

                graph.add_edge(d, b, EdgeType::Master);
                graph.add_edge(f, d, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // No ideal result, expected is F.esp -> D.esp -> B.esp -> C.esp -> E.esp
                assert!(graph.inner.contains_edge(f, d));
                assert!(graph.inner.contains_edge(d, b));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(c, e));
                assert!(!graph.inner.contains_edge(e, f));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_deprioritise_edges_from_default_group_plugins_with_default_in_middle_and_d_bookends()
             {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_B, PLUGIN_C, PLUGIN_D1, PLUGIN_D2, PLUGIN_E, PLUGIN_F,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d1 = graph.add_node(fixture.sorting_data(PLUGIN_D1));
                let d2 = graph.add_node(fixture.sorting_data(PLUGIN_D2));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_F, "F"));

                graph.add_edge(d2, b, EdgeType::Master);
                graph.add_edge(f, d1, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be D2.esp -> B.esp -> C.esp -> E.esp -> F.esp -> D1.esp
                assert!(graph.inner.contains_edge(d2, b));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(c, e));
                assert!(graph.inner.contains_edge(e, f));
                assert!(graph.inner.contains_edge(f, d1));
                assert!(!graph.inner.contains_edge(d1, d2));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_deprioritise_edges_from_default_group_plugins_with_default_in_middle_and_d_throughout()
             {
                const PLUGIN_D4: &str = "D4.esp";
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_B, PLUGIN_C, PLUGIN_D1, PLUGIN_D2, PLUGIN_D3, PLUGIN_D4, PLUGIN_E,
                    PLUGIN_F,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d1 = graph.add_node(fixture.sorting_data(PLUGIN_D1));
                let d2 = graph.add_node(fixture.sorting_data(PLUGIN_D2));
                let d3 = graph.add_node(fixture.sorting_data(PLUGIN_D3));
                let d4 = graph.add_node(fixture.sorting_data(PLUGIN_D4));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_F, "F"));

                graph.add_edge(d2, b, EdgeType::Master);
                graph.add_edge(d4, c, EdgeType::Master);
                graph.add_edge(f, d1, EdgeType::Master);

                graph.add_group_edges(&fixture.groups_graph).unwrap();

                // Should be:
                // D2.esp -> B.esp -> D4.esp -> C.esp -> D3.esp -> E.esp -> F.esp -> D1.esp
                assert!(graph.inner.contains_edge(d2, b));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(c, d3));
                assert!(graph.inner.contains_edge(c, e));
                assert!(graph.inner.contains_edge(d3, e));
                assert!(graph.inner.contains_edge(e, f));
                assert!(graph.inner.contains_edge(f, d1));
                assert!(graph.inner.contains_edge(d4, c));
                assert!(graph.inner.contains_edge(b, d4));
                assert!(!graph.inner.contains_edge(d1, d2));
                assert!(!graph.inner.contains_edge(d1, d3));
                assert!(!graph.inner.contains_edge(d1, d4));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_handle_asymmetric_branches_in_the_groups_graph() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::new("D".into()).with_after_groups(vec!["A".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp -> C.esp
                //                 -> D.esp
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(a, d));
                assert!(!graph.inner.contains_edge(d, b));
                assert!(!graph.inner.contains_edge(d, c));
                assert!(!graph.inner.contains_edge(b, d));
                assert!(!graph.inner.contains_edge(c, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_handle_asymmetric_branches_in_the_groups_graph_that_merge() {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::new("D".into()).with_after_groups(vec!["A".into()]),
                        Group::new("E".into()).with_after_groups(vec!["C".into(), "D".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp -> C.esp -> E.esp
                //                 -> D.esp ---------->
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(c, e));
                assert!(graph.inner.contains_edge(a, d));
                assert!(graph.inner.contains_edge(d, e));
                assert!(!graph.inner.contains_edge(d, b));
                assert!(!graph.inner.contains_edge(d, c));
                assert!(!graph.inner.contains_edge(b, d));
                assert!(!graph.inner.contains_edge(c, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_handle_branches_in_the_groups_graph_that_form_a_diamond_pattern() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["A".into()]),
                        Group::new("D".into()).with_after_groups(vec!["B".into(), "C".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp -> D.esp
                //                 -> C.esp ->
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(b, d));
                assert!(graph.inner.contains_edge(a, c));
                assert!(graph.inner.contains_edge(c, d));
                assert!(!graph.inner.contains_edge(b, c));
                assert!(!graph.inner.contains_edge(c, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edges_across_the_merge_point_of_branches_in_the_groups_graph() {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["A".into()]),
                        Group::new("D".into()).with_after_groups(vec!["B".into(), "C".into()]),
                        Group::new("E".into()).with_after_groups(vec!["D".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));

                graph.add_edge(d, c, EdgeType::Master);

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp -> D.esp -> C.esp -> E.esp
                assert!(graph.inner.contains_edge(d, c));
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(b, d));
                assert!(graph.inner.contains_edge(d, e));
                assert!(graph.inner.contains_edge(a, c));
                assert!(graph.inner.contains_edge(c, e));
                assert!(!graph.inner.contains_edge(b, c));
                assert!(!graph.inner.contains_edge(c, b));
                assert!(!graph.inner.contains_edge(c, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_handle_a_groups_graph_with_multiple_successive_branches() {
                const PLUGIN_G: &str = "G.esp";
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E, PLUGIN_F, PLUGIN_G,
                ]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["A".into()]),
                        Group::new("D".into()).with_after_groups(vec!["B".into(), "C".into()]),
                        Group::new("E".into()).with_after_groups(vec!["D".into()]),
                        Group::new("F".into()).with_after_groups(vec!["D".into()]),
                        Group::new("G".into()).with_after_groups(vec!["E".into(), "F".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "F"));
                let g = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "G"));

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be:
                // A.esp -> B.esp -> D.esp -> E.esp -> G.esp
                //       -> C.esp ->       -> F.esp ->
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(a, c));
                assert!(graph.inner.contains_edge(b, d));
                assert!(graph.inner.contains_edge(c, d));
                assert!(graph.inner.contains_edge(d, e));
                assert!(graph.inner.contains_edge(d, f));
                assert!(graph.inner.contains_edge(e, g));
                assert!(graph.inner.contains_edge(f, g));
                assert!(!graph.inner.contains_edge(b, c));
                assert!(!graph.inner.contains_edge(c, b));
                assert!(!graph.inner.contains_edge(e, f));
                assert!(!graph.inner.contains_edge(f, e));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_find_all_groups_in_all_paths_between_two_groups_when_ignoring_a_plugin() {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::new("D".into()).with_after_groups(vec!["C".into()]),
                        Group::default().with_after_groups(vec!["B".into(), "D".into()]),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));
                let e = graph.add_node(fixture.sorting_data(PLUGIN_E));

                graph.add_edge(e, a, EdgeType::Master);

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be:
                // A.esp -> B.esp -> D.esp -> E.esp -> G.esp
                //       -> C.esp ->       -> F.esp ->
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(c, d));

                assert!(!graph.inner.contains_edge(a, e));
                assert!(!graph.inner.contains_edge(b, e));
                assert!(!graph.inner.contains_edge(c, e));
                assert!(!graph.inner.contains_edge(d, e));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_handle_isolated_groups() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp
                //           C.esp
                assert!(graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(a, c));
                assert!(!graph.inner.contains_edge(c, a));
                assert!(!graph.inner.contains_edge(b, c));
                assert!(!graph.inner.contains_edge(c, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_handle_disconnected_group_graphs() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()),
                        Group::new("D".into()).with_after_groups(vec!["C".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp
                //           C.esp -> D.esp
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(c, d));
                assert!(!graph.inner.contains_edge(a, c));
                assert!(!graph.inner.contains_edge(a, d));
                assert!(!graph.inner.contains_edge(b, c));
                assert!(!graph.inner.contains_edge(b, d));
                assert!(!graph.inner.contains_edge(c, a));
                assert!(!graph.inner.contains_edge(c, b));
                assert!(!graph.inner.contains_edge(d, a));
                assert!(!graph.inner.contains_edge(d, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_add_edges_across_the_merge_point_of_two_root_node_paths() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()),
                        Group::new("C".into()).with_after_groups(vec!["A".into(), "B".into()]),
                        Group::new("D".into()).with_after_groups(vec!["C".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));

                graph.add_edge(c, b, EdgeType::Master);

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> C.esp -> D.esp
                //           B.esp ---------->
                assert!(graph.inner.contains_edge(c, b));
                assert!(graph.inner.contains_edge(a, c));
                assert!(graph.inner.contains_edge(c, d));
                assert!(graph.inner.contains_edge(b, d));
                assert!(!graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(b, a));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_not_depend_on_group_definition_order_if_there_is_a_single_linear_path() {
                let fixture = Fixture::with_plugins(&[PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let masterlists = &[
                    [
                        Group::new("B".into()),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::default().with_after_groups(vec!["C".into()]),
                    ],
                    [
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::new("B".into()),
                        Group::default().with_after_groups(vec!["C".into()]),
                    ],
                ];

                for masterlist in masterlists {
                    let groups_graph = build_groups_graph(masterlist, &[]).unwrap();

                    let mut graph = PluginsGraph::<TestPlugin>::new();
                    let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                    let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                    let d = graph.add_node(fixture.sorting_data(PLUGIN_D));

                    graph.add_edge(d, b, EdgeType::Master);

                    graph.add_group_edges(&groups_graph).unwrap();

                    // Should be D.esp -> B.esp -> C.esp
                    assert!(graph.inner.contains_edge(b, c));
                    assert!(graph.inner.contains_edge(d, b));
                    assert!(!graph.inner.contains_edge(c, d));

                    assert!(graph.check_for_cycles().is_ok());
                }
            }

            #[test]
            fn should_not_depend_on_group_definition_order_if_there_are_multiple_roots() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let masterlists = &[
                    [
                        Group::new("A".into()),
                        Group::new("B".into()),
                        Group::new("C".into()).with_after_groups(vec!["A".into(), "B".into()]),
                        Group::new("D".into()).with_after_groups(vec!["C".into()]),
                        Group::default(),
                    ],
                    [
                        Group::new("B".into()),
                        Group::new("A".into()),
                        Group::new("C".into()).with_after_groups(vec!["A".into(), "B".into()]),
                        Group::new("D".into()).with_after_groups(vec!["C".into()]),
                        Group::default(),
                    ],
                ];

                for masterlist in masterlists {
                    let groups_graph = build_groups_graph(masterlist, &[]).unwrap();

                    let mut graph = PluginsGraph::<TestPlugin>::new();
                    let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                    let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                    let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                    let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));

                    graph.add_edge(d, a, EdgeType::Master);

                    graph.add_group_edges(&groups_graph).unwrap();

                    // Should be B.esp -> D.esp -> A.esp -> C.esp
                    //           B.esp ------------------->
                    assert!(graph.inner.contains_edge(d, a));
                    assert!(graph.inner.contains_edge(a, c));
                    assert!(graph.inner.contains_edge(b, c));
                    assert!(graph.inner.contains_edge(b, d));
                    assert!(!graph.inner.contains_edge(a, b));
                    assert!(!graph.inner.contains_edge(b, a));
                    assert!(!graph.inner.contains_edge(c, d));

                    assert!(graph.check_for_cycles().is_ok());
                }
            }

            #[test]
            fn should_not_depend_on_branching_group_definition_order() {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E]);

                let masterlists = &[
                    [
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["A".into()]),
                        Group::new("D".into()).with_after_groups(vec!["B".into(), "C".into()]),
                        Group::new("E".into()).with_after_groups(vec!["D".into()]),
                        Group::default(),
                    ],
                    [
                        Group::new("A".into()),
                        Group::new("C".into()).with_after_groups(vec!["A".into()]),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("D".into()).with_after_groups(vec!["B".into(), "C".into()]),
                        Group::new("E".into()).with_after_groups(vec!["D".into()]),
                        Group::default(),
                    ],
                ];

                for masterlist in masterlists {
                    let groups_graph = build_groups_graph(masterlist, &[]).unwrap();

                    let mut graph = PluginsGraph::<TestPlugin>::new();
                    let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                    let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                    let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                    let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));
                    let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));

                    graph.add_edge(e, c, EdgeType::Master);

                    graph.add_group_edges(&groups_graph).unwrap();

                    // Should be A.esp -> B.esp -> D.esp -> E.esp -> C.esp
                    assert!(graph.inner.contains_edge(a, b));
                    assert!(graph.inner.contains_edge(a, c));
                    assert!(graph.inner.contains_edge(a, d));
                    assert!(graph.inner.contains_edge(a, e));
                    assert!(graph.inner.contains_edge(b, d));
                    assert!(graph.inner.contains_edge(b, e));
                    assert!(graph.inner.contains_edge(d, e));
                    assert!(graph.inner.contains_edge(e, c));

                    assert!(!graph.inner.contains_edge(b, c));
                    assert!(!graph.inner.contains_edge(c, b));
                    assert!(!graph.inner.contains_edge(c, d));
                    assert!(!graph.inner.contains_edge(c, e));
                    assert!(!graph.inner.contains_edge(d, c));

                    assert!(graph.check_for_cycles().is_ok());
                }
            }

            #[test]
            fn should_not_depend_on_plugin_graph_node_order() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A1, PLUGIN_A2, PLUGIN_B, PLUGIN_C]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let a1 = (PLUGIN_A1, "A");
                let a2 = (PLUGIN_A2, "A");
                let b = (PLUGIN_B, "B");
                let c = (PLUGIN_C, "C");

                let variations = &[
                    [a1, a2, b, c],
                    [a1, a2, c, b],
                    [a1, b, c, a2],
                    [a1, b, a2, c],
                    [a1, c, a2, b],
                    [a1, c, b, a2],
                    [a2, a1, b, c],
                    [a2, a1, c, b],
                    [a2, b, c, a1],
                    [a2, b, a1, c],
                    [a2, c, a1, b],
                    [a2, c, b, a1],
                    [b, a2, a1, c],
                    [b, a2, c, a1],
                    [b, a1, c, a2],
                    [b, a1, a2, c],
                    [b, c, a2, a1],
                    [b, c, a1, a2],
                    [c, a2, b, a1],
                    [c, a2, a1, b],
                    [c, b, a1, a2],
                    [c, b, a2, a1],
                    [c, a1, a2, b],
                    [c, a1, b, a2],
                ];

                for plugins in variations {
                    let mut graph = PluginsGraph::<TestPlugin>::new();

                    for plugin in plugins {
                        graph.add_node(fixture.group_sorting_data(plugin.0, plugin.1));
                    }

                    let a1 = graph.node_index_by_name(PLUGIN_A1).unwrap();
                    let a2 = graph.node_index_by_name(PLUGIN_A2).unwrap();
                    let b = graph.node_index_by_name(PLUGIN_B).unwrap();
                    let c = graph.node_index_by_name(PLUGIN_C).unwrap();

                    graph.add_edge(c, a1, EdgeType::Master);

                    graph.add_group_edges(&groups_graph).unwrap();

                    // Should be A2.esp -> C.esp -> A1.esp -> B.esp
                    //           A2.esp -------------------->
                    assert!(graph.inner.contains_edge(c, a1));
                    assert!(graph.inner.contains_edge(a1, b));
                    assert!(graph.inner.contains_edge(a2, b));
                    assert!(graph.inner.contains_edge(a2, c));
                    assert!(!graph.inner.contains_edge(a1, a2));
                    assert!(!graph.inner.contains_edge(a2, a1));
                    assert!(!graph.inner.contains_edge(b, c));

                    assert!(graph.check_for_cycles().is_ok());
                }
            }

            #[test]
            fn should_start_searching_from_root_groups_before_going_in_lexicographical_order() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("D".into()),
                        Group::new("A".into()).with_after_groups(vec!["D".into()]),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));

                graph.add_edge(c, d, EdgeType::Master);

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be C.esp -> D.esp -> A.esp -> B.esp
                // Processing groups lexicographically would give:
                // A.esp -> B.esp -> C.esp -> D.esp
                assert!(graph.inner.contains_edge(c, d));
                assert!(graph.inner.contains_edge(d, a));
                assert!(graph.inner.contains_edge(d, b));
                assert!(graph.inner.contains_edge(a, b));

                assert!(!graph.inner.contains_edge(a, c));
                assert!(!graph.inner.contains_edge(a, d));
                assert!(!graph.inner.contains_edge(b, a));
                assert!(!graph.inner.contains_edge(b, c));
                assert!(!graph.inner.contains_edge(b, d));
                assert!(!graph.inner.contains_edge(d, c));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_start_searching_from_the_root_group_with_the_longest_path() {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E, PLUGIN_F,
                ]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("D".into()),
                        Group::new("B".into()).with_after_groups(vec!["D".into()]),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::new("A".into()),
                        Group::new("E".into()).with_after_groups(vec!["C".into(), "A".into()]),
                        Group::new("F".into()).with_after_groups(vec!["E".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));
                let f = graph.add_node(fixture.group_sorting_data(PLUGIN_F, "F"));

                graph.add_edge(f, b, EdgeType::Master);

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be          D.esp -> B.esp -> C.esp -> E.esp
                //  A.esp -> F.esp ----------> B.esp
                //  A.esp -------------------------------------> E.esp
                assert!(graph.inner.contains_edge(d, b));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(c, e));
                assert!(graph.inner.contains_edge(a, f));
                assert!(graph.inner.contains_edge(a, e));
                assert!(graph.inner.contains_edge(f, b));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn does_not_start_searching_with_the_longest_path() {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["A".into()]),
                        Group::new("D".into()).with_after_groups(vec!["C".into()]),
                        Group::new("E".into()).with_after_groups(vec!["B".into(), "D".into()]),
                        Group::default(),
                    ],
                    &[],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));
                let e = graph.add_node(fixture.group_sorting_data(PLUGIN_E, "E"));

                graph.add_edge(e, c, EdgeType::Master);

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp -> E.esp -> C.esp -> D.esp
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(b, e));
                assert!(graph.inner.contains_edge(e, c));
                assert!(graph.inner.contains_edge(c, d));

                assert!(graph.check_for_cycles().is_ok());
            }

            #[test]
            fn should_mark_nodes_as_unfinishable_if_a_node_in_their_subtree_is_unfinishable() {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A, PLUGIN_B, PLUGIN_B1, PLUGIN_B2, PLUGIN_C, PLUGIN_D,
                ]);

                let groups_graph = build_groups_graph(
                    &[
                        Group::new("A".into()),
                        Group::new("B".into()).with_after_groups(vec!["A".into()]),
                        Group::new("C".into()).with_after_groups(vec!["B".into()]),
                        Group::new("D".into()).with_after_groups(vec!["C".into()]),
                        Group::default(),
                    ],
                    &[
                        Group::new("BU1".into()).with_after_groups(vec!["B".into()]),
                        Group::new("BU2".into()).with_after_groups(vec!["BU1".into()]),
                        Group::new("C".into()).with_after_groups(vec!["BU2".into()]),
                    ],
                )
                .unwrap();

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.group_sorting_data(PLUGIN_A, "A"));
                let b = graph.add_node(fixture.group_sorting_data(PLUGIN_B, "B"));
                let b1 = graph.add_node(fixture.group_sorting_data(PLUGIN_B1, "BU1"));
                let b2 = graph.add_node(fixture.group_sorting_data(PLUGIN_B2, "BU2"));
                let c = graph.add_node(fixture.group_sorting_data(PLUGIN_C, "C"));
                let d = graph.add_node(fixture.group_sorting_data(PLUGIN_D, "D"));

                graph.add_group_edges(&groups_graph).unwrap();

                // Should be A.esp -> B.esp -----------------------> C.esp -> D.esp
                //                          -> BU1.esp -> BU2.esp ->
                assert!(graph.inner.contains_edge(a, b));
                assert!(graph.inner.contains_edge(a, c));
                assert!(graph.inner.contains_edge(a, d));
                assert!(graph.inner.contains_edge(b, c));
                assert!(graph.inner.contains_edge(b, d));
                assert!(graph.inner.contains_edge(b, b1));
                assert!(graph.inner.contains_edge(b, b2));
                assert!(graph.inner.contains_edge(b1, b2));
                assert!(graph.inner.contains_edge(b1, c));
                assert!(graph.inner.contains_edge(b1, d));
                assert!(graph.inner.contains_edge(b2, c));
                assert!(graph.inner.contains_edge(b2, c));
                assert!(graph.inner.contains_edge(c, d));

                assert!(graph.check_for_cycles().is_ok());
            }
        }

        mod add_overlap_edges {
            use super::*;

            #[test]
            fn should_not_add_edges_between_non_overlapping_plugins() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert!(!graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_not_add_edges_between_overlapping_plugins_with_equal_override_counts() {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.override_record_count = 1;
                a.add_overlapping_records(PLUGIN_B);

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.override_record_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert!(!graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_add_edge_between_overlapping_plugins_with_unequal_override_counts() {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.override_record_count = 2;
                a.add_overlapping_records(PLUGIN_B);

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.override_record_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert_eq!(EdgeType::RecordOverlap, edge_type(&graph, a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_not_add_edge_between_non_overlapping_plugins_with_unequal_override_counts() {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.override_record_count = 2;

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.override_record_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert!(!graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_not_add_edge_between_plugins_with_asset_overlap_and_equal_asset_counts() {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.asset_count = 1;
                a.add_overlapping_assets(PLUGIN_B);

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.asset_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert!(!graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_not_add_edge_between_plugins_with_no_asset_overlap_and_unequal_asset_counts()
            {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.asset_count = 2;

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.asset_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert!(!graph.inner.contains_edge(a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_add_edge_between_plugins_with_asset_overlap_and_unequal_asset_counts() {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.asset_count = 2;
                a.add_overlapping_assets(PLUGIN_B);

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.asset_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert_eq!(EdgeType::AssetOverlap, edge_type(&graph, a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_add_edge_between_overlapping_plugins_with_asset_overlap_and_equal_override_count_and_unequal_asset_counts()
             {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.asset_count = 2;
                a.add_overlapping_records(PLUGIN_B);
                a.add_overlapping_assets(PLUGIN_B);

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.asset_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert_eq!(EdgeType::AssetOverlap, edge_type(&graph, a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_add_edge_between_plugins_with_asset_overlap_and_unequal_override_count_and_unequal_asset_counts()
             {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.override_record_count = 1;
                a.asset_count = 2;
                a.add_overlapping_assets(PLUGIN_B);

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.override_record_count = 2;
                b.asset_count = 1;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert_eq!(EdgeType::AssetOverlap, edge_type(&graph, a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }

            #[test]
            fn should_choose_record_overlap_over_asset_overlap() {
                let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let a = fixture.get_plugin_mut(PLUGIN_A);
                a.override_record_count = 2;
                a.asset_count = 1;
                a.add_overlapping_records(PLUGIN_B);
                a.add_overlapping_assets(PLUGIN_B);

                let b = fixture.get_plugin_mut(PLUGIN_B);
                b.override_record_count = 1;
                b.asset_count = 2;

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));

                graph.add_overlap_edges().unwrap();

                assert_eq!(EdgeType::RecordOverlap, edge_type(&graph, a, b));
                assert!(!graph.inner.contains_edge(b, a));
            }
        }

        mod add_tie_break_edges {
            use super::*;

            const PLUGIN_F: &str = "F.esp";
            const PLUGIN_G: &str = "G.esp";
            const PLUGIN_H: &str = "H.esp";
            const PLUGIN_I: &str = "I.esp";
            const PLUGIN_J: &str = "J.esp";

            #[test]
            fn should_not_error_on_a_graph_with_one_node() {
                let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                graph.add_node(fixture.sorting_data(PLUGIN_A));

                assert!(graph.add_tie_break_edges().is_ok());
            }

            #[test]
            fn should_result_in_a_sort_order_equal_to_vertex_creation_order_if_there_are_no_other_edges()
             {
                let fixture =
                    Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                graph.add_node(fixture.sorting_data(PLUGIN_A));
                graph.add_node(fixture.sorting_data(PLUGIN_B));
                graph.add_node(fixture.sorting_data(PLUGIN_C));
                graph.add_node(fixture.sorting_data(PLUGIN_D));
                graph.add_node(fixture.sorting_data(PLUGIN_E));

                graph.add_tie_break_edges().unwrap();

                let sorted = graph.topological_sort().unwrap();

                assert!(graph.check_path_is_hamiltonian(&sorted).is_none());

                let sorted_plugin_names: Vec<_> = sorted
                    .into_iter()
                    .map(|i| graph[i].name().to_owned())
                    .collect();

                assert_eq!(
                    &[PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E],
                    sorted_plugin_names.as_slice()
                );
            }

            #[test]
            fn should_pin_paths_that_prevent_the_vertex_creation_order_from_being_used() {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E, PLUGIN_F, PLUGIN_G, PLUGIN_H,
                    PLUGIN_I, PLUGIN_J,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                graph.add_node(fixture.sorting_data(PLUGIN_A));
                graph.add_node(fixture.sorting_data(PLUGIN_B));
                graph.add_node(fixture.sorting_data(PLUGIN_C));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));
                let e = graph.add_node(fixture.sorting_data(PLUGIN_E));
                let f = graph.add_node(fixture.sorting_data(PLUGIN_F));
                let g = graph.add_node(fixture.sorting_data(PLUGIN_G));
                let h = graph.add_node(fixture.sorting_data(PLUGIN_H));
                let i = graph.add_node(fixture.sorting_data(PLUGIN_I));
                graph.add_node(fixture.sorting_data(PLUGIN_J));

                // Add a path g -> h -> i -> f
                graph.add_edge(g, h, EdgeType::RecordOverlap);
                graph.add_edge(h, i, EdgeType::RecordOverlap);
                graph.add_edge(i, f, EdgeType::RecordOverlap);

                // Also add g -> d and i -> e
                graph.add_edge(g, d, EdgeType::RecordOverlap);
                graph.add_edge(i, e, EdgeType::RecordOverlap);

                graph.add_tie_break_edges().unwrap();

                let sorted = graph.topological_sort().unwrap();

                assert!(graph.check_path_is_hamiltonian(&sorted).is_none());

                let sorted_plugin_names: Vec<_> = sorted
                    .into_iter()
                    .map(|i| graph[i].name().to_owned())
                    .collect();

                assert_eq!(
                    &[
                        PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_G, PLUGIN_D, PLUGIN_H, PLUGIN_I,
                        PLUGIN_E, PLUGIN_F, PLUGIN_J
                    ],
                    sorted_plugin_names.as_slice()
                );
            }

            #[test]
            fn should_prefix_path_to_new_load_order_if_the_first_pair_of_nodes_cannot_be_used_in_creation_order()
             {
                let fixture = Fixture::with_plugins(&[
                    PLUGIN_A, PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_E, PLUGIN_F, PLUGIN_G, PLUGIN_H,
                    PLUGIN_I, PLUGIN_J,
                ]);

                let mut graph = PluginsGraph::<TestPlugin>::new();
                let a = graph.add_node(fixture.sorting_data(PLUGIN_A));
                let b = graph.add_node(fixture.sorting_data(PLUGIN_B));
                let c = graph.add_node(fixture.sorting_data(PLUGIN_C));
                let d = graph.add_node(fixture.sorting_data(PLUGIN_D));
                graph.add_node(fixture.sorting_data(PLUGIN_E));
                graph.add_node(fixture.sorting_data(PLUGIN_F));
                graph.add_node(fixture.sorting_data(PLUGIN_G));
                graph.add_node(fixture.sorting_data(PLUGIN_H));
                graph.add_node(fixture.sorting_data(PLUGIN_I));
                graph.add_node(fixture.sorting_data(PLUGIN_J));

                // Add a path b -> c -> d -> a
                graph.add_edge(b, c, EdgeType::RecordOverlap);
                graph.add_edge(c, d, EdgeType::RecordOverlap);
                graph.add_edge(d, a, EdgeType::RecordOverlap);

                graph.add_tie_break_edges().unwrap();

                let sorted = graph.topological_sort().unwrap();

                assert!(graph.check_path_is_hamiltonian(&sorted).is_none());

                let sorted_plugin_names: Vec<_> = sorted
                    .into_iter()
                    .map(|i| graph[i].name().to_owned())
                    .collect();

                assert_eq!(
                    &[
                        PLUGIN_B, PLUGIN_C, PLUGIN_D, PLUGIN_A, PLUGIN_E, PLUGIN_F, PLUGIN_G,
                        PLUGIN_H, PLUGIN_I, PLUGIN_J
                    ],
                    sorted_plugin_names.as_slice()
                );
            }
        }
    }

    mod sort_plugins {
        use crate::{Vertex, sorting::error::PluginGraphValidationError};

        use super::*;

        #[test]
        fn should_not_change_the_result_if_given_its_own_output() {
            let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let expected = &[PLUGIN_A, PLUGIN_B];

            let sorted = sort_plugins(
                vec![
                    fixture.sorting_data(PLUGIN_B),
                    fixture.sorting_data(PLUGIN_A),
                ],
                &fixture.groups_graph,
                &[],
            )
            .unwrap();

            assert_eq!(expected, sorted.as_slice());

            let sorted = sort_plugins(
                vec![
                    fixture.sorting_data(PLUGIN_A),
                    fixture.sorting_data(PLUGIN_B),
                ],
                &fixture.groups_graph,
                &[],
            )
            .unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_use_group_metadata_when_deciding_relative_plugin_positions() {
            let fixture = Fixture::with_plugins(&[PLUGIN_B, PLUGIN_A]);

            let data = vec![
                fixture.group_sorting_data(PLUGIN_A, "A"),
                fixture.group_sorting_data(PLUGIN_B, "B"),
            ];

            let expected = &[PLUGIN_A, PLUGIN_B];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_use_load_after_metadata_when_deciding_relative_plugin_positions() {
            let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_load_after = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            let expected = &[PLUGIN_B, PLUGIN_A];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_use_requirement_metadata_when_deciding_relative_plugin_positions() {
            let fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_req = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            let expected = &[PLUGIN_B, PLUGIN_A];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_use_early_loader_positions_when_deciding_relative_plugin_positions() {
            let fixture = Fixture::with_plugins(&[PLUGIN_B, PLUGIN_A]);

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            let expected = &[PLUGIN_A, PLUGIN_B];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[PLUGIN_A.into()]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_error_if_a_plugin_has_a_group_that_does_not_exist() {
            let fixture = Fixture::with_plugins(&[PLUGIN_A]);

            let data = vec![fixture.group_sorting_data(PLUGIN_A, "missing")];

            assert!(sort_plugins(data, &fixture.groups_graph, &[]).is_err());
        }

        #[test]
        fn should_error_if_a_cyclic_interaction_is_encountered() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).add_master(PLUGIN_B);
            fixture.get_plugin_mut(PLUGIN_B).add_master(PLUGIN_A);

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::CycleFound(e)) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::Master),
                            Vertex::new(PLUGIN_B.into()).with_out_edge_type(EdgeType::Master),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_master_edge_would_contradict_master_flags() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let a = fixture.get_plugin_mut(PLUGIN_A);
            a.is_master = true;
            a.add_master(PLUGIN_B);

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into()).with_out_edge_type(EdgeType::Master),
                            Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::MasterFlag),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_masterlist_load_after_contradicts_master_flags() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_load_after = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::MasterlistLoadAfter),
                            Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::MasterFlag),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_user_load_after_contradicts_master_flags() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.user_load_after = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::UserLoadAfter),
                            Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::MasterFlag),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_masterlist_requirement_contradicts_master_flags() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_req = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::MasterlistRequirement),
                            Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::MasterFlag),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_user_requirement_contradicts_master_flags() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.user_req = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::UserRequirement),
                            Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::MasterFlag),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_an_early_loader_contradicts_master_flags() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            match sort_plugins(data, &fixture.groups_graph, &[PLUGIN_B.into()]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into()).with_out_edge_type(EdgeType::Hardcoded),
                            Vertex::new(PLUGIN_A.into()).with_out_edge_type(EdgeType::MasterFlag),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_not_error_if_a_master_edge_would_put_a_blueprint_master_before_a_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let a = fixture.get_plugin_mut(PLUGIN_A);
            a.is_master = true;
            a.is_blueprint_plugin = true;

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.add_master(PLUGIN_A);

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            let expected = &[PLUGIN_B, PLUGIN_A];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_not_error_if_a_master_edge_would_put_a_blueprint_master_before_a_non_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let a = fixture.get_plugin_mut(PLUGIN_A);
            a.is_master = true;
            a.is_blueprint_plugin = true;

            fixture.get_plugin_mut(PLUGIN_B).add_master(PLUGIN_A);

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            let expected = &[PLUGIN_B, PLUGIN_A];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_error_if_a_masterlist_load_after_would_put_a_blueprint_master_before_a_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_load_after = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::MasterlistLoadAfter),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_masterlist_load_after_would_put_a_blueprint_master_before_a_non_master()
         {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_load_after = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::MasterlistLoadAfter),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_user_load_after_would_put_a_blueprint_master_before_a_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.user_load_after = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::UserLoadAfter),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_user_load_after_would_put_a_blueprint_master_before_a_non_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.user_load_after = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::UserLoadAfter),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_masterlist_requirement_would_put_a_blueprint_master_before_a_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_req = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::MasterlistRequirement),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_masterlist_requirement_would_put_a_blueprint_master_before_a_non_master()
         {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.masterlist_req = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::MasterlistRequirement),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_user_requirement_would_put_a_blueprint_master_before_a_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.user_req = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::UserRequirement),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_error_if_a_user_requirement_would_put_a_blueprint_master_before_a_non_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let mut a = fixture.sorting_data(PLUGIN_A);
            a.user_req = Box::new([PLUGIN_B.into()]);

            let data = vec![a, fixture.sorting_data(PLUGIN_B)];

            match sort_plugins(data, &fixture.groups_graph, &[]) {
                Err(SortingError::ValidationError(PluginGraphValidationError::CycleFound(e))) => {
                    assert_eq!(
                        &[
                            Vertex::new(PLUGIN_B.into())
                                .with_out_edge_type(EdgeType::UserRequirement),
                            Vertex::new(PLUGIN_A.into())
                                .with_out_edge_type(EdgeType::BlueprintMaster),
                        ],
                        e.into_cycle().as_slice()
                    );
                }
                _ => panic!("Expected to find a cycle"),
            }
        }

        #[test]
        fn should_not_error_if_an_early_loader_would_put_a_blueprint_master_before_a_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            fixture.get_plugin_mut(PLUGIN_A).is_master = true;

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            let expected = &[PLUGIN_A, PLUGIN_B];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[PLUGIN_B.into()]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }

        #[test]
        fn should_not_error_if_an_early_loader_would_put_a_blueprint_master_before_a_non_master() {
            let mut fixture = Fixture::with_plugins(&[PLUGIN_A, PLUGIN_B]);

            let b = fixture.get_plugin_mut(PLUGIN_B);
            b.is_master = true;
            b.is_blueprint_plugin = true;

            let data = vec![
                fixture.sorting_data(PLUGIN_A),
                fixture.sorting_data(PLUGIN_B),
            ];

            let expected = &[PLUGIN_A, PLUGIN_B];

            let sorted = sort_plugins(data, &fixture.groups_graph, &[PLUGIN_B.into()]).unwrap();

            assert_eq!(expected, sorted.as_slice());
        }
    }
}
