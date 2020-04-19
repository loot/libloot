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

#include "plugin_sort.h"

#include "api/helpers/logging.h"
#include "api/sorting/plugin_graph.h"

namespace loot {
std::vector<std::string> SortPlugins(
    Game& game,
    const std::vector<std::string>& loadOrder) {
  PluginGraph graph;

  graph.AddPluginVertices(game, loadOrder);

  // If there aren't any vertices, exit early, because sorting assumes
  // there is at least one plugin.
  if (graph.CountVertices() == 0)
    return std::vector<std::string>();

  auto logger = getLogger();
  if (logger) {
    logger->info("Current load order: ");
    for (const auto& plugin : loadOrder) {
      logger->info("\t\t{}", plugin);
    }
  }

  // Now add the interactions between plugins to the graph as edges.
  graph.AddSpecificEdges();
  graph.AddHardcodedPluginEdges(game);

  std::unordered_map<std::string, Group> groups;
  for (const auto& group : game.GetDatabase()->GetGroups()) {
    groups.emplace(group.GetName(), group);
  }
  graph.AddGroupEdges(groups);
  graph.AddOverlapEdges();
  graph.AddTieBreakEdges();

  graph.CheckForCycles();

  return graph.TopologicalSort();
}
}
