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

    // Check if the vertex already exists in the recorded trail.
    auto it = find_if(begin(trail), end(trail), [&](const Vertex& v) {
      return v.GetName() == graph[source];
    });

    if (it != end(trail)) {
      // Erase everything from this position onwards, as it doesn't
      // contribute to a forward-cycle.
      trail.erase(it, end(trail));
    }

    trail.push_back(vertex);
  }

  void back_edge(edge_t edge, const GroupGraph& graph) {
    auto source = boost::source(edge, graph);
    auto target = boost::target(edge, graph);

    auto vertex = Vertex(graph[source], graph[edge]);
    trail.push_back(vertex);

    auto it = find_if(begin(trail), end(trail), [&](const Vertex& v) {
      return v.GetName() == graph[target];
    });

    if (it != trail.end()) {
      throw CyclicInteractionError(std::vector<Vertex>(it, trail.end()));
    }
  }

private:
  std::vector<Vertex> trail;
};

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
      for (const auto& otherGroupName : group.GetAfterGroups()) {
        const auto otherVertex = groupVertices.find(otherGroupName);
        if (otherVertex == groupVertices.end()) {
          throw UndefinedGroupError(otherGroupName);
        }

        boost::add_edge(otherVertex->second, vertex, edgeType, graph);
      }
    }
  };

  if (logger) {
    logger->trace("Adding masterlist groups to groups graph...");
  }
  addGroups(masterlistGroups, EdgeType::masterlistLoadAfter);

  if (logger) {
    logger->trace("Adding user groups to groups graph...");
  }
  addGroups(userGroups, EdgeType::userLoadAfter);

  return graph;
}

void CheckForCycles(const GroupGraph& graph) {
  const auto logger = getLogger();
  if (logger) {
    logger->trace("Checking for cycles in the group graph");
  }

  boost::depth_first_search(graph, boost::visitor(CycleDetector()));
}

vertex_t GetVertexByName(const GroupGraph& graph, const std::string& name) {
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

  throw std::invalid_argument("Can't find group with name \"" + name + "\"");
}

std::vector<Vertex> GetGroupsPath(const std::vector<Group>& masterlistGroups,
                                  const std::vector<Group>& userGroups,
                                  const std::string& fromGroupName,
                                  const std::string& toGroupName) {
  GroupGraph graph = BuildGroupGraph(masterlistGroups, userGroups);

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

std::vector<Group> MergeGroups(const std::vector<Group>& masterlistGroups,
                               const std::vector<Group>& userGroups) {
  auto mergedGroups = masterlistGroups;

  std::vector<Group> newGroups;
  for (const auto& userGroup : userGroups) {
    auto groupIt =
        std::find_if(mergedGroups.begin(),
                     mergedGroups.end(),
                     [&](const Group& existingGroup) {
                       return existingGroup.GetName() == userGroup.GetName();
                     });

    if (groupIt == mergedGroups.end()) {
      newGroups.push_back(userGroup);
    } else {
      // Replace the masterlist group description with the userlist group
      // description if the latter is not empty.
      auto description = userGroup.GetDescription().empty()
                             ? groupIt->GetDescription()
                             : userGroup.GetDescription();

      auto afterGroups = groupIt->GetAfterGroups();
      auto userAfterGroups = userGroup.GetAfterGroups();
      afterGroups.insert(
          afterGroups.end(), userAfterGroups.begin(), userAfterGroups.end());

      *groupIt = Group(userGroup.GetName(), afterGroups, description);
    }
  }

  mergedGroups.insert(mergedGroups.end(), newGroups.cbegin(), newGroups.cend());

  return mergedGroups;
}
}
