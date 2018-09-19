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
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>

#include "api/helpers/logging.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
class GroupSortingData {
public:
  GroupSortingData() {}
  GroupSortingData(std::string name) : name_(name) {}

  std::string GetName() const { return name_; }

  std::unordered_set<std::string> GetMasterlistAfterGroups() const {
    return masterlistAfterGroups_;
  }

  std::unordered_set<std::string> GetUserAfterGroups() const {
    return userAfterGroups_;
  }

  void SetMasterlistAfterGroups(std::unordered_set<std::string> groups) {
    masterlistAfterGroups_ = groups;
  }

  void SetUserAfterGroups(std::unordered_set<std::string> groups) {
    userAfterGroups_ = groups;
  }

private:
  std::string name_;
  std::unordered_set<std::string> masterlistAfterGroups_;
  std::unordered_set<std::string> userAfterGroups_;
};

typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::directedS,
                              GroupSortingData,
                              EdgeType>
    GroupGraph;
typedef boost::graph_traits<GroupGraph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<GroupGraph>::edge_descriptor edge_t;

class CycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(edge_t edge, const GroupGraph& graph) {
    auto source = boost::source(edge, graph);

    auto vertex = Vertex(graph[source].GetName(), graph[edge]);

    // Check if the plugin already exists in the recorded trail.
    auto it = find_if(begin(trail), end(trail), [&](const Vertex& v) {
      return v.GetName() == graph[source].GetName();
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

    auto vertex = Vertex(graph[source].GetName(), graph[edge]);
    trail.push_back(vertex);

    auto it = find_if(begin(trail), end(trail), [&](const Vertex& v) {
      return v.GetName() == graph[target].GetName();
    });

    if (it != trail.end()) {
      throw CyclicInteractionError(std::vector<Vertex>(it, trail.end()));
    }
  }

private:
  std::vector<Vertex> trail;
};

class AfterGroupsVisitor : public boost::dfs_visitor<> {
public:
  AfterGroupsVisitor(std::unordered_set<std::string>& visitedGroups) :
      visitedGroups_(visitedGroups) {}
  void tree_edge(edge_t edge, const GroupGraph& graph) {
    auto target = boost::target(edge, graph);
    visitedGroups_.insert(graph[target].GetName());
  }

  std::unordered_set<std::string> get_visited_groups() const {
    return visitedGroups_;
  }

private:
  std::unordered_set<std::string>& visitedGroups_;
};

std::string join(const std::unordered_set<std::string>& set) {
  std::string output;
  for (const auto& element : set) {
    output += element + ", ";
  }

  return output.substr(0, output.length() - 2);
}

std::unordered_map<std::string, std::unordered_set<std::string>>
GetTransitiveAfterGroups(const std::unordered_set<Group>& masterlistGroups,
                         const std::unordered_set<Group>& userGroups) {
  GroupGraph graph;

  auto logger = getLogger();
  if (logger) {
    logger->info("Sorting groups according to their load after data");
  }

  std::unordered_map<std::string, vertex_t> groupVertices;
  for (const auto& group : masterlistGroups) {
    auto groupSortingData = GroupSortingData(group.GetName());
    groupSortingData.SetMasterlistAfterGroups(group.GetAfterGroups());

    auto vertex = boost::add_vertex(groupSortingData, graph);
    groupVertices.emplace(group.GetName(), vertex);
  }
  for (const auto& group : userGroups) {
    auto it = groupVertices.find(group.GetName());
    if (it != groupVertices.end()) {
      graph[it->second].SetUserAfterGroups(group.GetAfterGroups());
    } else {
      auto groupSortingData = GroupSortingData(group.GetName());
      groupSortingData.SetUserAfterGroups(group.GetAfterGroups());

      auto vertex = boost::add_vertex(groupSortingData, graph);
      groupVertices.emplace(group.GetName(), vertex);
    }
  }

  for (const vertex_t& vertex :
       boost::make_iterator_range(boost::vertices(graph))) {
    auto group = graph[vertex];

    if (logger) {
      logger->trace(
          "Group \"{}\" directly loads after masterlist groups \"{}\" and user "
          "groups \"{}\"",
          group.GetName(),
          join(group.GetMasterlistAfterGroups()),
          join(group.GetUserAfterGroups()));
    }

    for (const auto& otherGroupName : group.GetMasterlistAfterGroups()) {
      auto otherVertex = groupVertices.find(otherGroupName);
      if (otherVertex == groupVertices.end()) {
        throw UndefinedGroupError(otherGroupName);
      }

      auto vertex = groupVertices[group.GetName()];
      boost::add_edge(
          vertex, otherVertex->second, EdgeType::MasterlistLoadAfter, graph);
    }

    for (const auto& otherGroupName : group.GetUserAfterGroups()) {
      auto otherVertex = groupVertices.find(otherGroupName);
      if (otherVertex == groupVertices.end()) {
        throw UndefinedGroupError(otherGroupName);
      }

      auto vertex = groupVertices[group.GetName()];
      boost::add_edge(
          vertex, otherVertex->second, EdgeType::UserLoadAfter, graph);
    }
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
    transitiveAfterGroups[graph[vertex].GetName()] = visitedGroups;

    if (logger) {
      logger->trace("Group \"{}\" transitively loads after groups \"{}\"",
                    graph[vertex].GetName(),
                    join(visitedGroups));
    }
  }

  return transitiveAfterGroups;
}
}
