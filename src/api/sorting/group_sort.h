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

#ifndef LOOT_API_SORTING_GROUP_SORT
#define LOOT_API_SORTING_GROUP_SORT

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>

#include "loot/exception/cyclic_interaction_error.h"
#include "loot/metadata/group.h"

namespace loot {
// Map entries are a group name and names of transitive load after groups.
std::unordered_map<std::string, std::unordered_set<std::string>>
GetTransitiveAfterGroups(const std::unordered_set<Group>& masterlistGroups,
                         const std::unordered_set<Group>& userGroups);

std::vector<Vertex> GetGroupsPath(
    const std::unordered_set<Group>& masterlistGroups,
    const std::unordered_set<Group>& userGroups,
    const std::string& fromGroupName,
    const std::string& toGroupName);

template<typename G>
class CycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(typename boost::graph_traits<G>::edge_descriptor edge,
                 const G& graph) {
    auto source = boost::source(edge, graph);

    auto vertex = Vertex(graph[source].GetName(), graph[edge]);

    // Check if the vertex already exists in the recorded trail.
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

  void back_edge(typename boost::graph_traits<G>::edge_descriptor edge,
                 const G& graph) {
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
}
#endif
