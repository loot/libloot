use std::{cmp::Reverse, collections::HashMap};

use petgraph::{Graph, algo::bellman_ford, graph::NodeIndex};

use crate::{
    EdgeType, Vertex, logging,
    metadata::Group,
    sorting::{
        dfs::find_cycle,
        error::{
            BuildGroupsGraphError, CyclicInteractionError, PathfindingError, UndefinedGroupError,
        },
    },
};

use super::{
    dfs::{DfsVisitor, depth_first_search},
    error::GroupsPathError,
};

pub type GroupsGraph = Graph<String, EdgeType>;

pub fn build_groups_graph(
    masterlist_groups: &[Group],
    userlist_groups: &[Group],
) -> Result<GroupsGraph, BuildGroupsGraphError> {
    let masterlist_groups = sorted_by_name(masterlist_groups);
    let userlist_groups = sorted_by_name(userlist_groups);

    let mut graph = GroupsGraph::new();
    let mut group_nodes: HashMap<&str, NodeIndex> = HashMap::new();

    logging::trace!("Adding masterlist groups to groups graph...");
    add_groups(
        &mut graph,
        &mut group_nodes,
        &masterlist_groups,
        EdgeType::MasterlistLoadAfter,
    )?;

    logging::trace!("Adding user groups to groups graph...");
    add_groups(
        &mut graph,
        &mut group_nodes,
        &userlist_groups,
        EdgeType::UserLoadAfter,
    )?;

    if let Some(cycle) = find_cycle(&graph, |node| node.clone()) {
        Err(CyclicInteractionError::new(cycle).into())
    } else {
        Ok(graph)
    }
}

fn sorted_by_name(groups: &[Group]) -> Vec<&Group> {
    let mut groups: Vec<_> = groups.iter().collect();
    groups.sort_by_key(|a| a.name());

    groups
}

fn add_groups<'a>(
    graph: &mut GroupsGraph,
    group_nodes: &mut HashMap<&'a str, NodeIndex>,
    groups: &[&'a Group],
    edge_type: EdgeType,
) -> Result<(), UndefinedGroupError> {
    for group in groups {
        let key = group.name();
        if !group_nodes.contains_key(key) {
            let node_index = graph.add_node(group.name().to_string());
            group_nodes.insert(key, node_index);
        }
    }

    for group in groups {
        if log::log_enabled!(log::Level::Trace) {
            logging::trace!(
                "Group \"{}\" directly loads after groups \"{}\"",
                group.name(),
                group.after_groups().join(", ")
            );
        }

        let node_index = group_nodes
            .get(group.name())
            .expect("Group node should have just been added");

        for other_group_name in sorted_clone(group.after_groups()) {
            if let Some(other_index) = group_nodes.get(other_group_name) {
                graph.update_edge(*other_index, *node_index, edge_type);
            } else {
                return Err(UndefinedGroupError::new(other_group_name.to_string()));
            }
        }
    }

    Ok(())
}

fn sorted_clone(strings: &[String]) -> Vec<&str> {
    let mut strings: Vec<_> = strings.iter().map(|s| s.as_str()).collect();
    strings.sort();

    strings
}

pub fn find_path(
    graph: &GroupsGraph,
    from_group_name: &str,
    to_group_name: &str,
) -> Result<Vec<Vertex>, GroupsPathError> {
    let float_graph: Graph<&String, f32> = graph.map(
        |_, n| n,
        |_, e| {
            if *e == EdgeType::UserLoadAfter {
                // A very small number so that user edges are practically always preferred.
                -1000000.0
            } else {
                1.0
            }
        },
    );

    let from_vertex = find_node_by_weight(graph, from_group_name)?;
    let to_vertex = find_node_by_weight(graph, to_group_name)?;

    let paths =
        bellman_ford(&float_graph, from_vertex).map_err(|_| PathfindingError::NegativeCycle)?;

    let mut path = vec![Vertex::new(graph[to_vertex].clone())];
    let mut current = to_vertex;
    while current != from_vertex {
        let preceding_vertex = match paths.predecessors.get(current.index()) {
            Some(Some(v)) => v,
            Some(None) => {
                logging::info!(
                    "No path found from {} to {} while looking for path to {}",
                    graph[from_vertex],
                    graph[current],
                    graph[to_vertex]
                );
                return Ok(Vec::new());
            }
            _ => {
                return Err(PathfindingError::PrecedingNodeNotFound(graph[current].clone()).into());
            }
        };

        if *preceding_vertex == current {
            logging::error!(
                "Unreachable vertex {} encountered while looking for vertex {}",
                graph[current],
                graph[from_vertex]
            );
            return Ok(Vec::new());
        }

        let edge = match graph.find_edge(*preceding_vertex, current) {
            Some(e) => e,
            None => {
                return Err(PathfindingError::EdgeNotFound {
                    from_group: graph[*preceding_vertex].clone(),
                    to_group: graph[current].clone(),
                }
                .into());
            }
        };

        let vertex = Vertex::new(graph[*preceding_vertex].clone()).with_out_edge_type(graph[edge]);
        path.push(vertex);

        current = *preceding_vertex;
    }

    path.reverse();

    Ok(path)
}

fn find_node_by_weight(
    graph: &Graph<String, EdgeType>,
    weight: &str,
) -> Result<NodeIndex, UndefinedGroupError> {
    match graph
        .node_indices()
        .find(|i| graph.node_weight(*i).map(|w| *w == weight).unwrap_or(false))
    {
        Some(n) => Ok(n),
        None => {
            logging::error!("Can't find group with name {}", weight);
            Err(UndefinedGroupError::new(weight.to_string()))
        }
    }
}

/// Sort the group vertices so that root vertices come first, in order of
/// decreasing path length, but otherwise preserving the existing
/// (lexicographical) ordering.
pub fn sorted_group_nodes(graph: &GroupsGraph) -> Vec<NodeIndex> {
    let mut nodes: Vec<(NodeIndex, bool, usize)> = graph
        .node_indices()
        .map(|n| {
            if is_root_node(graph, n) {
                let mut visitor = GroupsPathLengthVisitor::new();

                depth_first_search(graph, &mut HashMap::new(), n, &mut visitor);

                (n, true, visitor.max_path_length())
            } else {
                (n, false, 0)
            }
        })
        .collect();

    nodes.sort_by_key(|a| Reverse((a.1, a.2)));

    nodes.into_iter().map(|n| n.0).collect()
}

fn is_root_node(graph: &GroupsGraph, node_index: NodeIndex) -> bool {
    graph
        .neighbors_directed(node_index, petgraph::Direction::Incoming)
        .next()
        .is_none()
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
struct GroupsPathLengthVisitor {
    max_path_length: usize,
    current_path_length: usize,
}

impl GroupsPathLengthVisitor {
    fn new() -> Self {
        Default::default()
    }

    fn max_path_length(&self) -> usize {
        self.max_path_length
    }
}

impl<'a> DfsVisitor<'a> for GroupsPathLengthVisitor {
    fn visit_tree_edge(&mut self, _: petgraph::graph::EdgeReference<'a, EdgeType>) {}

    fn visit_forward_or_cross_edge(&mut self, _: petgraph::graph::EdgeReference<'a, EdgeType>) {}

    fn visit_back_edge(&mut self, _: petgraph::graph::EdgeReference<'a, EdgeType>) {}

    fn discover_node(&mut self, _: NodeIndex) {
        self.current_path_length += 1;
        if self.current_path_length > self.max_path_length {
            self.max_path_length = self.current_path_length
        }
    }

    fn finish_node(&mut self, _: NodeIndex) {
        self.current_path_length -= 1;
    }
}

pub fn get_default_group_node(graph: &GroupsGraph) -> Result<NodeIndex, UndefinedGroupError> {
    graph
        .node_indices()
        .find(|n| graph[*n] == Group::DEFAULT_NAME)
        .ok_or_else(|| UndefinedGroupError::new(Group::DEFAULT_NAME.to_string()))
}
