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
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/depth_first_search.hpp>

#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::directedS,
                              std::string> GroupGraph;
typedef boost::graph_traits<GroupGraph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<GroupGraph>::edge_descriptor edge_t;

class GroupCycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(edge_t edge, const GroupGraph& graph) {
    auto source = boost::source(edge, graph);
    auto name = graph[source];

    // Check if the plugin already exists in the recorded trail.
    auto it = find(begin(trail), end(trail), name);

    if (it != end(trail)) {
      // Erase everything from this position onwards, as it doesn't
      // contribute to a forward-cycle.
      trail.erase(it, end(trail));
    }

    trail.push_back(name);
  }

  void back_edge(edge_t edge, const GroupGraph& graph) {
    auto source = boost::source(edge, graph);
    auto target = boost::target(edge, graph);

    trail.push_back(graph[source]);
    std::string backCycle;
    auto it = find(begin(trail), end(trail), graph[target]);
    for (it; it != end(trail); ++it) {
      backCycle += *it + ", ";
    }
    backCycle.erase(backCycle.length() - 2);

    throw CyclicInteractionError(
        graph[source], graph[target], backCycle);
  }

private:
  std::vector<std::string> trail;
};

class AfterGroupsVisitor : public boost::dfs_visitor<> {
public:
  AfterGroupsVisitor(std::unordered_set<std::string>& visitedGroups) : visitedGroups_(visitedGroups) {}
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

std::unordered_map<std::string, std::unordered_set<std::string>> GetTransitiveAfterGroups(const std::unordered_set<Group> groups) {
  GroupGraph graph;

  std::unordered_map<std::string, vertex_t> groupVertices;
  for (const auto& group : groups) {
      auto vertex = boost::add_vertex(group.GetName(), graph);
      groupVertices.emplace(group.GetName(), vertex);
  }

  for (const auto& group : groups) {
    for (const auto& otherGroupName : group.GetAfterGroups()) {
      auto otherVertex = groupVertices.find(otherGroupName);
      if (otherVertex == groupVertices.end()) {
        throw UndefinedGroupError(otherGroupName);
      }

      auto vertex = groupVertices[group.GetName()];
      boost::add_edge(vertex, otherVertex->second, graph);
    }
  }

  // Check for cycles.
  boost::depth_first_search(graph, visitor(GroupCycleDetector()));

  std::unordered_map<std::string, std::unordered_set<std::string>> transitiveAfterGroups;
  for (const vertex_t& vertex : boost::make_iterator_range(boost::vertices(graph))) {
    std::unordered_set<std::string> visitedGroups;
    AfterGroupsVisitor afterGroupsVisitor(visitedGroups);

    // Create a color map.
    std::vector<boost::default_color_type> colorVec(boost::num_vertices(graph));
    auto colorMap = boost::make_iterator_property_map(colorVec.begin(),
      boost::get(boost::vertex_index, graph),
      colorVec[0]);

    boost::depth_first_visit(graph, vertex, afterGroupsVisitor, colorMap);
    transitiveAfterGroups[graph[vertex]] = visitedGroups;
  }

  return transitiveAfterGroups;
}
}
