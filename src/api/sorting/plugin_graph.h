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

#include <spdlog/spdlog.h>

#include <boost/container_hash/hash.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <map>

#include "api/game/game.h"
#include "api/plugin.h"
#include "api/sorting/plugin_sorting_data.h"
#include "loot/exception/cyclic_interaction_error.h"

namespace loot {
typedef boost::adjacency_list<boost::vecS,
                              boost::listS,
                              boost::bidirectionalS,
                              PluginSortingData,
                              EdgeType>
    RawPluginGraph;
typedef boost::graph_traits<RawPluginGraph>::vertex_descriptor vertex_t;
typedef boost::associative_property_map<std::map<vertex_t, size_t>>
    vertex_map_t;

std::string describeEdgeType(EdgeType edgeType);

class PathsCache {
public:
  bool IsPathCached(const vertex_t& fromVertex, const vertex_t& toVertex) const;
  void CachePath(const vertex_t& fromVertex, const vertex_t& toVertex);

private:
  std::unordered_map<vertex_t, std::unordered_set<vertex_t>> pathsCache_;
};

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
  bool PathExists(const vertex_t& fromVertex, const vertex_t& toVertex);

  void AddEdge(const vertex_t& fromVertex,
               const vertex_t& toVertex,
               EdgeType edgeType);

  RawPluginGraph graph_;
  PathsCache pathsCache_;
};
}

#endif
