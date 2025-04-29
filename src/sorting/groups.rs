use std::cmp::Reverse;

use rustc_hash::FxHashMap as HashMap;

use petgraph::{Graph, algo::bellman_ford, graph::NodeIndex};

use crate::{
    EdgeType, LogLevel, Vertex,
    logging::{self, is_log_enabled},
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

pub type GroupsGraph = Graph<Box<str>, EdgeType>;

pub fn build_groups_graph(
    masterlist_groups: &[Group],
    userlist_groups: &[Group],
) -> Result<GroupsGraph, BuildGroupsGraphError> {
    let masterlist_groups = sorted_by_name(masterlist_groups);
    let userlist_groups = sorted_by_name(userlist_groups);

    let mut graph = GroupsGraph::new();
    let mut group_nodes: HashMap<&str, NodeIndex> = HashMap::default();

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

    if let Some(cycle) = find_cycle(&graph, |node| node.clone().into_string()) {
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
            let node_index = graph.add_node(group.name().into());
            group_nodes.insert(key, node_index);
        }
    }

    for group in groups {
        if is_log_enabled(LogLevel::Trace) {
            logging::trace!(
                "Group \"{}\" directly loads after groups \"{}\"",
                group.name(),
                group.after_groups().join(", ")
            );
        }

        let Some(node_index) = group_nodes.get(group.name()) else {
            logging::error!(
                "Unexpectedly couldn't find node for group {}: it should have just been added to the graph",
                group.name()
            );
            return Err(UndefinedGroupError::new(group.name().to_owned()));
        };

        for other_group_name in sorted_clone(group.after_groups()) {
            if let Some(other_index) = group_nodes.get(other_group_name) {
                graph.update_edge(*other_index, *node_index, edge_type);
            } else {
                return Err(UndefinedGroupError::new(other_group_name.to_owned()));
            }
        }
    }

    Ok(())
}

fn sorted_clone(strings: &[String]) -> Vec<&str> {
    let mut strings: Vec<_> = strings.iter().map(String::as_str).collect();
    strings.sort_unstable();

    strings
}

pub fn find_path(
    graph: &GroupsGraph,
    from_group_name: &str,
    to_group_name: &str,
) -> Result<Vec<Vertex>, GroupsPathError> {
    let float_graph: Graph<&Box<str>, f32> = graph.map(
        |_, n| n,
        |_, e| {
            if *e == EdgeType::UserLoadAfter {
                // A very small number so that user edges are practically always preferred.
                -1_000_000.0
            } else {
                1.0
            }
        },
    );

    let from_vertex = find_node_by_weight(graph, from_group_name)?;
    let to_vertex = find_node_by_weight(graph, to_group_name)?;

    let paths =
        bellman_ford(&float_graph, from_vertex).map_err(|_e| PathfindingError::NegativeCycle)?;

    let mut path = vec![Vertex::new(graph[to_vertex].clone().into_string())];
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
                return Err(PathfindingError::PrecedingNodeNotFound(
                    graph[current].clone().into_string(),
                )
                .into());
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

        let Some(edge) = graph.find_edge(*preceding_vertex, current) else {
            return Err(PathfindingError::EdgeNotFound {
                from_group: graph[*preceding_vertex].clone().into_string(),
                to_group: graph[current].clone().into_string(),
            }
            .into());
        };

        let vertex = Vertex::new(graph[*preceding_vertex].clone().into_string())
            .with_out_edge_type(graph[edge]);
        path.push(vertex);

        current = *preceding_vertex;
    }

    path.reverse();

    Ok(path)
}

fn find_node_by_weight(
    graph: &Graph<Box<str>, EdgeType>,
    weight: &str,
) -> Result<NodeIndex, UndefinedGroupError> {
    if let Some(n) = graph
        .node_indices()
        .find(|i| graph.node_weight(*i).is_some_and(|w| w.as_ref() == weight))
    {
        Ok(n)
    } else {
        logging::error!("Can't find group with name {weight}");
        Err(UndefinedGroupError::new(weight.to_owned()))
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

                depth_first_search(graph, &mut HashMap::default(), n, &mut visitor);

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
        GroupsPathLengthVisitor::default()
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
            self.max_path_length = self.current_path_length;
        }
    }

    fn finish_node(&mut self, _: NodeIndex) {
        self.current_path_length -= 1;
    }
}

pub fn get_default_group_node(graph: &GroupsGraph) -> Result<NodeIndex, UndefinedGroupError> {
    graph
        .node_indices()
        .find(|n| graph[*n].as_ref() == Group::DEFAULT_NAME)
        .ok_or_else(|| UndefinedGroupError::new(Group::DEFAULT_NAME.to_owned()))
}

#[cfg(test)]
mod tests {
    use super::*;

    mod build_groups_graph {
        use super::*;

        #[test]
        fn should_error_if_an_after_group_does_not_exist() {
            let groups = &[Group::new("b".into()).with_after_groups(vec!["a".into()])];

            match build_groups_graph(groups, &[]) {
                Err(BuildGroupsGraphError::UndefinedGroup(e)) => {
                    assert_eq!("a", e.into_group_name());
                }
                _ => panic!("Expected an undefined group error"),
            }
        }

        #[test]
        fn should_error_if_masterlist_group_loads_after_user_group() {
            let masterlist = &[Group::new("b".into()).with_after_groups(vec!["a".into()])];
            let userlist = &[Group::new("a".into())];

            match build_groups_graph(masterlist, userlist) {
                Err(BuildGroupsGraphError::UndefinedGroup(e)) => {
                    assert_eq!("a", e.into_group_name());
                }
                _ => panic!("Expected an undefined group error"),
            }
        }

        #[test]
        fn should_error_if_after_groups_are_cyclic() {
            let masterlist = &[
                Group::new("a".into()),
                Group::new("b".into()).with_after_groups(vec!["a".into()]),
            ];
            let userlist = &[
                Group::new("a".into()).with_after_groups(vec!["c".into()]),
                Group::new("c".into()).with_after_groups(vec!["b".into()]),
            ];

            match build_groups_graph(masterlist, userlist) {
                Err(BuildGroupsGraphError::CycleFound(e)) => {
                    let cycle = e.into_cycle();

                    assert_eq!(
                        &[
                            Vertex::new("a".into())
                                .with_out_edge_type(EdgeType::MasterlistLoadAfter),
                            Vertex::new("b".into()).with_out_edge_type(EdgeType::UserLoadAfter),
                            Vertex::new("c".into()).with_out_edge_type(EdgeType::UserLoadAfter),
                        ],
                        cycle.as_slice()
                    );
                }
                _ => panic!("Expected a cyclic interaction error"),
            }
        }

        #[test]
        fn cyclic_interaction_error_should_only_include_groups_that_are_part_of_the_cycle() {
            let masterlist = &[
                Group::new("a".into()).with_after_groups(vec!["b".into()]),
                Group::new("b".into()).with_after_groups(vec!["a".into()]),
                Group::new("c".into()).with_after_groups(vec!["b".into()]),
            ];

            match build_groups_graph(masterlist, &[]) {
                Err(BuildGroupsGraphError::CycleFound(e)) => {
                    let cycle = e.into_cycle();

                    assert_eq!(
                        &[
                            Vertex::new("a".into())
                                .with_out_edge_type(EdgeType::MasterlistLoadAfter),
                            Vertex::new("b".into())
                                .with_out_edge_type(EdgeType::MasterlistLoadAfter),
                        ],
                        cycle.as_slice()
                    );
                }
                _ => panic!("Expected a cyclic interaction error"),
            }
        }
    }

    mod find_path {
        use super::*;

        #[test]
        fn should_error_if_the_from_group_does_not_exist() {
            let masterlist = &[
                Group::new("a".into()),
                Group::new("b".into()).with_after_groups(vec!["a".into()]),
            ];
            let graph = build_groups_graph(masterlist, &[]).unwrap();

            assert!(find_path(&graph, "c", "a").is_err());
        }

        #[test]
        fn should_error_if_the_to_group_does_not_exist() {
            let masterlist = &[
                Group::new("a".into()),
                Group::new("b".into()).with_after_groups(vec!["a".into()]),
            ];
            let graph = build_groups_graph(masterlist, &[]).unwrap();

            assert!(find_path(&graph, "a", "c").is_err());
        }

        #[test]
        fn should_return_an_empty_vec_if_there_is_no_path() {
            let masterlist = &[Group::new("a".into()), Group::new("b".into())];
            let graph = build_groups_graph(masterlist, &[]).unwrap();

            let path = find_path(&graph, "a", "b").unwrap();

            assert!(path.is_empty());
        }

        #[test]
        fn should_find_the_shortest_path_if_there_is_no_user_metadata() {
            let masterlist = &[
                Group::new("a".into()),
                Group::new("b".into()).with_after_groups(vec!["a".into()]),
                Group::new("c".into()).with_after_groups(vec!["a".into()]),
                Group::new("d".into()).with_after_groups(vec!["c".into()]),
                Group::new("e".into()).with_after_groups(vec!["b".into(), "d".into()]),
            ];
            let graph = build_groups_graph(masterlist, &[]).unwrap();

            let path = find_path(&graph, "a", "e").unwrap();

            assert_eq!(
                &[
                    Vertex::new("a".into()).with_out_edge_type(EdgeType::MasterlistLoadAfter),
                    Vertex::new("b".into()).with_out_edge_type(EdgeType::MasterlistLoadAfter),
                    Vertex::new("e".into())
                ],
                path.as_slice()
            );
        }

        #[test]
        fn should_find_the_shortest_path_involving_user_metadata_if_there_no_user_metadata() {
            let masterlist = &[
                Group::new("a".into()),
                Group::new("b".into()).with_after_groups(vec!["a".into()]),
                Group::new("c".into()).with_after_groups(vec!["a".into()]),
                Group::new("e".into()).with_after_groups(vec!["b".into()]),
            ];
            let userlist = &[
                Group::new("d".into()).with_after_groups(vec!["c".into()]),
                Group::new("e".into()).with_after_groups(vec!["d".into()]),
            ];
            let graph = build_groups_graph(masterlist, userlist).unwrap();

            let path = find_path(&graph, "a", "e").unwrap();

            assert_eq!(
                &[
                    Vertex::new("a".into()).with_out_edge_type(EdgeType::MasterlistLoadAfter),
                    Vertex::new("c".into()).with_out_edge_type(EdgeType::UserLoadAfter),
                    Vertex::new("d".into()).with_out_edge_type(EdgeType::UserLoadAfter),
                    Vertex::new("e".into())
                ],
                path.as_slice()
            );
        }

        #[test]
        fn should_not_depend_on_the_after_group_definition_order() {
            let masterlists = &[
                &[
                    Group::new("a".into()),
                    Group::new("b".into()).with_after_groups(vec!["a".into()]),
                    Group::new("c".into()).with_after_groups(vec!["a".into()]),
                    Group::new("d".into()).with_after_groups(vec!["b".into(), "c".into()]),
                    Group::new("e".into()).with_after_groups(vec!["d".into()]),
                ],
                &[
                    Group::new("a".into()),
                    Group::new("b".into()).with_after_groups(vec!["a".into()]),
                    Group::new("c".into()).with_after_groups(vec!["a".into()]),
                    Group::new("d".into()).with_after_groups(vec!["c".into(), "b".into()]),
                    Group::new("e".into()).with_after_groups(vec!["d".into()]),
                ],
            ];

            for masterlist in masterlists {
                let graph = build_groups_graph(*masterlist, &[]).unwrap();

                let path = find_path(&graph, "a", "e").unwrap();
                assert_eq!(
                    &[
                        Vertex::new("a".into()).with_out_edge_type(EdgeType::MasterlistLoadAfter),
                        Vertex::new("b".into()).with_out_edge_type(EdgeType::MasterlistLoadAfter),
                        Vertex::new("d".into()).with_out_edge_type(EdgeType::MasterlistLoadAfter),
                        Vertex::new("e".into())
                    ],
                    path.as_slice()
                );
            }
        }
    }
}
