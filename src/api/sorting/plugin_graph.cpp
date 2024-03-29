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
#include <queue>

#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "api/sorting/group_sort.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
typedef boost::graph_traits<RawPluginGraph>::edge_descriptor edge_t;
typedef boost::graph_traits<RawPluginGraph>::edge_iterator edge_it;

class CycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(edge_t edge, const RawPluginGraph& graph) {
    const auto source = boost::source(edge, graph);

    const auto vertex = Vertex(graph[source].GetName(), graph[edge]);

    trail.push_back(vertex);
  }

  void back_edge(edge_t edge, const RawPluginGraph& graph) {
    const auto source = boost::source(edge, graph);
    const auto target = boost::target(edge, graph);

    const auto vertex = Vertex(graph[source].GetName(), graph[edge]);
    trail.push_back(vertex);

    const auto it = find_if(begin(trail), end(trail), [&](const Vertex& v) {
      return v.GetName() == graph[target].GetName();
    });

    if (it == trail.end()) {
      throw std::logic_error(
          "The target of a back edge cannot be found in the current edge path");
    }

    throw CyclicInteractionError(std::vector<Vertex>(it, trail.end()));
  }

  void finish_vertex(vertex_t, const RawPluginGraph&) {
    if (!trail.empty()) {
      trail.pop_back();
    }
  }

private:
  std::vector<Vertex> trail;
};

struct GroupPlugin {
  vertex_t vertex{0};
  bool groupIsUserMetadata{false};
};

struct PredecessorGroupPlugin {
  vertex_t vertex{0};
  bool pathInvolvesUserMetadata{false};
};

std::unordered_map<std::string, std::vector<PredecessorGroupPlugin>>
GetPredecessorGroupsPlugins(
    const std::unordered_map<std::string, std::vector<GroupPlugin>>&
        groupsPlugins,
    const std::unordered_map<std::string, std::vector<PredecessorGroup>>&
        predecessorGroupsMap) {
  std::unordered_map<std::string, std::vector<PredecessorGroupPlugin>>
      predecessorGroupsPlugins;
  for (const auto& group : predecessorGroupsMap) {
    std::vector<PredecessorGroupPlugin> predecessorGroupPlugins;

    for (const auto& predecessorGroup : group.second) {
      const auto pluginsIt = groupsPlugins.find(predecessorGroup.name);
      if (pluginsIt != groupsPlugins.end()) {
        // If the path from the predecessor group to this one involves user
        // metadata, the plugins' paths all involve user metadata, otherwise
        // only those plugins that belong to the predecessor group due to user
        // metadata have a path involving user metadata.
        for (const auto& groupPlugin : pluginsIt->second) {
          predecessorGroupPlugins.push_back(PredecessorGroupPlugin{
              groupPlugin.vertex,
              predecessorGroup.pathInvolvesUserMetadata ||
                  groupPlugin.groupIsUserMetadata});
        }
      }
    }

    predecessorGroupsPlugins.insert({group.first, predecessorGroupPlugins});
  }

  return predecessorGroupsPlugins;
}

std::unordered_map<std::string, std::vector<PredecessorGroupPlugin>>
GetPredecessorGroupsPlugins(
    const PluginGraph& graph,
    const std::unordered_map<std::string, std::vector<PredecessorGroup>>&
        predecessorGroupsMap) {
  // Each element of the vector is a pair of a plugin name and if it's in the
  // group due to user metadata.
  std::unordered_map<std::string, std::vector<GroupPlugin>> groupsPlugins;

  for (const vertex_t& vertex :
       boost::make_iterator_range(graph.GetVertices())) {
    const auto& plugin = graph.GetPlugin(vertex);
    const auto groupName = plugin.GetGroup();
    const auto groupPlugin = GroupPlugin{vertex, plugin.IsGroupUserMetadata()};

    const auto groupIt = groupsPlugins.find(groupName);

    if (groupIt == groupsPlugins.end()) {
      groupsPlugins.emplace(groupName, std::vector<GroupPlugin>({groupPlugin}));
    } else {
      groupIt->second.push_back(groupPlugin);
    }
  }

  // Sort plugins by their names. This is necessary to ensure that
  // plugin precedessor group plugins are listed in a consistent
  // order, which is important because that is the order in which
  // group edges are added and differences could cause different
  // sorting results.
  for (auto& groupPlugins : groupsPlugins) {
    std::sort(groupPlugins.second.begin(),
              groupPlugins.second.end(),
              [&](const GroupPlugin& lhs, const GroupPlugin& rhs) {
                return graph.GetPlugin(lhs.vertex).GetName() <
                       graph.GetPlugin(rhs.vertex).GetName();
              });
  }

  // Map sets of transitive group dependencies to sets of transitive plugin
  // dependencies.
  return GetPredecessorGroupsPlugins(groupsPlugins, predecessorGroupsMap);
}

std::vector<PredecessorGroupPlugin> GetPredecessorGroupPlugins(
    const PluginSortingData& plugin,
    const std::unordered_map<std::string, std::vector<PredecessorGroupPlugin>>&
        predecessorGroupsPlugins) {
  const auto groupsIt = predecessorGroupsPlugins.find(plugin.GetGroup());
  if (groupsIt == predecessorGroupsPlugins.end()) {
    throw UndefinedGroupError(plugin.GetGroup());
  }

  if (plugin.IsGroupUserMetadata()) {
    // If the current plugin is a member of its group due to user metadata,
    // then all predecessor plugins are such due to user metadata.
    auto predecessorGroupPlugins = groupsIt->second;
    for (auto& predecessorGroupPlugin : predecessorGroupPlugins) {
      predecessorGroupPlugin.pathInvolvesUserMetadata = true;
    }

    return predecessorGroupPlugins;
  }

  return groupsIt->second;
}

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
    case EdgeType::masterlistGroup:
      return "Masterlist Group";
    case EdgeType::userGroup:
      return "User Group";
    case EdgeType::recordOverlap:
      return "Record Overlap";
    case EdgeType::assetOverlap:
      return "Asset Overlap";
    case EdgeType::tieBreak:
      return "Tie Break";
    default:
      return "Unknown";
  }
}

std::string PathToString(const RawPluginGraph& graph,
                         const std::vector<vertex_t>& path) {
  std::string pathString;
  for (const auto& vertex : path) {
    pathString += graph[vertex].GetName() + ", ";
  }

  return pathString.substr(0, pathString.length() - 2);
}

class BidirVisitor {
public:
  virtual ~BidirVisitor() = default;

  virtual void VisitForwardVertex(const vertex_t& sourceVertex,
                                  const vertex_t& targetVertex) = 0;

  virtual void VisitReverseVertex(const vertex_t& sourceVertex,
                                  const vertex_t& targetVertex) = 0;

  virtual void VisitIntersectionVertex(const vertex_t& intersectionVertex) = 0;
};

class PathCacher : public BidirVisitor {
public:
  explicit PathCacher(PathsCache& pathsCache,
                      const vertex_t& fromVertex,
                      const vertex_t& toVertex) :
      pathsCache_(&pathsCache), fromVertex_(fromVertex), toVertex_(toVertex) {}

  void VisitForwardVertex(const vertex_t&, const vertex_t& targetVertex) {
    pathsCache_->CachePath(fromVertex_, targetVertex);
  }

  void VisitReverseVertex(const vertex_t& sourceVertex, const vertex_t&) {
    pathsCache_->CachePath(sourceVertex, toVertex_);
  }

  void VisitIntersectionVertex(const vertex_t&) {}

protected:
  vertex_t GetFromVertex() const { return fromVertex_; }

  vertex_t GetToVertex() const { return toVertex_; }

private:
  PathsCache* pathsCache_{nullptr};
  vertex_t fromVertex_{0};
  vertex_t toVertex_{0};
};

class PathFinder : public PathCacher {
public:
  explicit PathFinder(const RawPluginGraph& graph,
                      PathsCache& pathsCache,
                      const vertex_t& fromVertex,
                      const vertex_t& toVertex) :
      PathCacher(pathsCache, fromVertex, toVertex), graph_(&graph) {}

  void VisitForwardVertex(const vertex_t& sourceVertex,
                          const vertex_t& targetVertex) {
    PathCacher::VisitForwardVertex(sourceVertex, targetVertex);

    forwardParents.insert_or_assign(targetVertex, sourceVertex);
  }

  void VisitReverseVertex(const vertex_t& sourceVertex,
                          const vertex_t& targetVertex) {
    PathCacher::VisitReverseVertex(sourceVertex, targetVertex);

    reverseChildren.insert_or_assign(sourceVertex, targetVertex);
  }

  void VisitIntersectionVertex(const vertex_t& intersectionVertex) {
    intersectionVertex_ = intersectionVertex;
  }

  std::optional<std::vector<vertex_t>> GetPath() {
    if (!intersectionVertex_.has_value()) {
      return std::nullopt;
    }

    std::vector<vertex_t> path({intersectionVertex_.value()});
    auto currentVertex = intersectionVertex_.value();

    const auto logger = getLogger();

    while (currentVertex != GetFromVertex()) {
      const auto it = forwardParents.find(currentVertex);
      if (it == forwardParents.end()) {
        const auto pluginName = (*graph_)[currentVertex].GetName();
        if (logger) {
          logger->error("Could not find parent vertex of {}. Path so far is {}",
                        pluginName,
                        PathToString(*graph_, path));
        }
        throw std::runtime_error(
            "Unexpectedly could not find parent vertex of " + pluginName);
      }

      path.push_back(it->second);

      currentVertex = it->second;
    }

    // The path current runs backwards, so reverse it.
    std::reverse(path.begin(), path.end());

    currentVertex = intersectionVertex_.value();

    while (currentVertex != GetToVertex()) {
      const auto it = reverseChildren.find(currentVertex);
      if (it == reverseChildren.end()) {
        const auto pluginName = (*graph_)[currentVertex].GetName();
        if (logger) {
          logger->error("Could not find child vertex of {}. Path so far is {}",
                        pluginName,
                        PathToString(*graph_, path));
        }
        throw std::runtime_error(
            "Unexpectedly could not find child vertex of " + pluginName);
      }

      path.push_back(it->second);

      currentVertex = it->second;
    }

    return path;
  }

private:
  const RawPluginGraph* graph_{nullptr};

  std::unordered_map<vertex_t, vertex_t> forwardParents;
  std::unordered_map<vertex_t, vertex_t> reverseChildren;
  std::optional<vertex_t> intersectionVertex_;
};

bool FindPath(RawPluginGraph& graph,
              const vertex_t& fromVertex,
              const vertex_t& toVertex,
              BidirVisitor& visitor) {
  std::queue<vertex_t> forwardQueue;
  std::queue<vertex_t> reverseQueue;
  std::unordered_set<vertex_t> forwardVisited;
  std::unordered_set<vertex_t> reverseVisited;

  forwardQueue.push(fromVertex);
  forwardVisited.insert(fromVertex);
  reverseQueue.push(toVertex);
  reverseVisited.insert(toVertex);

  const auto logger = getLogger();

  while (!forwardQueue.empty() && !reverseQueue.empty()) {
    if (!forwardQueue.empty()) {
      const auto v = forwardQueue.front();
      forwardQueue.pop();
      if (v == toVertex || reverseVisited.count(v) > 0) {
        visitor.VisitIntersectionVertex(v);
        return true;
      }
      for (const auto adjacentV :
           boost::make_iterator_range(boost::adjacent_vertices(v, graph))) {
        if (forwardVisited.count(adjacentV) == 0) {
          visitor.VisitForwardVertex(v, adjacentV);

          forwardVisited.insert(adjacentV);
          forwardQueue.push(adjacentV);
        }
      }
    }
    if (!reverseQueue.empty()) {
      const auto v = reverseQueue.front();
      reverseQueue.pop();
      if (v == fromVertex || forwardVisited.count(v) > 0) {
        visitor.VisitIntersectionVertex(v);
        return true;
      }
      for (const auto adjacentV :
           boost::make_iterator_range(boost::inv_adjacent_vertices(v, graph))) {
        if (reverseVisited.count(adjacentV) == 0) {
          visitor.VisitReverseVertex(adjacentV, v);

          reverseVisited.insert(adjacentV);
          reverseQueue.push(adjacentV);
        }
      }
    }
  }

  return false;
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
  const auto logger = getLogger();
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
  if (logger) {
    logger->trace("Checking plugin graph for cycles...");
  }

  boost::depth_first_search(graph_, visitor(CycleDetector()));
}

std::vector<vertex_t> PluginGraph::TopologicalSort() const {
  std::vector<vertex_t> sortedVertices;
  const auto logger = getLogger();
  if (logger) {
    logger->trace("Performing topological sort on plugin graph...");
  }
  boost::topological_sort(graph_, std::back_inserter(sortedVertices));

  std::reverse(sortedVertices.begin(), sortedVertices.end());

  return sortedVertices;
}

std::optional<std::pair<vertex_t, vertex_t>> PluginGraph::IsHamiltonianPath(
    const std::vector<vertex_t>& path) const {
  const auto logger = getLogger();
  if (logger) {
    logger->trace("Checking uniqueness of path through plugin graph...");
  }

  for (auto it = path.begin(); it != path.end(); ++it) {
    if (next(it) != path.end() && !boost::edge(*it, *next(it), graph_).second) {
      return std::make_pair(*it, *next(it));
    }
  }

  return std::nullopt;
}

std::vector<std::string> PluginGraph::ToPluginNames(
    const std::vector<vertex_t>& path) const {
  std::vector<std::string> names;
  for (const auto& vertex : path) {
    names.push_back(GetPlugin(vertex).GetName());
  }

  return names;
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

  PathCacher visitor(pathsCache_, fromVertex, toVertex);

  return loot::FindPath(graph_, fromVertex, toVertex, visitor);
}

std::optional<std::vector<vertex_t>> PluginGraph::FindPath(
    const vertex_t& fromVertex,
    const vertex_t& toVertex) {
  PathFinder visitor(graph_, pathsCache_, fromVertex, toVertex);

  loot::FindPath(graph_, fromVertex, toVertex, visitor);

  return visitor.GetPath();
}

std::optional<EdgeType> PluginGraph::GetEdgeType(const vertex_t& fromVertex,
                                                 const vertex_t& toVertex) {
  const auto edge = boost::edge(fromVertex, toVertex, graph_);
  if (!edge.second) {
    return std::nullopt;
  }

  return graph_[edge.first];
}

void PluginGraph::AddEdge(const vertex_t& fromVertex,
                          const vertex_t& toVertex,
                          EdgeType edgeType) {
  if (pathsCache_.IsPathCached(fromVertex, toVertex)) {
    return;
  }

  const auto logger = getLogger();
  if (logger) {
    logger->debug("Adding {} edge from \"{}\" to \"{}\".",
                  describeEdgeType(edgeType),
                  GetPlugin(fromVertex).GetName(),
                  GetPlugin(toVertex).GetName());
  }

  boost::add_edge(fromVertex, toVertex, edgeType, graph_);
  pathsCache_.CachePath(fromVertex, toVertex);
}

vertex_t PluginGraph::AddVertex(const PluginSortingData& plugin) {
  return boost::add_vertex(plugin, graph_);
}

void PluginGraph::AddSpecificEdges() {
  const auto logger = getLogger();
  if (logger) {
    logger->trace(
        "Adding edges based on plugin data and non-group metadata...");
  }

  // Add edges for all relationships that aren't overlaps.
  for (auto [vit, vitend] = GetVertices(); vit != vitend; ++vit) {
    const auto& vertex = *vit;
    const auto& plugin = GetPlugin(vertex);

    // This loop should have no effect now that master-flagged and
    // non-master-flagged plugins are sorted separately, but is kept
    // as a safety net.
    for (vertex_it vit2 = std::next(vit); vit2 != vitend; ++vit2) {
      const auto& otherVertex = *vit2;
      const auto& otherPlugin = GetPlugin(otherVertex);

      if (plugin.IsMaster() == otherPlugin.IsMaster()) {
        continue;
      }

      const auto isOtherPluginAMaster = otherPlugin.IsMaster();
      vertex_t childVertex = isOtherPluginAMaster ? vertex : otherVertex;
      vertex_t parentVertex = isOtherPluginAMaster ? otherVertex : vertex;

      AddEdge(parentVertex, childVertex, EdgeType::masterFlag);
    }

    for (const auto& master : plugin.GetMasters()) {
      const auto parentVertex = GetVertexByName(master);
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::master);
      }
    }

    for (const auto& file : plugin.GetMasterlistRequirements()) {
      const auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::masterlistRequirement);
      }
    }
    for (const auto& file : plugin.GetUserRequirements()) {
      const auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::userRequirement);
      }
    }

    for (const auto& file : plugin.GetMasterlistLoadAfterFiles()) {
      const auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::masterlistLoadAfter);
      }
    }
    for (const auto& file : plugin.GetUserLoadAfterFiles()) {
      const auto parentVertex = GetVertexByName(std::string(file.GetName()));
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), vertex, EdgeType::userLoadAfter);
      }
    }
  }
}

void PluginGraph::AddHardcodedPluginEdges(
    const std::vector<std::string>& hardcodedPlugins) {
  const auto logger = getLogger();
  if (logger) {
    logger->trace(
        "Adding edges for implicitly active plugins and plugins with hardcoded "
        "positions...");
  }

  if (hardcodedPlugins.empty()) {
    return;
  }

  std::map<std::vector<std::string>::const_iterator, vertex_t>
      implicitlyActivePluginVertices;
  std::vector<vertex_t> otherPluginVertices;

  // Build the vertex map for implicitly active plugins and record the vertices
  // for other plugins.
  for (const auto& vertex : boost::make_iterator_range(GetVertices())) {
    const auto pluginName = GetPlugin(vertex).GetName();
    const auto it =
        std::find_if(hardcodedPlugins.begin(),
                     hardcodedPlugins.end(),
                     [&](const std::string& name) {
                       return CompareFilenames(name, pluginName) == 0;
                     });

    if (it != hardcodedPlugins.end()) {
      implicitlyActivePluginVertices.emplace(it, vertex);
    } else {
      otherPluginVertices.push_back(vertex);
    }
  }

  if (implicitlyActivePluginVertices.empty()) {
    return;
  }

  // Now add edges between consecutive implicitly active plugins.
  auto lastImplicitlyActiveVertexIt = implicitlyActivePluginVertices.end();
  for (auto it = hardcodedPlugins.begin(); it != hardcodedPlugins.end();) {
    const auto fromVertexIt = implicitlyActivePluginVertices.find(it);

    // Find the next valid implicitly active plugin and its vertex.
    auto toVertexIt = implicitlyActivePluginVertices.end();
    for (it = std::next(it); it != hardcodedPlugins.end(); ++it) {
      toVertexIt = implicitlyActivePluginVertices.find(it);
      if (toVertexIt != implicitlyActivePluginVertices.end()) {
        break;
      }
    }

    if (fromVertexIt != implicitlyActivePluginVertices.end()) {
      lastImplicitlyActiveVertexIt = fromVertexIt;

      if (toVertexIt != implicitlyActivePluginVertices.end()) {
        AddEdge(fromVertexIt->second, toVertexIt->second, EdgeType::hardcoded);
      }
    }
  }

  // Finally, add edges from the last implicitly active plugin to the other
  // plugins.
  if (lastImplicitlyActiveVertexIt != implicitlyActivePluginVertices.end()) {
    for (const auto& vertex : otherPluginVertices) {
      AddEdge(
          lastImplicitlyActiveVertexIt->second, vertex, EdgeType::hardcoded);
    }
  }
}

void PluginGraph::AddGroupEdges(
    const std::unordered_map<std::string, Group>& groups,
    const std::unordered_map<std::string, std::vector<PredecessorGroup>>&
        predecessorGroupsMap) {
  const auto logger = getLogger();
  if (logger) {
    logger->trace("Adding edges based on plugin group memberships...");
  }

  const auto predecessorGroupsPlugins =
      GetPredecessorGroupsPlugins(*this, predecessorGroupsMap);

  // Tuple fields are from, to, and edge type.
  std::vector<std::tuple<vertex_t, vertex_t, EdgeType>> acyclicEdges;
  std::map<std::string, std::unordered_set<std::string>> groupPluginsToIgnore;

  for (const vertex_t& vertex : boost::make_iterator_range(GetVertices())) {
    const auto& toPlugin = GetPlugin(vertex);

    const auto predecessorGroupPlugins =
        GetPredecessorGroupPlugins(toPlugin, predecessorGroupsPlugins);

    for (const auto& plugin : predecessorGroupPlugins) {
      // After group plugin names are taken from other PluginSortingData names,
      // so exact string comparisons can be used.
      const auto parentVertex = plugin.vertex;

      if (PathExists(vertex, parentVertex)) {
        const auto& fromPlugin = GetPlugin(parentVertex);

        if (logger) {
          logger->debug(
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

      const auto edgeType = plugin.pathInvolvesUserMetadata
                                ? EdgeType::userGroup
                                : EdgeType::masterlistGroup;

      acyclicEdges.push_back(std::make_tuple(parentVertex, vertex, edgeType));
    }
  }

  for (const auto& edge : acyclicEdges) {
    const auto fromVertex = std::get<0>(edge);
    const auto toVertex = std::get<1>(edge);
    const auto edgeType = std::get<2>(edge);
    const auto& fromPlugin = GetPlugin(fromVertex);
    const auto& toPlugin = GetPlugin(toVertex);
    const bool ignore =
        ShouldIgnoreGroupEdge(fromPlugin, toPlugin, groupPluginsToIgnore);

    if (!ignore) {
      AddEdge(fromVertex, toVertex, edgeType);
    } else if (logger) {
      logger->debug(
          "Skipping {} edge from \"{}\" to \"{}\" as it would "
          "create a multi-group cycle.",
          describeEdgeType(edgeType),
          fromPlugin.GetName(),
          toPlugin.GetName());
    }
  }
}

void PluginGraph::AddOverlapEdges() {
  const auto logger = getLogger();
  if (logger) {
    logger->trace("Adding edges for overlapping plugins...");
  }

  for (auto [vit, vitend] = GetVertices(); vit != vitend; ++vit) {
    const auto vertex = *vit;
    const auto& plugin = GetPlugin(vertex);
    const auto pluginRecordCount = plugin.GetOverrideRecordCount();
    const auto pluginAssetCount = plugin.GetAssetCount();

    if (pluginRecordCount == 0 && pluginAssetCount == 0) {
      if (logger) {
        logger->debug(
            "Skipping vertex for \"{}\": the plugin contains no override "
            "records and loads no assets.",
            plugin.GetName());
      }
      continue;
    }

    for (vertex_it vit2 = std::next(vit); vit2 != vitend; ++vit2) {
      const auto otherVertex = *vit2;
      const auto& otherPlugin = GetPlugin(otherVertex);

      // Don't add an edge between these two plugins if one already
      // exists (only check direct edges and not paths for efficiency).
      if (EdgeExists(vertex, otherVertex) || EdgeExists(otherVertex, vertex)) {
        continue;
      }

      // Two plugins can overlap due to overriding the same records,
      // or by loading assets from BSAs/BA2s that have the same path.
      // If records overlap, the plugin that overrides more records
      // should load earlier.
      // If assets overlap, the plugin that loads more assets should
      // load earlier.
      // If two plugins have overlapping records and assets and one
      // overrides more records but loads fewer assets than the other,
      // the fact it overrides more records should take precedence
      // (records are more significant than assets).
      // I.e. if two plugins don't have overlapping records, check their
      // assets, otherwise only check their assets if their override
      // record counts are equal.

      auto thisPluginLoadsFirst = false;
      EdgeType edgeType = EdgeType::recordOverlap;

      const auto otherPluginRecordCount = otherPlugin.GetOverrideRecordCount();

      if (pluginRecordCount == otherPluginRecordCount ||
          !plugin.DoRecordsOverlap(otherPlugin)) {
        // Records don't overlap, or override the same number of records,
        // check assets.
        // No records overlap, check assets.
        const auto otherPluginAssetCount = otherPlugin.GetAssetCount();
        if (pluginAssetCount == otherPluginAssetCount ||
            !plugin.DoAssetsOverlap(otherPlugin)) {
          // Assets don't overlap or both plugins load the same number of
          // assets, don't add an edge.
          continue;
        } else {
          thisPluginLoadsFirst = pluginAssetCount > otherPluginAssetCount;
          edgeType = EdgeType::assetOverlap;
        }
      } else {
        // Records overlap and override different numbers of records.
        // Load this plugin first if it overrides more records.
        thisPluginLoadsFirst = pluginRecordCount > otherPluginRecordCount;
      }

      const auto fromVertex = thisPluginLoadsFirst ? vertex : otherVertex;
      const auto toVertex = thisPluginLoadsFirst ? otherVertex : vertex;

      if (!PathExists(toVertex, fromVertex)) {
        AddEdge(fromVertex, toVertex, edgeType);
      } else if (logger) {
        logger->debug(
            "Skipping {} edge from \"{}\" to \"{}\" as it would "
            "create a cycle.",
            describeEdgeType(edgeType),
            GetPlugin(fromVertex).GetName(),
            GetPlugin(toVertex).GetName());
      }
    }
  }
}

void PluginGraph::AddTieBreakEdges() {
  const auto logger = getLogger();
  if (logger) {
    logger->trace("Adding edges to break ties between plugins...");
  }

  // In order for the sort to be performed stably, there must be only one
  // possible result. This can be enforced by adding edges between all vertices
  // that aren't already linked. Use existing load order to decide the direction
  // of these edges, and only add an edge if it won't cause a cycle.
  //
  // Brute-forcing this by adding an edge between every pair of vertices
  // (unless it would cause a cycle) works but scales terribly, as before each
  // edge is added a bidirectional search needs to be done for a path in the
  // other direction (to detect a potential cycle). This search takes more time
  // as the number of edges involves increases, so adding tie breaks gets slower
  // as they get added.
  //
  // The point of adding these tie breaks is to ensure that there's a
  // Hamiltonian path through the graph and therefore only one possible
  // topological sort result.
  //
  // Instead of trying to brute-force this, iterate over the graph's vertices in
  // their existing load order (each vertex represents a plugin, so the two
  // terms are used interchangeably), and add an edge going from the earlier to
  // the later for each consecutive pair of plugins (e.g. for [A, B, C], add
  // edges A->B, B->C), unless adding the edge would cause a cycle. If sorting
  // has made no changes to the load order, then it'll be possible to add all
  // those edges and only N - 1 bidirectional searches will be needed when there
  // are N vertices.
  //
  // If it's not possible to add such an edge for a pair of plugins [A, B], that
  // means that LOOT thinks A needs to load after B, i.e. the sorted load order
  // will be different. If the existing path between A and B is B -> C -> D -> A
  // then walk back through the load order to find a plugin that B will load
  // after without causing a cycle, and add an edge going from that plugin to B.
  // Then do the same for each subsequent plugin in the path between A and B so
  // that every plugin in the existing load order until A has a path to each of
  // the plugins in the path from B to A, and that there is only one path that
  // will visit all plugins until A. Keep a record of this path, because that's
  // the load order that needs to be walked back through whenever the existing
  // relative positions of plugins can't be used (if the existing load order was
  // used, the process would miss out on plugins introduced in previous backward
  // walks, and so you'd end up with multiple paths that don't necessarily touch
  // all plugins).

  // Storage for the load order as it evolves: a list is used because there may
  // be a lot of inserts and it works out more efficient with large load orders
  // and the efficiency difference doesn't matter for small load orders.
  std::list<vertex_t> newLoadOrder;

  // Holds vertices that have already been put into newLoadOrder.
  std::unordered_set<vertex_t> processedVertices;

  const auto pinVertexPosition =
      [&](const vertex_t& vertex,
          const std::list<vertex_t>::const_reverse_iterator reverseEndIt)
      -> std::list<vertex_t>::const_reverse_iterator {
    // It's possible that this vertex has already been pinned in place,
    // e.g. because it was visited earlier in the old load order or
    // as part of a path that was processed. In that case just skip it.
    if (processedVertices.count(vertex) != 0) {
      if (logger) {
        logger->debug(
            "The plugin \"{}\" has already been processed, skipping it.",
            GetPlugin(vertex).GetName());
      }
      return reverseEndIt;
    }

    // Otherwise, this vertex needs to be inserted into the path that includes
    // all other vertices that have been processed so far. This can be done by
    // searching for the last vertex in the "new load order" path for which
    // there is not a path going from this vertex to that vertex. I.e. find the
    // last plugin that this one can load after. We could instead find the last
    // plugin that this one *must* load after, but it turns out that's
    // significantly slower because it generally involves going further back
    // along the "new load order" path.
    const auto previousVertexPosition =
        std::find_if(newLoadOrder.crbegin(),
                     reverseEndIt,
                     [&](const vertex_t& loadOrderVertex) {
                       return !PathExists(vertex, loadOrderVertex);
                     });

    // Add an edge going from the found vertex to this one, in case it
    // doesn't exist (we only know there's not a path going the other way).
    if (previousVertexPosition != reverseEndIt) {
      const auto precedingVertex = *previousVertexPosition;

      AddEdge(precedingVertex, vertex, EdgeType::tieBreak);
    }

    // Insert position is just after the found vertex, and a forward iterator
    // points to the element one after the element pointed to by the
    // corresponding reverse iterator.
    const auto insertPosition = previousVertexPosition.base();

    // Add an edge going from this vertex to the next one in the "new load
    // order" path, in case there isn't already one.
    if (insertPosition != newLoadOrder.end()) {
      const auto followingVertex = *insertPosition;

      AddEdge(vertex, followingVertex, EdgeType::tieBreak);
    }

    // Now update newLoadOrder with the vertex's new position.
    const auto newLoadOrderIt = newLoadOrder.insert(insertPosition, vertex);
    processedVertices.insert(vertex);

    if (logger) {
      const auto nextLoadOrderIt = std::next(newLoadOrderIt);

      if (nextLoadOrderIt == newLoadOrder.end()) {
        logger->debug(
            "The plugin \"{}\" loads at the end of the new load order so "
            "far.",
            GetPlugin(vertex).GetName());
      } else {
        logger->debug(
            "The plugin \"{}\" loads before \"{}\" in the new load order.",
            GetPlugin(vertex).GetName(),
            GetPlugin(*nextLoadOrderIt).GetName());
      }
    }

    // Return a new value for reverseEndIt, pointing to the newly
    // inserted vertex, as if it was not the last vertex in a path
    // being processed the next vertex in the path by definition
    // cannot load before this one, so we can save an unnecessary
    // check by using this new reverseEndIt value when pinning the
    // next vertex.
    return std::make_reverse_iterator(std::next(newLoadOrderIt));
  };

  // First get the graph vertices and sort them into the current load order.
  const auto [it, itend] = GetVertices();
  std::vector<vertex_t> vertices(it, itend);

  std::sort(vertices.begin(),
            vertices.end(),
            [this](const vertex_t& lhs, const vertex_t& rhs) {
              return ComparePlugins(GetPlugin(lhs), GetPlugin(rhs)) < 0;
            });

  // Now iterate over the vertices in their sorted order.
  const auto vitstart = vertices.begin();
  const auto vitend = vertices.end();
  for (auto vit = vitstart; vit != vitend; ++vit) {
    const auto currentVertex = *vit;
    const auto nextVertexIt = std::next(vit);

    if (nextVertexIt == vitend) {
      // Don't dereference the past-the-end iterator.
      break;
    }

    const auto nextVertex = *nextVertexIt;

    auto pathFromNextVertex = FindPath(nextVertex, currentVertex);

    if (!pathFromNextVertex.has_value()) {
      // There's no path from nextVertex to currentVertex, so it's OK to add
      // an edge going in the other direction, meaning that nextVertex can
      // load after currentVertex.
      AddEdge(currentVertex, nextVertex, EdgeType::tieBreak);

      // nextVertex now loads after currentVertex. If currentVertex hasn't
      // already been added to the load order, append it. It might have already
      // been added if it was part of a path going from nextVertex and
      // currentVertex in a previous loop (i.e. for different values of
      // nextVertex and currentVertex).
      if (processedVertices.count(currentVertex) == 0) {
        newLoadOrder.push_back(currentVertex);
        processedVertices.insert(currentVertex);

        if (logger) {
          logger->debug(
              "The plugin \"{}\" loads at the end of the new load order so "
              "far.",
              GetPlugin(currentVertex).GetName());
        }
      } else if (currentVertex != newLoadOrder.back()) {
        if (logger) {
          logger->trace(
              "Plugin \"{}\" has already been processed and is not last in the "
              "new load order, determining where to place \"{}\".",
              GetPlugin(currentVertex).GetName(),
              GetPlugin(nextVertex).GetName());
        }

        // If currentVertex was already processed and not the last vertex
        // in newLoadOrder then nextVertex also needs to be pinned in place or
        // it may not have a defined position relative to all the
        // vertices following currentVertex in newLoadOrder undefined, so
        // there wouldn't be a unique path through them.
        //
        // We're using newLoadOrder.rend() as the last iterator position because
        // we don't know currentVertex's position.
        pinVertexPosition(nextVertex, newLoadOrder.rend());
      }
    } else {
      // Each vertex in pathFromNextVertex (besides the last, which is
      // currentVertex) needs to be positioned relative to a vertex that has
      // already been iterated over (i.e. in what begins as the old load
      // order) so that there is a single path between all vertices.
      //
      // If currentVertex is the first in the iteration order, then
      // nextVertex is simply the earliest known plugin in the new load order
      // so far.
      if (vit == vitstart) {
        // Record the path as the start of the new load order.
        // Don't need to add any edges because there's nothing for nextVertex
        // to load after at this point.
        if (logger) {
          logger->debug(
              "The path ends with the first plugin checked, treating the "
              "following path as the start of the load order: {}",
              PathToString(graph_, pathFromNextVertex.value()));
        }
        for (const auto& pathVertex : pathFromNextVertex.value()) {
          newLoadOrder.push_back(pathVertex);
          processedVertices.insert(pathVertex);
        }
        continue;
      }

      // Ignore the last vertex in the path because it's currentVertex and
      // will just be appended to the load order so doesn't need special
      // processing.
      pathFromNextVertex.value().pop_back();

      // This is used to keep track of when to stop searching for a
      // vertex to load after, as a minor optimisation.
      auto reverseEndIt = newLoadOrder.crend();

      // Iterate over the path going from nextVertex towards currentVertex
      // (which got chopped off the end of the path).
      for (const auto& currentPathVertex : pathFromNextVertex.value()) {
        // Update reverseEndIt to reduce the scope of the search in the
        // next loop (if there is one).
        reverseEndIt = pinVertexPosition(currentPathVertex, reverseEndIt);
      }

      // Add currentVertex to the end of the newLoadOrder - do this after
      // processing the other vertices in the path so that involves less
      // work.
      if (processedVertices.count(currentVertex) == 0) {
        newLoadOrder.push_back(currentVertex);
        processedVertices.insert(currentVertex);
      }
    }
  }
}
}
