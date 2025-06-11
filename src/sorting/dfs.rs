use std::collections::VecDeque;

use petgraph::{
    Graph,
    graph::{EdgeReference, NodeIndex},
    visit::EdgeRef,
};
use rustc_hash::{FxHashMap as HashMap, FxHashSet as HashSet};

use crate::{EdgeType, Vertex, logging};

pub trait DfsVisitor<'a> {
    fn visit_tree_edge(&mut self, edge_ref: EdgeReference<'a, EdgeType>);

    fn visit_forward_or_cross_edge(&mut self, edge_ref: EdgeReference<'a, EdgeType>);

    fn visit_back_edge(&mut self, edge_ref: EdgeReference<'a, EdgeType>);

    fn discover_node(&mut self, node_index: NodeIndex);

    fn finish_node(&mut self, node_index: NodeIndex);
}

pub trait BidirBfsVisitor {
    fn visit_forward_bfs_edge(&mut self, source: NodeIndex, target: NodeIndex);

    fn visit_reverse_bfs_edge(&mut self, source: NodeIndex, target: NodeIndex);

    fn visit_intersection_node(&mut self, node: NodeIndex);
}

pub fn bidirectional_bfs<N, E>(
    graph: &Graph<N, E>,
    from_index: NodeIndex,
    to_index: NodeIndex,
    visitor: &mut impl BidirBfsVisitor,
) -> bool {
    let mut forward_queue = VecDeque::from([from_index]);
    let mut reverse_queue = VecDeque::from([to_index]);
    let mut forward_visited = HashSet::default();
    forward_visited.insert(from_index);
    let mut reverse_visited = HashSet::default();
    reverse_visited.insert(to_index);

    while let (Some(forward_current), Some(reverse_current)) =
        (forward_queue.pop_front(), reverse_queue.pop_front())
    {
        if forward_current == to_index || reverse_visited.contains(&forward_current) {
            visitor.visit_intersection_node(forward_current);
            return true;
        }

        for adjacent in graph.neighbors(forward_current) {
            if !forward_visited.contains(&adjacent) {
                visitor.visit_forward_bfs_edge(forward_current, adjacent);

                forward_visited.insert(adjacent);
                forward_queue.push_back(adjacent);
            }
        }

        if reverse_current == from_index || forward_visited.contains(&reverse_current) {
            visitor.visit_intersection_node(reverse_current);
            return true;
        }

        for adjacent in graph.neighbors_directed(reverse_current, petgraph::Direction::Incoming) {
            if !reverse_visited.contains(&adjacent) {
                visitor.visit_reverse_bfs_edge(adjacent, reverse_current);

                reverse_visited.insert(adjacent);
                reverse_queue.push_back(adjacent);
            }
        }
    }

    false
}

// Petgraph has APIs for performing depth-first searches, but they don't give any information about the current edge, only its source and target nodes, which is a problem if the same pair of nodes can have multiple edges between them with different weights. As such, implement it myself.
pub fn find_cycle<N>(
    graph: &Graph<N, EdgeType>,
    node_mapper: impl FnMut(&N) -> String,
) -> Option<Vec<Vertex>> {
    let mut cycle_detector = CycleDetector::new(graph, node_mapper);

    let mut colour_map = HashMap::default();

    for node_index in graph.node_indices() {
        depth_first_search(graph, &mut colour_map, node_index, &mut cycle_detector);

        if cycle_detector.found_cycle() {
            return cycle_detector.into_cycle_path();
        }
    }

    None
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[non_exhaustive]
pub enum Colour {
    #[default]
    White,
    Grey,
    Black,
}

pub fn depth_first_search<'a, N>(
    graph: &'a Graph<N, EdgeType>,
    colour_map: &mut HashMap<NodeIndex, Colour>,
    start_node_index: NodeIndex,
    visitor: &mut impl DfsVisitor<'a>,
) {
    let mut stack = vec![(start_node_index, edges(graph, start_node_index))];

    colour_map.insert(start_node_index, Colour::Grey);
    visitor.discover_node(start_node_index);

    while let Some((current, unprocessed_edges)) = stack.last_mut() {
        if let Some(edge) = unprocessed_edges.next() {
            let target = edge.target();

            match colour_map.get(&target).unwrap_or(&Colour::White) {
                Colour::White => {
                    visitor.visit_tree_edge(edge);

                    colour_map.insert(target, Colour::Grey);
                    visitor.discover_node(target);

                    stack.push((target, edges(graph, target)));
                }
                Colour::Grey => visitor.visit_back_edge(edge),
                Colour::Black => visitor.visit_forward_or_cross_edge(edge),
            }
        } else {
            colour_map.insert(*current, Colour::Black);
            visitor.finish_node(*current);

            stack.pop();
        }
    }
}

fn edges<N>(
    graph: &Graph<N, EdgeType>,
    node_index: NodeIndex,
) -> impl Iterator<Item = EdgeReference<'_, EdgeType>> {
    // Petgraph produces edges in the reverse of the order that neighbouring
    // nodes were added to the graph, but for backwards compatibility we want
    // the opposite order.
    // Unfortunately Petgraph's Edges iterator doesn't impl DoubleEndedIterator
    // (though it probably could), so this needs to buffer the edges.
    let mut edges: Vec<_> = graph.edges(node_index).collect();
    edges.reverse();
    edges.into_iter()
}

#[derive(Clone, Debug)]
struct CycleDetector<'a, N, F: FnMut(&N) -> String> {
    graph: &'a Graph<N, EdgeType>,
    get_node_name: F,
    trail: Vec<Vertex>,
    found_cycle: bool,
}

impl<'a, N, F: FnMut(&N) -> String> CycleDetector<'a, N, F> {
    fn new(graph: &'a Graph<N, EdgeType>, get_node_name: F) -> Self {
        CycleDetector {
            graph,
            get_node_name,
            trail: Vec::new(),
            found_cycle: false,
        }
    }

    fn found_cycle(&self) -> bool {
        self.found_cycle
    }

    fn into_cycle_path(self) -> Option<Vec<Vertex>> {
        self.found_cycle.then_some(self.trail)
    }
}

impl<'a, N, F: FnMut(&N) -> String> DfsVisitor<'a> for CycleDetector<'a, N, F> {
    fn visit_tree_edge(&mut self, edge_ref: EdgeReference<'a, EdgeType>) {
        if self.found_cycle {
            return;
        }

        let source = edge_ref.source();
        let name = (self.get_node_name)(&self.graph[source]);
        let edge_type = *edge_ref.weight();

        let vertex = Vertex::new(name).with_out_edge_type(edge_type);

        self.trail.push(vertex);
    }

    fn visit_forward_or_cross_edge(&mut self, _: EdgeReference<'a, EdgeType>) {}

    fn visit_back_edge(&mut self, edge_ref: EdgeReference<'a, EdgeType>) {
        if self.found_cycle {
            return;
        }

        self.visit_tree_edge(edge_ref);

        let target_name = (self.get_node_name)(&self.graph[edge_ref.target()]);

        if let Some(pos) = self.trail.iter().position(|v| v.name() == target_name) {
            self.trail.drain(..pos);
            self.found_cycle = true;
        } else {
            logging::error!(
                "The target of a back edge cannot be found in the current visitor trail"
            );
        }
    }

    fn discover_node(&mut self, _: NodeIndex) {}

    fn finish_node(&mut self, _: NodeIndex) {
        if !self.found_cycle {
            self.trail.pop();
        }
    }
}
