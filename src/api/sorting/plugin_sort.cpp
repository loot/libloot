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
#include "api/sorting/group_sort.h"
#include "api/sorting/plugin_graph.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
int ComparePlugins(const PluginSortingData& plugin1,
                   const PluginSortingData& plugin2) {
  if (plugin1.GetLoadOrderIndex().has_value() &&
      !plugin2.GetLoadOrderIndex().has_value()) {
    return -1;
  }

  if (!plugin1.GetLoadOrderIndex().has_value() &&
      plugin2.GetLoadOrderIndex().has_value()) {
    return 1;
  }

  if (plugin1.GetLoadOrderIndex().has_value() &&
      plugin2.GetLoadOrderIndex().has_value()) {
    if (plugin1.GetLoadOrderIndex().value() <
        plugin2.GetLoadOrderIndex().value()) {
      return -1;
    } else {
      return 1;
    }
  }

  // Neither plugin has a load order position. Compare plugin basenames to
  // get an ordering.
  const auto name1 = plugin1.GetName();
  const auto name2 = plugin2.GetName();
  const auto basename1 = name1.substr(0, name1.length() - 4);
  const auto basename2 = name2.substr(0, name2.length() - 4);

  const int result = CompareFilenames(basename1, basename2);

  if (result != 0) {
    return result;
  } else {
    // Could be a .esp and .esm plugin with the same basename,
    // compare their extensions.
    const auto ext1 = name1.substr(name1.length() - 4);
    const auto ext2 = name2.substr(name2.length() - 4);
    return CompareFilenames(ext1, ext2);
  }
}

std::vector<PluginSortingData> GetPluginsSortingData(
    const Game& game,
    const std::vector<std::string>& loadOrder) {
  auto loadedPlugins = game.GetCache().GetPlugins();

  // Sort plugins by their names. This is necessary to ensure that
  // plugin precedessor group plugins are listed in a consistent
  // order, which is important because that is the order in which
  // group edges are added and differences could cause different
  // sorting results.
  std::sort(loadedPlugins.begin(),
            loadedPlugins.end(),
            [](const auto& lhs, const auto& rhs) {
              if (!lhs) {
                return false;
              }

              if (!rhs) {
                return true;
              }

              return lhs->GetName() < rhs->GetName();
            });

  std::vector<const PluginInterface*> loadedPluginInterfaces;
  std::transform(loadedPlugins.begin(),
                 loadedPlugins.end(),
                 std::back_inserter(loadedPluginInterfaces),
                 [](const Plugin* plugin) { return plugin; });

  std::vector<PluginSortingData> pluginsSortingData;
  pluginsSortingData.reserve(loadedPlugins.size());

  for (const auto& plugin : loadedPlugins) {
    if (!plugin) {
      continue;
    }

    const auto masterlistMetadata =
        game.GetDatabase()
            .GetPluginMetadata(plugin->GetName(), false, true)
            .value_or(PluginMetadata(plugin->GetName()));
    const auto userMetadata =
        game.GetDatabase()
            .GetPluginUserMetadata(plugin->GetName(), true)
            .value_or(PluginMetadata(plugin->GetName()));

    const auto pluginSortingData = PluginSortingData(plugin,
                                                     masterlistMetadata,
                                                     userMetadata,
                                                     loadOrder,
                                                     game.Type(),
                                                     loadedPluginInterfaces);

    pluginsSortingData.push_back(pluginSortingData);
  }

  std::unordered_map<std::string, std::vector<std::string>> groupPlugins;
  for (const auto& plugin : pluginsSortingData) {
    const auto groupName = plugin.GetGroup();
    const auto groupIt = groupPlugins.find(groupName);
    if (groupIt == groupPlugins.end()) {
      groupPlugins.emplace(groupName,
                           std::vector<std::string>({plugin.GetName()}));
    } else {
      groupIt->second.push_back(plugin.GetName());
    }
  }

  // Map sets of transitive group dependencies to sets of transitive plugin
  // dependencies.
  auto groups = GetTransitiveAfterGroups(game.GetDatabase().GetGroups(false),
                                         game.GetDatabase().GetUserGroups());
  for (auto& group : groups) {
    std::unordered_set<std::string> transitivePlugins;
    for (const auto& afterGroup : group.second) {
      const auto pluginsIt = groupPlugins.find(afterGroup);
      if (pluginsIt != groupPlugins.end()) {
        transitivePlugins.insert(pluginsIt->second.begin(),
                                 pluginsIt->second.end());
      }
    }
    group.second = transitivePlugins;
  }

  // Add all transitive plugin dependencies for a group to the plugin's load
  // after metadata.
  const auto logger = getLogger();
  for (auto& plugin : pluginsSortingData) {
    if (logger) {
      logger->trace(
          "Plugin \"{}\" belongs to group \"{}\", setting after group plugins",
          plugin.GetName(),
          plugin.GetGroup());
    }

    const auto groupsIt = groups.find(plugin.GetGroup());
    if (groupsIt == groups.end()) {
      throw UndefinedGroupError(plugin.GetGroup());
    } else {
      plugin.SetAfterGroupPlugins(groupsIt->second);
    }
  }

  // Sort the plugins according to into their existing load order, or
  // lexicographical ordering for pairs of plugins without load order positions.
  // This ensures a consistent iteration order for vertices given the same input
  // data. The vertex iteration order can affect what edges get added and so
  // the final sorting result, so consistency is important.
  // Load order is used because this simplifies the logic when adding tie-break
  // edges.
  std::sort(pluginsSortingData.begin(),
            pluginsSortingData.end(),
            [](const auto& lhs, const auto& rhs) {
              return ComparePlugins(lhs, rhs) < 0;
            });

  return pluginsSortingData;
}

std::vector<std::string> SortPluginGraph(PluginGraph& graph, const Game& game) {
  // Now add the interactions between plugins to the graph as edges.
  graph.AddSpecificEdges();
  graph.AddHardcodedPluginEdges(game);

  std::unordered_map<std::string, Group> groups;
  for (const auto& group : game.GetDatabase().GetGroups()) {
    groups.emplace(group.GetName(), group);
  }
  graph.AddGroupEdges(groups);

  // Check for cycles now because from this point on edges are only added if
  // they don't cause cycles, and adding tie-break edges is by far the slowest
  // part of the process, so if there is a cycle checking now will provide
  // quicker feedback than checking later.
  graph.CheckForCycles();

  graph.AddOverlapEdges();
  graph.AddTieBreakEdges();

  // Check for cycles again, just in case there's a bug that lets some occur.
  // The check doesn't take a significant amount of time.
  graph.CheckForCycles();

  const auto path = graph.TopologicalSort();

  const auto result = graph.IsHamiltonianPath(path);
  const auto logger = getLogger();
  if (result.has_value() && logger) {
    logger->error("The path is not unique. No edge exists between {} and {}.",
                  graph.GetPlugin(result.value().first).GetName(),
                  graph.GetPlugin(result.value().second).GetName());
  }

  // Output a plugin list using the sorted vertices.
  return graph.ToPluginNames(path);
}

std::vector<std::string> SortPlugins(
    const Game& game,
    const std::vector<std::string>& loadOrder) {
  auto pluginsSortingData = GetPluginsSortingData(game, loadOrder);

  // If there aren't any plugins, exit early, because sorting assumes
  // there is at least one plugin.
  if (pluginsSortingData.empty()) {
    return {};
  }

  const auto logger = getLogger();
  if (logger) {
    logger->debug("Current load order:");
    for (const auto& plugin : loadOrder) {
      logger->debug("\t{}", plugin);
    }
  }

  const auto firstNonMasterIt = std::stable_partition(
      pluginsSortingData.begin(),
      pluginsSortingData.end(),
      [](const PluginSortingData& plugin) { return plugin.IsMaster(); });

  // Some parts of sorting are O(N^2) for N plugins, and master flags cause
  // O(M*N) edges to be added for M masters and N non-masters, which can be
  // two thirds of all edges added. The cost of each bidirectional search
  // scales with the number of edges, so reducing edges makes searches
  // faster.
  // As such, sort plugins using two separate graphs for masters and
  // non-masters. This means that any edges that go from a non-master to a
  // master are effectively ignored, so won't cause cyclic interaction errors.
  // Edges going the other way will also effectively be ignored, but that
  // shouldn't have a noticeable impact.
  PluginGraph mastersGraph;
  PluginGraph nonMastersGraph;

  for (auto it = pluginsSortingData.begin(); it != firstNonMasterIt; ++it) {
    mastersGraph.AddVertex(*it);
  }

  for (auto it = firstNonMasterIt; it != pluginsSortingData.end(); ++it) {
    nonMastersGraph.AddVertex(*it);
  }

  auto newLoadOrder = SortPluginGraph(mastersGraph, game);
  const auto newNonMastersLoadOrder = SortPluginGraph(nonMastersGraph, game);

  newLoadOrder.insert(newLoadOrder.end(),
                      newNonMastersLoadOrder.begin(),
                      newNonMastersLoadOrder.end());

  if (logger) {
    logger->debug("Calculated order:");
    for (const auto& name : newLoadOrder) {
      logger->debug("\t{}", name);
    }
  }

  return newLoadOrder;
}
}
