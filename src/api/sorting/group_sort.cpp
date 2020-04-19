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

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>

#include "api/helpers/logging.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::directedS,
                              std::string,
                              EdgeType>
    GroupGraph;
typedef boost::graph_traits<GroupGraph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<GroupGraph>::edge_descriptor edge_t;
typedef boost::associative_property_map<std::map<edge_t, int>> edge_map_t;

class AfterGroupsVisitor : public boost::dfs_visitor<> {
public:
  AfterGroupsVisitor(std::unordered_set<std::string>& visitedGroups) :
      visitedGroups_(visitedGroups) {}
  void tree_edge(edge_t edge, const GroupGraph& graph) {
    auto target = boost::target(edge, graph);
    visitedGroups_.insert(graph[target]);
  }

  std::unordered_set<std::string> get_visited_groups() const {
    return visitedGroups_;
  }

private:
  std::unordered_set<std::string>& visitedGroups_;
};

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

std::string joinVector(const std::vector<std::string>& set) {
  std::string output;
  for (const auto& element : set) {
    output += element + ", ";
  }

  return output.substr(0, output.length() - 2);
}

std::string joinUnorderedSet(const std::unordered_set<std::string>& set) {
  std::string output;
  for (const auto& element : set) {
    output += element + ", ";
  }

  return output.substr(0, output.length() - 2);
}

GroupGraph BuildGraph(const std::vector<Group>& masterlistGroups,
                      const std::vector<Group>& userGroups) {
  GroupGraph graph;

  std::unordered_map<std::string, vertex_t> groupVertices;
  for (const auto& group : masterlistGroups) {
    auto vertex = boost::add_vertex(group.GetName(), graph);
    groupVertices.emplace(group.GetName(), vertex);
  }

  auto logger = getLogger();
  for (const auto& group : masterlistGroups) {
    if (logger) {
      logger->trace(
          "Masterlist group \"{}\" directly loads after groups \"{}\"",
          group.GetName(),
          joinVector(group.GetAfterGroups()));
    }

    auto vertex = groupVertices.at(group.GetName());
    for (const auto& otherGroupName : group.GetAfterGroups()) {
      auto otherVertex = groupVertices.find(otherGroupName);
      if (otherVertex == groupVertices.end()) {
        throw UndefinedGroupError(otherGroupName);
      }

      boost::add_edge(
          vertex, otherVertex->second, EdgeType::masterlistLoadAfter, graph);
    }
  }

  for (const auto& group : userGroups) {
    if (groupVertices.find(group.GetName()) == groupVertices.end()) {
      auto vertex = boost::add_vertex(group.GetName(), graph);
      groupVertices.emplace(group.GetName(), vertex);
    }
  }

  for (const auto& group : userGroups) {
    if (logger) {
      logger->trace("Userlist group \"{}\" directly loads after groups \"{}\"",
                    group.GetName(),
                    joinVector(group.GetAfterGroups()));
    }

    auto vertex = groupVertices.at(group.GetName());
    for (const auto& otherGroupName : group.GetAfterGroups()) {
      auto otherVertex = groupVertices.find(otherGroupName);
      if (otherVertex == groupVertices.end()) {
        throw UndefinedGroupError(otherGroupName);
      }

      boost::add_edge(
          vertex, otherVertex->second, EdgeType::userLoadAfter, graph);
    }
  }

  return graph;
}

std::unordered_map<std::string, std::unordered_set<std::string>>
GetTransitiveAfterGroups(const std::vector<Group>& masterlistGroups,
                         const std::vector<Group>& userGroups) {
  GroupGraph graph = BuildGraph(masterlistGroups, userGroups);

  auto logger = getLogger();
  if (logger) {
    logger->info("Sorting groups according to their load after data");
  }

  // Check for cycles.
  if (logger) {
    logger->trace("Checking for cycles in the group graph");
  }
  boost::depth_first_search(graph, boost::visitor(CycleDetector()));

  std::unordered_map<std::string, std::unordered_set<std::string>>
      transitiveAfterGroups;
  for (const vertex_t& vertex :
       boost::make_iterator_range(boost::vertices(graph))) {
    std::unordered_set<std::string> visitedGroups;
    AfterGroupsVisitor afterGroupsVisitor(visitedGroups);

    // Create a color map.
    std::vector<boost::default_color_type> colorVec(boost::num_vertices(graph));
    auto colorMap = boost::make_iterator_property_map(
        colorVec.begin(), boost::get(boost::vertex_index, graph), colorVec[0]);

    boost::depth_first_visit(graph, vertex, afterGroupsVisitor, colorMap);
    transitiveAfterGroups[graph[vertex]] = visitedGroups;

    if (logger) {
      logger->trace("Group \"{}\" transitively loads after groups \"{}\"",
                    graph[vertex],
                    joinUnorderedSet(visitedGroups));
    }
  }

  return transitiveAfterGroups;
}

vertex_t GetVertexByName(const GroupGraph& graph, const std::string& name) {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph))) {
    if (graph[vertex] == name) {
      return vertex;
    }
  }

  auto logger = getLogger();
  if (logger) {
    logger->error("Can't find group with name \"{}\"", name);
  }

  throw std::invalid_argument("Can't find group with name \"" + name + "\"");
}

std::vector<Vertex> GetGroupsPath(
    const std::vector<Group>& masterlistGroups,
    const std::vector<Group>& userGroups,
    const std::string& fromGroupName,
    const std::string& toGroupName) {
  GroupGraph graph = BuildGraph(masterlistGroups, userGroups);

  auto logger = getLogger();

  auto fromVertex = GetVertexByName(graph, fromGroupName);
  auto toVertex = GetVertexByName(graph, toGroupName);

  std::map<edge_t, int> weightMap;
  for (const auto& edge : boost::make_iterator_range(boost::edges(graph))) {
    if (graph[edge] == EdgeType::userLoadAfter) {
      weightMap[edge] = -1000000;  // Magnitude is an arbitrarily large number.
    } else {
      weightMap[edge] = 1;
    }
  }

  std::vector<vertex_t> predecessors(boost::num_vertices(graph));
  std::vector<int> distance(predecessors.size(),
                            (std::numeric_limits<int>::max)());
  distance[toVertex] = 0;

  bellman_ford_shortest_paths(
      graph,
      boost::weight_map(edge_map_t(weightMap))
          .predecessor_map(boost::make_iterator_property_map(
              predecessors.begin(), get(boost::vertex_index, graph)))
          .distance_map(&distance[0])
          .root_vertex(toVertex));

  std::vector<Vertex> path;
  vertex_t currentVertex = fromVertex;
  while (currentVertex != toVertex) {
    auto nextVertex = predecessors[currentVertex];
    if (nextVertex == currentVertex) {
      if (logger) {
        logger->error(
            "Unreachable vertex {} encountered while looking for vertex {}",
            graph[currentVertex],
            graph[toVertex]);
      }
      return std::vector<Vertex>();
    }

    auto pair = boost::edge(nextVertex, currentVertex, graph);
    if (!pair.second) {
      throw std::runtime_error("Unexpectedly couldn't find edge between \"" +
                               graph[currentVertex] + "\" and \"" +
                               graph[nextVertex] + "\"");
    }
    auto vertex = Vertex(graph[currentVertex], graph[pair.first]);
    path.push_back(vertex);

    currentVertex = nextVertex;
  }
  path.push_back(Vertex(graph[currentVertex]));

  return path;
}
}
