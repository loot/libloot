/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2018    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */

#include "group_sort.h"

#include <boost/algorithm/string.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>

#include "api/helpers/logging.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
typedef boost::graph_traits<GroupGraph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<GroupGraph>::edge_descriptor edge_t;
typedef boost::associative_property_map<std::map<edge_t, int>> edge_map_t;

class CycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(edge_t edge, const GroupGraph& graph) {
    auto source = boost::source(edge, graph);

    auto vertex = Vertex(graph[source], graph[edge]);

    trail.push_back(vertex);
  }

  void back_edge(edge_t edge, const GroupGraph& graph) {
    auto source = boost::source(edge, graph);
    const auto target = boost::target(edge, graph);
    const auto targetGroupName = graph[target];

    auto vertex = Vertex(graph[source], graph[edge]);
    trail.push_back(vertex);

    auto it = find_if(begin(trail), end(trail), [&](const Vertex& v) {
      return v.GetName() == graph[target];
    });

    if (it == trail.end()) {
      throw std::logic_error(
          "The target of a back edge cannot be found in the current edge path. "
          "The target group is \"" +
          targetGroupName + "\"");
    }

    throw CyclicInteractionError(std::vector<Vertex>(it, trail.end()));
  }

  void finish_vertex(vertex_t, const GroupGraph&) {
    if (!trail.empty()) {
      trail.pop_back();
    }
  }

private:
  std::vector<Vertex> trail;
};

std::vector<Group> SortByName(const std::vector<Group>& groups) {
  auto copy = groups;
  std::sort(copy.begin(), copy.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.GetName() < rhs.GetName();
  });

  return copy;
}

std::vector<std::string> SortNames(std::vector<std::string>&& groupNames) {
  std::sort(groupNames.begin(), groupNames.end());

  return groupNames;
}

GroupGraph BuildGroupGraph(const std::vector<Group>& masterlistGroups,
                           const std::vector<Group>& userGroups) {
  const auto logger = getLogger();

  GroupGraph graph;
  std::unordered_map<std::string, vertex_t> groupVertices;

  const auto addGroups = [&](const std::vector<Group>& groups,
                             const EdgeType edgeType) {
    for (const auto& group : groups) {
      const auto groupName = group.GetName();

      if (groupVertices.find(groupName) == groupVertices.end()) {
        const auto vertex = boost::add_vertex(groupName, graph);
        groupVertices.emplace(groupName, vertex);
      }
    }

    for (const auto& group : groups) {
      const auto groupName = group.GetName();

      if (logger) {
        logger->trace("Group \"{}\" directly loads after groups \"{}\"",
                      groupName,
                      boost::join(group.GetAfterGroups(), ", "));
      }

      const auto vertex = groupVertices.at(groupName);

      // Similar to groups, after groups are sorted by name so that the order
      // of a group vertex's in-edges is independent of the order they're
      // listed in the group definition. The order of in-edges affects the
      // result of calling GetGroupsPath().
      for (const auto& otherGroupName : SortNames(group.GetAfterGroups())) {
        const auto otherVertex = groupVertices.find(otherGroupName);
        if (otherVertex == groupVertices.end()) {
          throw UndefinedGroupError(otherGroupName);
        }

        boost::add_edge(otherVertex->second, vertex, edgeType, graph);
      }
    }
  };

  // Sort groups by name so that they get added to the graph in an order that
  // is consistent and independent of the order in which they are defined.
  // This is important because the order in which vertices are created affects
  // the order in which edges are created and so can affect the outcome of
  // sorting.
  // It would be surprising if swapping the order in which two groups were
  // defined in e.g. the masterlist had an impact on LOOT's sorting behaviour,
  // but if a group's name changes that's effectively deleting one group and
  // creating another. It would also be surprising that the groups' names can
  // have an effect, but the effect is at least constant for a given set of
  // groups.
  // It might also be surprising that whether a group is defined in the
  // masterlist or userlist can have an effect, but it's consistent with the
  // handling of edges for all other masterlist and userlist metadata.
  if (logger) {
    logger->trace("Adding masterlist groups to groups graph...");
  }
  addGroups(SortByName(masterlistGroups), EdgeType::masterlistLoadAfter);

  if (logger) {
    logger->trace("Adding user groups to groups graph...");
  }
  addGroups(SortByName(userGroups), EdgeType::userLoadAfter);

  if (logger) {
    logger->trace("Checking for cycles in the group graph");
  }
  boost::depth_first_search(graph, boost::visitor(CycleDetector()));

  return graph;
}

vertex_t GetVertexByName(const GroupGraph& graph, std::string_view name) {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph))) {
    if (graph[vertex] == name) {
      return vertex;
    }
  }

  const auto logger = getLogger();
  if (logger) {
    logger->error("Can't find group with name \"{}\"", name);
  }

  throw std::invalid_argument("Can't find group with name \"" + std::string(name) + "\"");
}

std::vector<Vertex> GetGroupsPath(const GroupGraph& graph,
                                  std::string_view fromGroupName,
                                  std::string_view toGroupName) {
  auto logger = getLogger();

  auto fromVertex = GetVertexByName(graph, fromGroupName);
  auto toVertex = GetVertexByName(graph, toGroupName);

  std::map<edge_t, int> weightMap;
  for (const auto& edge : boost::make_iterator_range(boost::edges(graph))) {
    if (graph[edge] == EdgeType::userLoadAfter) {
      // Magnitude is an arbitrarily large number.
      static constexpr int USER_LOAD_AFTER_EDGE_WEIGHT = -1000000;
      weightMap[edge] = USER_LOAD_AFTER_EDGE_WEIGHT;
    } else {
      weightMap[edge] = 1;
    }
  }

  std::vector<vertex_t> predecessors(boost::num_vertices(graph));
  std::vector<int> distance(predecessors.size(),
                            (std::numeric_limits<int>::max)());
  distance.at(fromVertex) = 0;

  bellman_ford_shortest_paths(
      graph,
      boost::weight_map(edge_map_t(weightMap))
          .predecessor_map(boost::make_iterator_property_map(
              predecessors.begin(), get(boost::vertex_index, graph)))
          .distance_map(distance.data())
          .root_vertex(fromVertex));

  std::vector<Vertex> path{Vertex(graph[toVertex])};
  vertex_t currentVertex = toVertex;
  while (currentVertex != fromVertex) {
    const auto precedingVertex = predecessors.at(currentVertex);
    if (precedingVertex == currentVertex) {
      if (logger) {
        logger->error(
            "Unreachable vertex {} encountered while looking for vertex {}",
            graph[currentVertex],
            graph[toVertex]);
      }
      return std::vector<Vertex>();
    }

    const auto pair = boost::edge(precedingVertex, currentVertex, graph);
    if (!pair.second) {
      throw std::runtime_error("Unexpectedly couldn't find edge between \"" +
                               graph[precedingVertex] + "\" and \"" +
                               graph[currentVertex] + "\"");
    }
    const auto vertex = Vertex(graph[precedingVertex], graph[pair.first]);
    path.push_back(vertex);

    currentVertex = precedingVertex;
  }

  std::reverse(path.begin(), path.end());

  return path;
}
    }
