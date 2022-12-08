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

#include "plugin_graph.h"

#include <boost/algorithm/string.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/topological_sort.hpp>
#include <cstdlib>
#include <queue>

#include "api/game/game.h"
#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "api/metadata/condition_evaluator.h"
#include "api/sorting/group_sort.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

using std::string;
using std::vector;

namespace loot {
typedef boost::graph_traits<RawPluginGraph>::edge_descriptor edge_t;
typedef boost::graph_traits<RawPluginGraph>::edge_iterator edge_it;
typedef boost::associative_property_map<std::map<vertex_t, size_t>>
    vertex_map_t;

class CycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(edge_t edge, const RawPluginGraph& graph) {
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

  void back_edge(edge_t edge, const RawPluginGraph& graph) {
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

bool ShouldIgnorePlugin(
    const std::string& group,
    const std::string& pluginName,
    const std::map<std::string, std::unordered_set<std::string>>&
        groupPluginsToIgnore) {
  const auto pluginsToIgnore = groupPluginsToIgnore.find(group);
  if (pluginsToIgnore != groupPluginsToIgnore.end()) {
    return pluginsToIgnore->second.count(pluginName) > 0;
  }

  return false;
}

bool ShouldIgnoreGroupEdge(
    const PluginSortingData& fromPlugin,
    const PluginSortingData& toPlugin,
    const std::map<std::string, std::unordered_set<std::string>>&
        groupPluginsToIgnore) {
  return ShouldIgnorePlugin(
             fromPlugin.GetGroup(), toPlugin.GetName(), groupPluginsToIgnore) ||
         ShouldIgnorePlugin(
             toPlugin.GetGroup(), fromPlugin.GetName(), groupPluginsToIgnore);
}

void IgnorePluginGroupEdges(
    const std::string& pluginName,
    const std::unordered_set<std::string>& groups,
    std::map<std::string, std::unordered_set<std::string>>&
        groupPluginsToIgnore) {
  for (const auto& group : groups) {
    const auto pluginsToIgnore = groupPluginsToIgnore.find(group);
    if (pluginsToIgnore != groupPluginsToIgnore.end()) {
      pluginsToIgnore->second.insert(pluginName);
    } else {
      groupPluginsToIgnore.emplace(
          group, std::unordered_set<std::string>({pluginName}));
    }
  }
}

// Look for paths to targetGroupName from group. Don't pass visitedGroups by
// reference as each after group should be able to record paths independently.
std::unordered_set<std::string> FindGroupsInAllPaths(
    const Group& group,
    const std::string& targetGroupName,
    const std::unordered_map<std::string, Group>& groups,
    std::unordered_set<std::string> visitedGroups) {
  // If the current group is the target group, return the set of groups in the
  // path leading to it.
  if (group.GetName() == targetGroupName) {
    return visitedGroups;
  }

  if (group.GetAfterGroups().empty()) {
    return std::unordered_set<std::string>();
  }

  visitedGroups.insert(group.GetName());

  // Recurse on each after group. We want to find all paths, so merge
  // all return values.
  std::unordered_set<std::string> mergedVisitedGroups;
  for (const auto& afterGroupName : group.GetAfterGroups()) {
    const auto groupIt = groups.find(afterGroupName);
    if (groupIt == groups.end()) {
      throw std::runtime_error("Cannot find group \"" + afterGroupName +
                               "\" during sorting.");
    }

    const auto recursedVisitedGroups = FindGroupsInAllPaths(
        groupIt->second, targetGroupName, groups, visitedGroups);

    mergedVisitedGroups.insert(recursedVisitedGroups.begin(),
                               recursedVisitedGroups.end());
  }

  // Return mergedVisitedGroups if it is empty, to indicate the current group's
  // after groups had no path to the target group.
  if (mergedVisitedGroups.empty()) {
    return mergedVisitedGroups;
  }

  // If any after groups had paths to the target group, mergedVisitedGroups
  // will be non-empty. To ensure that it contains full paths, merge it
  // with visitedGroups and return that merged set.
  visitedGroups.insert(mergedVisitedGroups.begin(), mergedVisitedGroups.end());

  return visitedGroups;
}

std::unordered_set<std::string> FindGroupsInAllPaths(
    const std::unordered_map<std::string, Group>& groups,
    const std::string& firstGroupName,
    const std::string& lastGroupName) {
  // Groups are linked in reverse order, i.e. firstGroup can be found from
  // lastGroup, but not the other way around.
  const auto groupIt = groups.find(lastGroupName);
  if (groupIt == groups.end()) {
    throw std::runtime_error("Cannot find group \"" + lastGroupName +
                             "\" during sorting.");
  }

  auto groupsInPaths = FindGroupsInAllPaths(groupIt->second,
                                            firstGroupName,
                                            groups,
                                            std::unordered_set<std::string>());

  groupsInPaths.erase(lastGroupName);

  return groupsInPaths;
}

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

std::string describeEdgeType(EdgeType edgeType) {
  switch (edgeType) {
    case EdgeType::hardcoded:
      return "Hardcoded";
    case EdgeType::masterFlag:
      return "Master Flag";
    case EdgeType::master:
      return "Master";
    case EdgeType::masterlistRequirement:
      return "Masterlist Requirement";
    case EdgeType::userRequirement:
      return "User Requirement";
    case EdgeType::masterlistLoadAfter:
      return "Masterlist Load After";
    case EdgeType::userLoadAfter:
      return "User Load After";
    case EdgeType::group:
      return "Group";
    case EdgeType::overlap:
      return "Overlap";
    case EdgeType::tieBreak:
      return "Tie Break";
    default:
      return "Unknown";
  }
}

bool PathsCache::IsPathCached(const vertex_t& fromVertex,
                              const vertex_t& toVertex) const {
  const auto descendents = pathsCache_.find(fromVertex);

  if (descendents == pathsCache_.end()) {
    return false;
  }

  return descendents->second.count(toVertex);
}

void PathsCache::CachePath(const vertex_t& fromVertex,
                           const vertex_t& toVertex) {
  auto descendents = pathsCache_.find(fromVertex);

  if (descendents == pathsCache_.end()) {
    pathsCache_.emplace(fromVertex, std::unordered_set<vertex_t>({toVertex}));
  } else {
    descendents->second.insert(toVertex);
  }
}

size_t PluginGraph::CountVertices() const {
  return boost::num_vertices(graph_);
}

std::pair<vertex_it, vertex_it> PluginGraph::GetVertices() const {
  return boost::vertices(graph_);
}

std::optional<vertex_t> PluginGraph::GetVertexByName(
    const std::string& name) const {
  for (const auto& vertex : boost::make_iterator_range(GetVertices())) {
    if (CompareFilenames(GetPlugin(vertex).GetName(), name) == 0) {
      return vertex;
    }
  }

  return std::nullopt;
}

const PluginSortingData& PluginGraph::GetPlugin(const vertex_t& vertex) const {
  return graph_[vertex];
}

void PluginGraph::CheckForCycles() const {
  auto logger = getLogger();
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
  if (logger) {
    logger->trace("Checking plugin graph for cycles...");
  }

  std::map<vertex_t, size_t> indexMap;
  const auto vertexIndexMap = vertex_map_t(indexMap);
  size_t i = 0;
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
  BGL_FORALL_VERTICES(v, graph_, RawPluginGraph) {
    put(vertexIndexMap, v, i++);
  }

  boost::depth_first_search(
      graph_, visitor(CycleDetector()).vertex_index_map(vertexIndexMap));
}

std::vector<std::string> PluginGraph::TopologicalSort() const {
  // Build an index map, which std::list-based VertexList graphs don't have.
  std::map<vertex_t, size_t> indexMap;
  const auto vertexIndexMap = vertex_map_t(indexMap);
  size_t i = 0;
  BGL_FORALL_VERTICES(v, graph_, RawPluginGraph) {
    put(vertexIndexMap, v, i++);
  }

  std::list<vertex_t> sortedVertices;
  auto logger = getLogger();
  if (logger) {
    logger->trace("Performing topological sort on plugin graph...");
  }
  boost::topological_sort(graph_,
                          std::front_inserter(sortedVertices),
                          boost::vertex_index_map(vertexIndexMap));

  // Check that the sorted path is Hamiltonian (ie. unique).
  if (logger) {
    logger->trace("Checking uniqueness of calculated load order...");
  }
  for (auto it = sortedVertices.begin(); it != sortedVertices.end(); ++it) {
    if (next(it) != sortedVertices.end() &&
        !boost::edge(*it, *next(it), graph_).second && logger) {
      logger->error(
          "The calculated load order is not unique. No edge exists between {} "
          "and {}.",
          GetPlugin(*it).GetName(),
          GetPlugin(*next(it)).GetName());
    }
  }

  // Output a plugin list using the sorted vertices.
  if (logger) {
    logger->info("Calculated order: ");
  }
  vector<std::string> plugins;
  for (const auto& vertex : sortedVertices) {
    plugins.push_back(GetPlugin(vertex).GetName());
    if (logger) {
      logger->info("\t{}", plugins.back());
    }
  }

  return plugins;
}

bool PluginGraph::EdgeExists(const vertex_t& fromVertex,
                             const vertex_t& toVertex) {
  return boost::edge(fromVertex, toVertex, graph_).second;
}

bool PluginGraph::PathExists(const vertex_t& fromVertex,
                             const vertex_t& toVertex) {
  if (pathsCache_.IsPathCached(fromVertex, toVertex)) {
    return true;
  }

  std::queue<vertex_t> forwardQueue;
  std::queue<vertex_t> reverseQueue;
  std::unordered_set<vertex_t> forwardVisited;
  std::unordered_set<vertex_t> reverseVisited;

  forwardQueue.push(fromVertex);
  forwardVisited.insert(fromVertex);
  reverseQueue.push(toVertex);
  reverseVisited.insert(toVertex);

  while (!forwardQueue.empty() && !reverseQueue.empty()) {
    if (!forwardQueue.empty()) {
      const auto v = forwardQueue.front();
      forwardQueue.pop();
      if (v == toVertex || reverseVisited.count(v) > 0) {
        return true;
      }
      for (const auto adjacentV :
           boost::make_iterator_range(boost::adjacent_vertices(v, graph_))) {
        if (forwardVisited.count(adjacentV) == 0) {
          pathsCache_.CachePath(fromVertex, adjacentV);

          forwardVisited.insert(adjacentV);
          forwardQueue.push(adjacentV);
        }
      }
    }
    if (!reverseQueue.empty()) {
      const auto v = reverseQueue.front();
      reverseQueue.pop();
      if (v == fromVertex || forwardVisited.count(v) > 0) {
        return true;
      }
      for (const auto adjacentV : boost::make_iterator_range(
               boost::inv_adjacent_vertices(v, graph_))) {
        if (reverseVisited.count(adjacentV) == 0) {
          pathsCache_.CachePath(adjacentV, toVertex);

          reverseVisited.insert(adjacentV);
          reverseQueue.push(adjacentV);
        }
      }
    }
  }

  return false;
}

void PluginGraph::AddEdge(const vertex_t& fromVertex,
                          const vertex_t& toVertex,
                          EdgeType edgeType) {
  if (pathsCache_.IsPathCached(fromVertex, toVertex)) {
    return;
  }

  auto logger = getLogger();
  if (logger) {
    logger->trace("Adding {} edge from \"{}\" to \"{}\".",
                  describeEdgeType(edgeType),
                  GetPlugin(fromVertex).GetName(),
                  GetPlugin(toVertex).GetName());
  }

  boost::add_edge(fromVertex, toVertex, edgeType, graph_);
  pathsCache_.CachePath(fromVertex, toVertex);
}

void PluginGraph::AddVertex(const PluginSortingData& plugin) {
  boost::add_vertex(plugin, graph_);
}

void PluginGraph::AddPluginVertices(const Game& game,
                                    const std::vector<std::string>& loadOrder) {
  // The resolution of tie-breaks in the plugin graph may be dependent
  // on the order in which vertices are iterated over, as an earlier tie
  // break resolution may cause a potential later tie break to instead
  // cause a cycle. Vertices are stored in a std::list and added to the
  // list using push_back().
  // Plugins are stored in an unordered map, so simply iterating over
  // its elements is not guarunteed to produce a consistent vertex order.
  // MSVC 2013 and GCC 5.0 have been shown to produce consistent
  // iteration orders that differ, and while MSVC 2013's order seems to
  // be independent on the order in which the unordered map was filled
  // (being lexicographical), GCC 5.0's unordered map iteration order is
  // dependent on its insertion order.
  // Given that, the order of vertex creation should be made consistent
  // in order to produce consistent sorting results. While MSVC 2013
  // doesn't strictly need this, there is no guarantee that this
  // unspecified behaviour will remain in future compiler updates, so
  // implement it generally.
  auto loadedPlugins = game.GetCache().GetPlugins();
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
  for (const auto& plugin : loadedPlugins) {
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
  auto logger = getLogger();
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

  for (const auto& plugin : pluginsSortingData) {
    AddVertex(plugin);
  }
}

void PluginGraph::AddSpecificEdges() {
  // Add edges for all relationships that aren't overlaps.
  for (auto [vit, vitend] = GetVertices(); vit != vitend; ++vit) {
    const auto& vertex = *vit;
    const auto& plugin = GetPlugin(vertex);

    for (vertex_it vit2 = vit; vit2 != vitend; ++vit2) {
      const auto& otherVertex = *vit2;
      const auto& otherPlugin = GetPlugin(otherVertex);

      if (plugin.IsMaster() == otherPlugin.IsMaster())
        continue;

      const auto isOtherPluginAMaster = otherPlugin.IsMaster();
      vertex_t childVertex = isOtherPluginAMaster ? vertex : otherVertex;
      vertex_t parentVertex = isOtherPluginAMaster ? otherVertex : vertex;

      AddEdge(parentVertex, childVertex, EdgeType::masterFlag);
    }

    for (const auto& master : plugin.GetMasters()) {
      auto parentVertex = GetVertexByName(master);
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::master);
      }
    }

    for (const auto& file : plugin.GetMasterlistRequirements()) {
      auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::masterlistRequirement);
      }
    }
    for (const auto& file : plugin.GetUserRequirements()) {
      auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::userRequirement);
      }
    }

    for (const auto& file : plugin.GetMasterlistLoadAfterFiles()) {
      auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::masterlistLoadAfter);
      }
    }
    for (const auto& file : plugin.GetUserLoadAfterFiles()) {
      auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::userLoadAfter);
      }
    }
  }
}

void PluginGraph::AddHardcodedPluginEdges(const Game& game) {
  using std::filesystem::u8path;

  auto implicitlyActivePlugins =
      game.GetLoadOrderHandler().GetImplicitlyActivePlugins();

  auto logger = getLogger();
  std::set<std::string> processedPluginPaths;
  for (const auto& plugin : implicitlyActivePlugins) {
    processedPluginPaths.insert(NormalizeFilename(plugin));

    if (game.Type() == GameType::tes5 &&
        loot::equivalent(game.DataPath() / u8path(plugin),
                         game.DataPath() / "update.esm")) {
      if (logger) {
        logger->trace(
            "Skipping adding hardcoded plugin edges for Update.esm as it does "
            "not have a hardcoded position for Skyrim.");
        continue;
      }
    }

    auto pluginVertex = GetVertexByName(plugin);

    if (!pluginVertex.has_value()) {
      if (logger) {
        logger->trace(
            "Skipping adding hardcoded plugin edges for \"{}\" as it has not "
            "been loaded.",
            plugin);
      }
      continue;
    }

    for (const auto& vertex : boost::make_iterator_range(GetVertices())) {
      if (vertex == pluginVertex.value()) {
        continue;
      }

      if (processedPluginPaths.count(
              NormalizeFilename(GetPlugin(vertex).GetName())) == 0) {
        AddEdge(pluginVertex.value(), vertex, EdgeType::hardcoded);
      }
    }
  }
}

void PluginGraph::AddGroupEdges(
    const std::unordered_map<std::string, Group>& groups) {
  std::vector<std::pair<vertex_t, vertex_t>> acyclicEdgePairs;
  std::map<std::string, std::unordered_set<std::string>> groupPluginsToIgnore;

  const auto logger = getLogger();
  for (const vertex_t& vertex : boost::make_iterator_range(GetVertices())) {
    const auto& toPlugin = GetPlugin(vertex);

    for (const auto& pluginName : toPlugin.GetAfterGroupPlugins()) {
      const auto parentVertex = GetVertexByName(pluginName);
      if (!parentVertex.has_value()) {
        continue;
      }

      if (PathExists(vertex, parentVertex.value())) {
        const auto& fromPlugin = GetPlugin(parentVertex.value());

        if (logger) {
          logger->trace(
              "Skipping group edge from \"{}\" to \"{}\" as it would "
              "create a cycle.",
              fromPlugin.GetName(),
              toPlugin.GetName());
        }

        // If the earlier plugin is not a master and the later plugin is,
        // don't ignore the plugin with the default group for all
        // intermediate plugins, as some of those plugins may be masters
        // that wouldn't be involved in the cycle, and any of those
        // plugins that are not masters would have their own cycles
        // detected anyway.
        if (!fromPlugin.IsMaster() && toPlugin.IsMaster()) {
          continue;
        }

        // The default group is a special case, as it's given to plugins
        // with no metadata. If a plugin in the default group causes
        // a cycle due to its group, ignore that plugin's group for all
        // groups in the group graph paths between default and the other
        // plugin's group.
        std::string pluginToIgnore;
        if (toPlugin.GetGroup() == Group().GetName()) {
          pluginToIgnore = toPlugin.GetName();
        } else if (fromPlugin.GetGroup() == Group().GetName()) {
          pluginToIgnore = fromPlugin.GetName();
        } else {
          // If neither plugin is in the default group, it's impossible
          // to decide which group to ignore, so ignore neither of them.
          continue;
        }

        const auto groupsInPaths = FindGroupsInAllPaths(
            groups, fromPlugin.GetGroup(), toPlugin.GetGroup());

        IgnorePluginGroupEdges(
            pluginToIgnore, groupsInPaths, groupPluginsToIgnore);

        continue;
      }

      acyclicEdgePairs.push_back(std::make_pair(parentVertex.value(), vertex));
    }
  }

  for (const auto& edgePair : acyclicEdgePairs) {
    const auto& fromPlugin = GetPlugin(edgePair.first);
    const auto& toPlugin = GetPlugin(edgePair.second);
    const bool ignore =
        ShouldIgnoreGroupEdge(fromPlugin, toPlugin, groupPluginsToIgnore);

    if (!ignore) {
      AddEdge(edgePair.first, edgePair.second, EdgeType::group);
    } else if (logger) {
      logger->trace(
          "Skipping group edge from \"{}\" to \"{}\" as it would "
          "create a multi-group cycle.",
          fromPlugin.GetName(),
          toPlugin.GetName());
    }
  }
}

void PluginGraph::AddOverlapEdges() {
  const auto logger = getLogger();
  for (auto [vit, vitend] = GetVertices(); vit != vitend; ++vit) {
    const vertex_t vertex = *vit;
    const auto& plugin = GetPlugin(vertex);

    if (plugin.NumOverrideFormIDs() == 0) {
      if (logger) {
        logger->trace(
            "Skipping vertex for \"{}\": the plugin contains no override "
            "records.",
            plugin.GetName());
      }
      continue;
    }

    for (vertex_it vit2 = std::next(vit); vit2 != vitend; ++vit2) {
      vertex_t otherVertex = *vit2;
      const auto& otherPlugin = GetPlugin(otherVertex);

      if (vertex == otherVertex || EdgeExists(vertex, otherVertex) ||
          EdgeExists(otherVertex, vertex) ||
          plugin.NumOverrideFormIDs() == otherPlugin.NumOverrideFormIDs() ||
          !plugin.DoFormIDsOverlap(otherPlugin)) {
        continue;
      }

      const auto thisPluginOverridesMoreFormIDs =
          plugin.NumOverrideFormIDs() > otherPlugin.NumOverrideFormIDs();
      vertex_t fromVertex =
          thisPluginOverridesMoreFormIDs ? vertex : otherVertex;
      vertex_t toVertex = thisPluginOverridesMoreFormIDs ? otherVertex : vertex;

      if (!PathExists(toVertex, fromVertex))
        AddEdge(fromVertex, toVertex, EdgeType::overlap);
    }
  }
}

void PluginGraph::AddTieBreakEdges() {
  // In order for the sort to be performed stably, there must be only one
  // possible result. This can be enforced by adding edges between all vertices
  // that aren't already linked. Use existing load order to decide the direction
  // of these edges.
  for (auto [vit, vitend] = GetVertices(); vit != vitend; ++vit) {
    vertex_t vertex = *vit;

    for (vertex_it vit2 = std::next(vit); vit2 != vitend; ++vit2) {
      vertex_t otherVertex = *vit2;

      const auto thisPluginShouldLoadEarlier =
          ComparePlugins(GetPlugin(vertex), GetPlugin(otherVertex)) < 0;
      vertex_t fromVertex = thisPluginShouldLoadEarlier ? vertex : otherVertex;
      vertex_t toVertex = thisPluginShouldLoadEarlier ? otherVertex : vertex;

      if (!PathExists(toVertex, fromVertex))
        AddEdge(fromVertex, toVertex, EdgeType::tieBreak);
    }
  }
}
}
