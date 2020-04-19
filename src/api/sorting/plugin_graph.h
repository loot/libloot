/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

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

#ifndef LOOT_API_SORTING_PLUGIN_SORTER
#define LOOT_API_SORTING_PLUGIN_SORTER

#define FMT_NO_FMT_STRING_ALIAS

#include <map>

#include <spdlog/spdlog.h>
#include <boost/container_hash/hash.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>

#include "api/game/game.h"
#include "api/plugin.h"
#include "api/sorting/plugin_sorting_data.h"
#include "loot/exception/cyclic_interaction_error.h"

namespace loot {
typedef boost::adjacency_list<boost::listS,
                              boost::listS,
                              boost::bidirectionalS,
                              PluginSortingData,
                              EdgeType>
    RawPluginGraph;
typedef boost::graph_traits<RawPluginGraph>::vertex_descriptor vertex_t;
typedef boost::associative_property_map<std::map<vertex_t, size_t>>
    vertex_map_t;

std::string describeEdgeType(EdgeType edgeType);

struct GraphPath {
  GraphPath(vertex_t from, vertex_t to) : from(from), to(to) {}

  bool operator==(const GraphPath& rhs) const {
    return this->from == rhs.from && this->to == rhs.to;
  }

  vertex_t from;
  vertex_t to;
};
}

namespace std {
template<>
struct hash<loot::GraphPath> {
  size_t operator()(const loot::GraphPath& graphPath) const {
    size_t seed = 0;
    boost::hash_combine(seed, graphPath.from);
    boost::hash_combine(seed, graphPath.to);

    return seed;
  }
};
}

namespace loot {
class PluginGraph {
public:
  size_t CountVertices() const;
  void CheckForCycles() const;
  
  void AddPluginVertices(Game& game, const std::vector<std::string>& loadOrder);
  void AddSpecificEdges();
  void AddHardcodedPluginEdges(Game& game);
  void AddGroupEdges(const std::unordered_map<std::string, Group>& groups);
  void AddOverlapEdges();
  void AddTieBreakEdges();

  std::vector<std::string> TopologicalSort() const;

private:
  std::optional<vertex_t> GetVertexByName(const std::string& name) const;
  bool EdgeCreatesCycle(const vertex_t& u, const vertex_t& v);

  void AddEdge(const vertex_t& fromVertex,
               const vertex_t& toVertex,
               EdgeType edgeType);

  RawPluginGraph graph_;
  std::unordered_set<GraphPath> pathsCache_;
};
}

#endif
