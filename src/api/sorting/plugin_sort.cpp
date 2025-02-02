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

#include <boost/algorithm/string.hpp>

#include "api/helpers/logging.h"
#include "api/sorting/group_sort.h"
#include "api/sorting/plugin_graph.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
std::vector<PluginSortingData> GetPluginsSortingData(
    const DatabaseInterface& db,
    const std::vector<const Plugin*>& loadOrder) {
  std::vector<PluginSortingData> pluginsSortingData;
  pluginsSortingData.reserve(loadOrder.size());

  std::vector<ComparableFilename> comparableLoadOrder;
  for (const auto& plugin : loadOrder) {
    comparableLoadOrder.push_back(ToComparableFilename(plugin->GetName()));
  }

  for (const auto& plugin : loadOrder) {
    const auto pluginFilename = plugin->GetName();

    const auto masterlistMetadata =
        db.GetPluginMetadata(pluginFilename, false, true)
            .value_or(PluginMetadata(pluginFilename));
    const auto userMetadata = db.GetPluginUserMetadata(pluginFilename, true)
                                  .value_or(PluginMetadata(pluginFilename));

    const auto pluginSortingData = PluginSortingData(
        plugin, masterlistMetadata, userMetadata, comparableLoadOrder);

    pluginsSortingData.push_back(pluginSortingData);
  }

  return pluginsSortingData;
}

void ValidatePluginGroups(const std::vector<PluginSortingData>& plugins,
                          const GroupGraph& graph) {
  std::unordered_set<std::string> groupNames;

  for (const vertex_t& vertex :
       boost::make_iterator_range(boost::vertices(graph))) {
    groupNames.insert(graph[vertex]);
  }

  for (const auto& plugin : plugins) {
    const auto pluginGroup = plugin.GetGroup();
    if (groupNames.count(pluginGroup) == 0) {
      throw UndefinedGroupError(pluginGroup);
    }
  }
}

bool IsInRange(const std::vector<PluginSortingData>::const_iterator& begin,
               const std::vector<PluginSortingData>::const_iterator& end,
               const std::string& name) {
  return std::any_of(begin, end, [&](const PluginSortingData& plugin) {
    return CompareFilenames(plugin.GetName(), name) == 0;
  });
}

void ValidateSpecificAndHardcodedEdges(
    const std::vector<PluginSortingData>::const_iterator& begin,
    const std::vector<PluginSortingData>::const_iterator& firstBlueprintMaster,
    const std::vector<PluginSortingData>::const_iterator& firstNonMaster,
    const std::vector<PluginSortingData>::const_iterator& end,
    const std::vector<std::string>& hardcodedPlugins) {
  const auto logger = getLogger();

  const auto isNonMaster = [&](const std::string& name) {
    return IsInRange(firstNonMaster, end, name);
  };
  const auto isBlueprintMaster = [&](const std::string& name) {
    return IsInRange(firstBlueprintMaster, firstNonMaster, name);
  };

  for (auto it = begin; it != firstBlueprintMaster; ++it) {
    for (const auto& master : it->GetMasters()) {
      if (isNonMaster(master)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(master, EdgeType::master),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }

      if (isBlueprintMaster(master) && logger) {
        // Log a warning instead of throwing an exception because the game will
        // just ignore this master, and the issue can't be fixed without
        // editing the plugin and the blueprint master may not actually have
        // any of its records overridden.
        logger->warn(
            "The master plugin \"{}\" has the blueprint master \"{}\" as one "
            "of its masters",
            it->GetName(),
            master);
      }
    }

    for (const auto& file : it->GetMasterlistRequirements()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::masterlistRequirement),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }

      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::masterlistRequirement),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }

    for (const auto& file : it->GetUserRequirements()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::userRequirement),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }

      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::userRequirement),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }

    for (const auto& file : it->GetMasterlistLoadAfterFiles()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::masterlistLoadAfter),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }

      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::masterlistLoadAfter),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }

    for (const auto& file : it->GetUserLoadAfterFiles()) {
      const auto name = std::string(file.GetName());
      if (isNonMaster(name)) {
        throw CyclicInteractionError(
            std::vector<Vertex>{Vertex(name, EdgeType::userLoadAfter),
                                Vertex(it->GetName(), EdgeType::masterFlag)});
      }

      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::userLoadAfter),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }
  }

  for (auto it = firstNonMaster; it != end; ++it) {
    for (const auto& master : it->GetMasters()) {
      if (isBlueprintMaster(master) && logger) {
        // Log a warning instead of throwing an exception because the game will
        // just ignore this master, and the issue can't be fixed without
        // editing the plugin and the blueprint master may not actually have
        // any of its records overridden.
        logger->warn(
            "The non-master plugin \"{}\" has the blueprint master \"{}\" as "
            "one of its masters",
            it->GetName(),
            master);
      }
    }

    for (const auto& file : it->GetMasterlistRequirements()) {
      const auto name = std::string(file.GetName());
      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::masterlistRequirement),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }

    for (const auto& file : it->GetUserRequirements()) {
      const auto name = std::string(file.GetName());
      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::userRequirement),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }

    for (const auto& file : it->GetMasterlistLoadAfterFiles()) {
      const auto name = std::string(file.GetName());
      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::masterlistLoadAfter),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }

    for (const auto& file : it->GetUserLoadAfterFiles()) {
      const auto name = std::string(file.GetName());
      if (isBlueprintMaster(name)) {
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(name, EdgeType::userLoadAfter),
            Vertex(it->GetName(), EdgeType::blueprintMaster)});
      }
    }
  }

  if (begin != firstNonMaster) {
    // There's at least one master, check that there are no hardcoded
    // non-masters.
    for (const auto& plugin : hardcodedPlugins) {
      if (isNonMaster(plugin)) {
        // Just report the cycle to the first master.
        throw CyclicInteractionError(std::vector<Vertex>{
            Vertex(plugin, EdgeType::hardcoded),
            Vertex(begin->GetName(), EdgeType::masterFlag)});
      }
    }
  }
}

std::vector<std::string> SortPlugins(
    const std::vector<PluginSortingData>::const_iterator& begin,
    const std::vector<PluginSortingData>::const_iterator& end,
    const std::vector<std::string>& hardcodedPlugins,
    const GroupGraph& groupGraph) {
  PluginGraph graph;

  for (auto it = begin; it != end; ++it) {
    graph.AddVertex(*it);
  }

  // Now add the interactions between plugins to the graph as edges.
  graph.AddSpecificEdges();
  graph.AddHardcodedPluginEdges(hardcodedPlugins);

  // Check for cycles now because from this point on edges are only added if
  // they don't cause cycles, and adding overlap and tie-break edges is
  // relatively slow, so checking now provides quicker feedback if there is an
  // issue.
  graph.CheckForCycles();

  graph.AddGroupEdges(groupGraph);
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
    std::vector<PluginSortingData>&& pluginsSortingData,
    const std::vector<Group>& masterlistGroups,
    const std::vector<Group>& userGroups,
    const std::vector<std::string>& earlyLoadingPlugins) {
  // If there aren't any plugins, exit early, because sorting assumes
  // there is at least one plugin.
  if (pluginsSortingData.empty()) {
    return {};
  }

  // Sort the plugins according to the lexicographical order of their names.
  // This ensures a consistent iteration order for vertices given the same input
  // data. The vertex iteration order can affect what edges get added and so
  // the final sorting result, so consistency is important.
  // This order needs to be independent of any state (e.g. the current load
  // order) so that sorting and applying the result doesn't then produce a
  // different result if you then sort again.
  std::sort(pluginsSortingData.begin(),
            pluginsSortingData.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.GetName() < rhs.GetName();
            });

  const auto groupGraph = BuildGroupGraph(masterlistGroups, userGroups);
  ValidatePluginGroups(pluginsSortingData, groupGraph);

  // Some parts of sorting are O(N^2) for N plugins, and master flags cause
  // O(M*N) edges to be added for M masters and N non-masters, which can be
  // two thirds of all edges added. The cost of each bidirectional search
  // scales with the number of edges, so reducing edges makes searches
  // faster.
  // Similarly, blueprint plugins load after all others.
  // As such, sort plugins using three separate graphs for masters,
  // non-masters and blueprint plugins. This means that any edges that go from a
  // non-master to a master are effectively ignored, so won't cause cyclic
  // interaction errors. Edges going the other way will also effectively be
  // ignored, but that shouldn't have a noticeable impact.
  const auto firstNonMasterIt = std::stable_partition(
      pluginsSortingData.begin(),
      pluginsSortingData.end(),
      [](const PluginSortingData& plugin) { return plugin.IsMaster(); });

  const auto firstBlueprintPluginIt =
      std::stable_partition(pluginsSortingData.begin(),
                            firstNonMasterIt,
                            [](const PluginSortingData& plugin) {
                              return !plugin.IsBlueprintMaster();
                            });

  ValidateSpecificAndHardcodedEdges(pluginsSortingData.begin(),
                                    firstBlueprintPluginIt,
                                    firstNonMasterIt,
                                    pluginsSortingData.end(),
                                    earlyLoadingPlugins);

  auto newMastersLoadOrder = SortPlugins(pluginsSortingData.begin(),
                                         firstBlueprintPluginIt,
                                         earlyLoadingPlugins,
                                         groupGraph);

  const auto newBlueprintMastersLoadOrder = SortPlugins(firstBlueprintPluginIt,
                                                        firstNonMasterIt,
                                                        earlyLoadingPlugins,
                                                        groupGraph);

  const auto newNonMastersLoadOrder = SortPlugins(firstNonMasterIt,
                                                  pluginsSortingData.end(),
                                                  earlyLoadingPlugins,
                                                  groupGraph);

  newMastersLoadOrder.insert(newMastersLoadOrder.end(),
                             newNonMastersLoadOrder.begin(),
                             newNonMastersLoadOrder.end());
  newMastersLoadOrder.insert(newMastersLoadOrder.end(),
                             newBlueprintMastersLoadOrder.begin(),
                             newBlueprintMastersLoadOrder.end());

  return newMastersLoadOrder;
}

std::vector<std::string> SortPlugins(
    Game& game,
    const std::vector<std::string>& loadOrder) {
  std::vector<const Plugin*> plugins;
  for (const auto& pluginFilename : loadOrder) {
    const auto plugin = game.GetCache().GetPlugin(pluginFilename);
    if (plugin == nullptr) {
      throw std::invalid_argument("The plugin \"" + pluginFilename +
                                  "\" has not been loaded.");
    }

    plugins.push_back(plugin);
  }

  auto pluginsSortingData = GetPluginsSortingData(game.GetDatabase(), plugins);

  const auto logger = getLogger();
  if (logger) {
    logger->debug("Current load order:");
    for (const auto& plugin : loadOrder) {
      logger->debug("\t{}", plugin);
    }
  }

  const auto newLoadOrder =
      SortPlugins(std::move(pluginsSortingData),
                  game.GetDatabase().GetGroups(false),
                  game.GetDatabase().GetUserGroups(),
                  game.GetLoadOrderHandler().GetEarlyLoadingPlugins());

  if (logger) {
    logger->debug("Calculated order:");
    for (const auto& name : newLoadOrder) {
      logger->debug("\t{}", name);
    }
  }

  return newLoadOrder;
}
}
