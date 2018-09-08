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

#include "plugin_sorter.h"

#include <cstdlib>
#include <queue>

#include <boost/algorithm/string.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/locale.hpp>

#include "api/game/game.h"
#include "api/helpers/logging.h"
#include "api/metadata/condition_evaluator.h"
#include "api/sorting/group_sort.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

using std::list;
using std::string;
using std::vector;

namespace loot {
typedef boost::graph_traits<PluginGraph>::vertex_iterator vertex_it;
typedef boost::graph_traits<PluginGraph>::edge_descriptor edge_t;
typedef boost::graph_traits<PluginGraph>::edge_iterator edge_it;

class CycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(edge_t edge, const PluginGraph& graph) {
    const vertex_t source = boost::source(edge, graph);
    const string name = graph[source].GetName();

    // Check if the plugin already exists in the recorded trail.
    auto it = find(begin(trail), end(trail), name);

    if (it != end(trail)) {
      // Erase everything from this position onwards, as it doesn't
      // contribute to a forward-cycle.
      trail.erase(it, end(trail));
    }

    trail.push_back(name);
  }

  void back_edge(edge_t edge, const PluginGraph& graph) {
    vertex_t source = boost::source(edge, graph);
    vertex_t target = boost::target(edge, graph);

    trail.push_back(graph[source].GetName());
    string backCycle;
    auto it = find(begin(trail), end(trail), graph[target].GetName());
    for (it; it != end(trail); ++it) {
      backCycle += *it + ", ";
    }
    backCycle.erase(backCycle.length() - 2);

    throw CyclicInteractionError(
        graph[source].GetName(), graph[target].GetName(), backCycle);
  }

private:
  list<string> trail;
};

std::vector<std::string> PluginSorter::Sort(Game& game) {
  logger_ = getLogger();

  // Clear existing data.
  graph_.clear();
  indexMap_.clear();
  oldLoadOrder_.clear();

  AddPluginVertices(game);

  // If there aren't any vertices, exit early, because sorting assumes
  // there is at least one plugin.
  if (boost::num_vertices(graph_) == 0)
    return vector<std::string>();

  // Get the existing load order.
  oldLoadOrder_ = game.GetLoadOrder();
  if (logger_) {
    logger_->info("Fetched existing load order: ");
    for (const auto& plugin : oldLoadOrder_) {
      logger_->info("\t\t{}", plugin);
    }
  }

  // Now add the interactions between plugins to the graph as edges.
  if (logger_) {
    logger_->info("Adding edges to plugin graph.");
    logger_->debug("Adding non-overlap edges.");
  }
  AddSpecificEdges();

  AddHardcodedPluginEdges(game);

  AddGroupEdges();

  if (logger_) {
    logger_->debug("Adding overlap edges.");
  }
  AddOverlapEdges();

  if (logger_) {
    logger_->debug("Adding tie-break edges.");
  }
  AddTieBreakEdges();

  if (logger_) {
    logger_->debug("Checking to see if the graph is cyclic.");
  }
  CheckForCycles();

  // Now we can sort.
  if (logger_) {
    logger_->debug("Performing a topological sort.");
  }
  list<vertex_t> sortedVertices;
  boost::topological_sort(graph_,
                          std::front_inserter(sortedVertices),
                          boost::vertex_index_map(vertexIndexMap_));

  // Check that the sorted path is Hamiltonian (ie. unique).
  for (auto it = sortedVertices.begin(); it != sortedVertices.end(); ++it) {
    if (next(it) != sortedVertices.end() &&
        !boost::edge(*it, *next(it), graph_).second && logger_) {
      logger_->error(
          "The calculated load order is not unique. No edge exists between {} "
          "and {}.",
          graph_[*it].GetName(),
          graph_[*next(it)].GetName());
    }
  }

  // Output a plugin list using the sorted vertices.
  if (logger_) {
    logger_->info("Calculated order: ");
  }
  vector<std::string> plugins;
  for (const auto& vertex : sortedVertices) {
    plugins.push_back(graph_[vertex].GetName());
    if (logger_) {
      logger_->info("\t{}", plugins.back());
    }
  }

  return plugins;
}

void PluginSorter::AddPluginVertices(Game& game) {
  if (logger_) {
    logger_->info(
        "Merging masterlist, userlist into plugin list, evaluating conditions "
        "and checking for install validity.");
  }

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
  // doesn't strictly need this, there is no guaruntee that this
  // unspecified behaviour will remain in future compiler updates, so
  // implement it generally.

  // Using a set of plugin names followed by finding the matching key
  // in the unordered map, as it's probably faster than copying the
  // full plugin objects then sorting them.
  std::map<std::string, std::vector<std::string>> groupPlugins;

  for (const auto& plugin : game.GetCache()->GetPlugins()) {
    if (logger_) {
      logger_->trace("Getting and evaluating metadata for plugin {}",
                     plugin->GetName());
    }

    auto metadata =
        game.GetDatabase()->GetPluginMetadata(plugin->GetName(), true, true);

    auto groupIt = groupPlugins.find(metadata.GetGroup());
    if (groupIt == groupPlugins.end()) {
      groupPlugins.emplace(metadata.GetGroup(),
                           std::vector<std::string>({plugin->GetName()}));
    } else {
      groupIt->second.push_back(plugin->GetName());
    }

    if (logger_) {
      logger_->trace("Getting and evaluating metadata for plugin \"{}\"",
                     plugin->GetName());
    }

    boost::add_vertex(PluginSortingData(*plugin, std::move(metadata)), graph_);
  }

  // Map sets of transitive group dependencies to sets of transitive plugin
  // dependencies.
  groups_ = game.GetDatabase()->GetGroups();
  auto groups = GetTransitiveAfterGroups(groups_);
  for (auto& group : groups) {
    std::unordered_set<std::string> transitivePlugins;
    for (const auto& afterGroup : group.second) {
      auto pluginsIt = groupPlugins.find(afterGroup);
      if (pluginsIt != groupPlugins.end()) {
        transitivePlugins.insert(pluginsIt->second.begin(),
                                 pluginsIt->second.end());
      }
    }
    group.second = transitivePlugins;
  }

  // Add all transitive plugin dependencies for a group to the plugin's load
  // after metadata.
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    PluginSortingData& plugin = graph_[vertex];

    if (logger_) {
      logger_->trace(
          "Plugin \"{}\" belongs to group \"{}\", setting after group plugins",
          plugin.GetName(),
          plugin.GetGroup());
    }

    auto groupsIt = groups.find(plugin.GetGroup());
    if (groupsIt == groups.end()) {
      throw UndefinedGroupError(plugin.GetGroup());
    } else {
      plugin.SetAfterGroupPlugins(groupsIt->second);
    }
  }

  // Prebuild an index map, which std::list-based VertexList graphs don't have.
  vertexIndexMap_ = vertex_map_t(indexMap_);
  size_t i = 0;
  BGL_FORALL_VERTICES(v, graph_, PluginGraph)
  put(vertexIndexMap_, v, i++);
}

bool PluginSorter::GetVertexByName(const std::string& name,
                                   vertex_t& vertexOut) const {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (boost::iequals(graph_[vertex].GetName(), name)) {
      vertexOut = vertex;
      return true;
    }
  }

  return false;
}

void PluginSorter::CheckForCycles() const {
  boost::depth_first_search(
      graph_, visitor(CycleDetector()).vertex_index_map(vertexIndexMap_));
}

bool PluginSorter::EdgeCreatesCycle(const vertex_t& fromVertex,
                                    const vertex_t& toVertex) const {
  auto start = toVertex;
  auto end = fromVertex;

  std::queue<vertex_t> forwardQueue;
  std::queue<vertex_t> reverseQueue;
  std::unordered_set<vertex_t> forwardVisited;
  std::unordered_set<vertex_t> reverseVisited;

  forwardQueue.push(start);
  forwardVisited.insert(start);
  reverseQueue.push(end);
  reverseVisited.insert(end);

  while (!forwardQueue.empty() && !reverseQueue.empty()) {
    if (!forwardQueue.empty()) {
      auto v = forwardQueue.front();
      forwardQueue.pop();
      if (v == end || reverseVisited.count(v) > 0) {
        return true;
      }
      for (auto adjacentV : boost::make_iterator_range(boost::adjacent_vertices(v, graph_))) {
        if (forwardVisited.count(adjacentV) == 0) {
          forwardVisited.insert(adjacentV);
          forwardQueue.push(adjacentV);
        }
      }
    }
    if (!reverseQueue.empty()) {
      auto v = reverseQueue.front();
      reverseQueue.pop();
      if (v == start || forwardVisited.count(v) > 0) {
        return true;
      }
      for (auto adjacentV : boost::make_iterator_range(boost::inv_adjacent_vertices(v, graph_))) {
        if (reverseVisited.count(adjacentV) == 0) {
          reverseVisited.insert(adjacentV);
          reverseQueue.push(adjacentV);
        }
      }
    }
  }

  return false;
}

void PluginSorter::AddEdge(const vertex_t& fromVertex,
                           const vertex_t& toVertex) {
  if (!boost::edge(fromVertex, toVertex, graph_).second) {
    if (logger_) {
      logger_->trace("Adding edge from \"{}\" to \"{}\".",
                     graph_[fromVertex].GetName(),
                     graph_[toVertex].GetName());
    }

    boost::add_edge(fromVertex, toVertex, graph_);
  }
}

void PluginSorter::AddHardcodedPluginEdges(Game& game) {
  if (logger_) {
    logger_->trace("Adding hardcoded plugin edges.");
  }

  auto implicitlyActivePlugins =
      game.GetLoadOrderHandler()->GetImplicitlyActivePlugins();

  std::set<std::string> processedPlugins;
  for (const auto& plugin : implicitlyActivePlugins) {
    auto lowercasedName = boost::locale::to_lower(plugin);
    processedPlugins.insert(lowercasedName);

    if (game.Type() == GameType::tes5 && lowercasedName == "update.esm") {
      if (logger_) {
        logger_->trace(
            "Skipping adding hardcoded plugin edges for Update.esm as it does "
            "not have a hardcoded position for Skyrim.");
        continue;
      }
    }

    vertex_t pluginVertex;

    if (!GetVertexByName(plugin, pluginVertex)) {
      if (logger_) {
        logger_->trace(
            "Skipping adding harcoded plugin edges for \"{}\" as it is not "
            "installed.",
            plugin);
      }
      continue;
    }

    vertex_it vit, vitend;
    for (tie(vit, vitend) = boost::vertices(graph_); vit != vitend; ++vit) {
      auto& graphPlugin = graph_[*vit];

      if (processedPlugins.count(graphPlugin.GetLowercasedName()) == 0) {
        AddEdge(pluginVertex, *vit);
      }
    }
  }
}

void PluginSorter::AddSpecificEdges() {
  // Add edges for all relationships that aren't overlaps.
  vertex_it vit, vitend;
  for (tie(vit, vitend) = boost::vertices(graph_); vit != vitend; ++vit) {
    if (logger_) {
      logger_->trace("Adding specific edges to vertex for \"{}\".",
                     graph_[*vit].GetName());
      logger_->trace("Adding edges for master flag differences.");
    }

    for (vertex_it vit2 = vit; vit2 != vitend; ++vit2) {
      if (graph_[*vit].IsMaster() == graph_[*vit2].IsMaster())
        continue;

      vertex_t vertex, parentVertex;
      if (graph_[*vit2].IsMaster()) {
        parentVertex = *vit2;
        vertex = *vit;
      } else {
        parentVertex = *vit;
        vertex = *vit2;
      }

      AddEdge(parentVertex, vertex);
    }

    vertex_t parentVertex;
    if (logger_) {
      logger_->trace("Adding in-edges for masters.");
    }
    for (const auto& master : graph_[*vit].GetMasters()) {
      if (GetVertexByName(master, parentVertex))
        AddEdge(parentVertex, *vit);
    }

    if (logger_) {
      logger_->trace("Adding in-edges for requirements.");
    }
    for (const auto& file : graph_[*vit].GetRequirements()) {
      if (GetVertexByName(file.GetName(), parentVertex))
        AddEdge(parentVertex, *vit);
    }

    if (logger_) {
      logger_->trace("Adding in-edges for 'load after's.");
    }
    for (const auto& file : graph_[*vit].GetLoadAfterFiles()) {
      if (GetVertexByName(file.GetName(), parentVertex))
        AddEdge(parentVertex, *vit);
    }
  }
}

bool shouldIgnorePlugin(
    const std::string& group,
    const std::string& pluginName,
    const std::map<std::string, std::unordered_set<std::string>>&
        groupPluginsToIgnore) {
  auto pluginsToIgnore = groupPluginsToIgnore.find(group);
  if (pluginsToIgnore != groupPluginsToIgnore.end()) {
    return pluginsToIgnore->second.count(boost::to_lower_copy(pluginName)) > 0;
  }

  return false;
}

bool shouldIgnoreGroupEdge(
    const PluginSortingData& fromPlugin,
    const PluginSortingData& toPlugin,
    const std::map<std::string, std::unordered_set<std::string>>&
        groupPluginsToIgnore) {
  return shouldIgnorePlugin(
             fromPlugin.GetGroup(), toPlugin.GetName(), groupPluginsToIgnore) ||
         shouldIgnorePlugin(
             toPlugin.GetGroup(), fromPlugin.GetName(), groupPluginsToIgnore);
}

void ignorePlugin(const std::string& pluginName,
                  const std::unordered_set<std::string>& groups,
                  std::map<std::string, std::unordered_set<std::string>>&
                      groupPluginsToIgnore) {
  auto lowercasePluginName = boost::to_lower_copy(pluginName);

  for (const auto& group : groups) {
    auto pluginsToIgnore = groupPluginsToIgnore.find(group);
    if (pluginsToIgnore != groupPluginsToIgnore.end()) {
      pluginsToIgnore->second.insert(lowercasePluginName);
    } else {
      groupPluginsToIgnore.emplace(
          group, std::unordered_set<std::string>({lowercasePluginName}));
    }
  }
}

// Look for paths to targetGroupName from group. Don't pass visitedGroups by
// reference as each after group should be able to record paths independently.
std::unordered_set<std::string> pathfinder(
    const Group& group,
    const std::string& targetGroupName,
    const std::unordered_set<Group>& groups,
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

  // Call pathfinder on each after group. We want to find all paths, so merge
  // all return values.
  std::unordered_set<std::string> mergedVisitedGroups;
  for (const auto& afterGroupName : group.GetAfterGroups()) {
    auto afterGroup = *groups.find(Group(afterGroupName));

    auto recursedVisitedGroups =
        pathfinder(afterGroup, targetGroupName, groups, visitedGroups);

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

std::unordered_set<std::string> getGroupsInPaths(
    const std::unordered_set<Group>& groups,
    const std::string& firstGroupName,
    const std::string& lastGroupName) {
  // Groups are linked in reverse order, i.e. firstGroup can be found from
  // lastGroup, but not the other way around.
  auto lastGroup = *groups.find(Group(lastGroupName));

  auto groupsInPaths = pathfinder(
      lastGroup, firstGroupName, groups, std::unordered_set<std::string>());

  groupsInPaths.erase(lastGroupName);

  return groupsInPaths;
}

void PluginSorter::AddGroupEdges() {
  if (logger_) {
    logger_->trace("Adding group edges.");
  }
  std::vector<std::pair<vertex_t, vertex_t>> acyclicEdgePairs;
  std::map<std::string, std::unordered_set<std::string>> groupPluginsToIgnore;
  for (const vertex_t& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (logger_) {
      logger_->trace("Checking group edges for \"{}\".",
                     graph_[vertex].GetName());
    }
    for (const auto& pluginName : graph_[vertex].GetAfterGroupPlugins()) {
      vertex_t parentVertex;
      if (GetVertexByName(pluginName, parentVertex)) {
        if (EdgeCreatesCycle(parentVertex, vertex)) {
          auto& fromPlugin = graph_[parentVertex];
          auto& toPlugin = graph_[vertex];

          if (logger_) {
            logger_->trace(
                "Skipping edge from \"{}\" to \"{}\" as it would "
                "create a cycle.",
                fromPlugin.GetName(),
                toPlugin.GetName());
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

          auto groupsInPaths = getGroupsInPaths(
              groups_, fromPlugin.GetGroup(), toPlugin.GetGroup());

          ignorePlugin(pluginToIgnore, groupsInPaths, groupPluginsToIgnore);

          continue;
        }

        acyclicEdgePairs.push_back(std::make_pair(parentVertex, vertex));
      }
    }
  }

  if (logger_) {
    logger_->trace(
        "Adding group edges that don't individually introduce cycles.");
  }
  for (const auto& edgePair : acyclicEdgePairs) {
    auto& fromPlugin = graph_[edgePair.first];
    auto& toPlugin = graph_[edgePair.second];
    bool ignore =
        shouldIgnoreGroupEdge(fromPlugin, toPlugin, groupPluginsToIgnore);

    if (!ignore) {
      AddEdge(edgePair.first, edgePair.second);
    } else if (logger_) {
      logger_->trace(
          "Skipping edge from \"{}\" to \"{}\" as it would "
          "create a multi-group cycle.",
          fromPlugin.GetName(),
          toPlugin.GetName());
    }
  }
}

void PluginSorter::AddOverlapEdges() {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (logger_) {
      logger_->trace("Adding overlap edges to vertex for \"{}\".",
                     graph_[vertex].GetName());
    }

    if (graph_[vertex].NumOverrideFormIDs() == 0) {
      if (logger_) {
        logger_->trace(
            "Skipping vertex for \"{}\": the plugin contains no override "
            "records.",
            graph_[vertex].GetName());
      }
      continue;
    }

    for (const auto& otherVertex :
         boost::make_iterator_range(boost::vertices(graph_))) {
      if (vertex == otherVertex ||
          boost::edge(vertex, otherVertex, graph_).second ||
          boost::edge(otherVertex, vertex, graph_).second ||
          graph_[vertex].NumOverrideFormIDs() ==
              graph_[otherVertex].NumOverrideFormIDs() ||
          !graph_[vertex].DoFormIDsOverlap(graph_[otherVertex])) {
        continue;
      }

      vertex_t toVertex, fromVertex;
      if (graph_[vertex].NumOverrideFormIDs() >
          graph_[otherVertex].NumOverrideFormIDs()) {
        fromVertex = vertex;
        toVertex = otherVertex;
      } else {
        fromVertex = otherVertex;
        toVertex = vertex;
      }

      if (!EdgeCreatesCycle(fromVertex, toVertex))
        AddEdge(fromVertex, toVertex);
    }
  }
}

int PluginSorter::ComparePlugins(const std::string& plugin1,
                                 const std::string& plugin2) const {
  auto it1 = find(begin(oldLoadOrder_), end(oldLoadOrder_), plugin1);
  auto it2 = find(begin(oldLoadOrder_), end(oldLoadOrder_), plugin2);

  if (it1 != end(oldLoadOrder_) && it2 == end(oldLoadOrder_))
    return -1;
  else if (it1 == end(oldLoadOrder_) && it2 != end(oldLoadOrder_))
    return 1;
  else if (it1 != end(oldLoadOrder_) && it2 != end(oldLoadOrder_)) {
    if (distance(begin(oldLoadOrder_), it1) <
        distance(begin(oldLoadOrder_), it2))
      return -1;
    else
      return 1;
  } else {
    // Neither plugin has a load order position. Need to use another
    // comparison to get an ordering.

    // Compare plugin basenames.
    string name1 = boost::locale::to_lower(plugin1);
    name1 = name1.substr(0, name1.length() - 4);
    string name2 = boost::locale::to_lower(plugin2);
    name2 = name2.substr(0, name2.length() - 4);

    if (name1 < name2)
      return -1;
    else if (name2 < name1)
      return 1;
    else {
      // Could be a .esp and .esm plugin with the same basename,
      // compare whole filenames.
      if (plugin1 < plugin2)
        return -1;
      else
        return 1;
    }
  }
  return 0;
}

void PluginSorter::AddTieBreakEdges() {
  // In order for the sort to be performed stably, there must be only one
  // possible result. This can be enforced by adding edges between all vertices
  // that aren't already linked. Use existing load order to decide the direction
  // of these edges.
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (logger_) {
      logger_->trace("Adding tie-break edges to vertex for \"{}\"",
                     graph_[vertex].GetName());
    }

    for (const auto& otherVertex :
         boost::make_iterator_range(boost::vertices(graph_))) {
      if (vertex == otherVertex ||
          boost::edge(vertex, otherVertex, graph_).second ||
          boost::edge(otherVertex, vertex, graph_).second)
        continue;

      vertex_t toVertex, fromVertex;
      if (ComparePlugins(graph_[vertex].GetName(),
                         graph_[otherVertex].GetName()) < 0) {
        fromVertex = vertex;
        toVertex = otherVertex;
      } else {
        fromVertex = otherVertex;
        toVertex = vertex;
      }

      if (!EdgeCreatesCycle(fromVertex, toVertex))
        AddEdge(fromVertex, toVertex);
    }
  }
}
}
