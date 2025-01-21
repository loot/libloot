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
#include <boost/container/deque.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/topological_sort.hpp>
#include <queue>

#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "api/sorting/group_sort.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace {
using loot::GroupGraph;
typedef boost::graph_traits<GroupGraph>::vertex_descriptor GroupGraphVertex;

bool IsRootVertex(const GroupGraphVertex& vertex, const GroupGraph& graph) {
  return boost::in_degree(vertex, graph) == 0;
}

class GroupsPathLengthVisitor : public boost::dfs_visitor<> {
public:
  explicit GroupsPathLengthVisitor(size_t& maxPathLength) :
      maxPathLength_(&maxPathLength) {}

  typedef boost::graph_traits<GroupGraph>::edge_descriptor GroupGraphEdge;

  void discover_vertex(GroupGraphVertex, const GroupGraph&) {
    currentPathLength_ += 1;
    if (currentPathLength_ > *maxPathLength_) {
      *maxPathLength_ = currentPathLength_;
    }
  }
  void finish_vertex(GroupGraphVertex, const GroupGraph&) {
    currentPathLength_ -= 1;
  }

private:
  size_t currentPathLength_{0};
  size_t* maxPathLength_{nullptr};
};

void DepthFirstVisit(
    const GroupGraph& graph,
    const boost::graph_traits<GroupGraph>::vertex_descriptor& startingVertex,
    GroupsPathLengthVisitor& visitor) {
  std::vector<boost::default_color_type> colorVec(boost::num_vertices(graph));
  const auto colorMap = boost::make_iterator_property_map(
      colorVec.begin(), boost::get(boost::vertex_index, graph), colorVec.at(0));

  boost::depth_first_visit(graph, startingVertex, visitor, colorMap);
}

// Sort the group vertices so that root vertices come first, in order of
// decreasing path length, but otherwise preserving the existing
// (lexicographical) ordering.
std::vector<GroupGraphVertex> GetSortedGroupVertices(
    const GroupGraph& groupGraph) {
  const auto [gvit, gvitend] = boost::vertices(groupGraph);
  std::vector<GroupGraphVertex> groupVertices{gvit, gvitend};

  // Calculate the max path lengths for root vertices.
  std::unordered_map<GroupGraphVertex, size_t> maxPathLengths;
  for (const auto& groupVertex : groupVertices) {
    if (IsRootVertex(groupVertex, groupGraph)) {
      size_t maxPathLength = 0;
      GroupsPathLengthVisitor visitor(maxPathLength);

      DepthFirstVisit(groupGraph, groupVertex, visitor);

      maxPathLengths.emplace(groupVertex, maxPathLength);
    }
  }

  // Now sort the group vertices.
  std::stable_sort(
      groupVertices.begin(),
      groupVertices.end(),
      [&](const GroupGraphVertex& lhs, const GroupGraphVertex& rhs) {
        return IsRootVertex(lhs, groupGraph) &&
               (!IsRootVertex(rhs, groupGraph) ||
                (maxPathLengths[lhs] > maxPathLengths[rhs]));
      });

  return groupVertices;
}
}

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

std::unordered_map<std::string, std::vector<vertex_t>> GetGroupsPlugins(
    const PluginGraph& graph) {
  std::unordered_map<std::string, std::vector<vertex_t>> groupsPlugins;

  for (const vertex_t& vertex :
       boost::make_iterator_range(graph.GetVertices())) {
    const auto groupName = graph.GetPlugin(vertex).GetGroup();

    const auto groupIt = groupsPlugins.find(groupName);

    if (groupIt == groupsPlugins.end()) {
      groupsPlugins.emplace(groupName, std::vector<vertex_t>({vertex}));
    } else {
      groupIt->second.push_back(vertex);
    }
  }

  return groupsPlugins;
}

boost::graph_traits<GroupGraph>::vertex_descriptor GetDefaultVertex(
    const GroupGraph& graph) {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph))) {
    if (graph[vertex] == Group::DEFAULT_NAME) {
      return vertex;
    }
  }

  throw std::logic_error("Could not find default group in group graph");
}

class GroupsPathVisitor : public boost::dfs_visitor<> {
public:
  typedef boost::graph_traits<GroupGraph>::edge_descriptor GroupGraphEdge;

  explicit GroupsPathVisitor(
      PluginGraph& pluginGraph,
      std::unordered_set<GroupGraphVertex>& finishedVertices,
      const std::unordered_map<std::string, std::vector<vertex_t>>&
          groupsPlugins) :
      pluginGraph_(&pluginGraph),
      groupsPlugins_(&groupsPlugins),
      finishedVertices_(&finishedVertices),
      logger_(getLogger()) {}

  explicit GroupsPathVisitor(
      PluginGraph& pluginGraph,
      std::unordered_set<GroupGraphVertex>& finishedVertices,
      const std::unordered_map<std::string, std::vector<vertex_t>>&
          groupsPlugins,
      const GroupGraphVertex vertexToIgnoreAsSource) :
      pluginGraph_(&pluginGraph),
      groupsPlugins_(&groupsPlugins),
      finishedVertices_(&finishedVertices),
      vertexToIgnoreAsSource_(vertexToIgnoreAsSource),
      logger_(getLogger()) {}

  void tree_edge(GroupGraphEdge edge, const GroupGraph& graph) {
    const auto source = boost::source(edge, graph);
    const auto target = boost::target(edge, graph);

    // Add the edge to the stack so that its providence can be taken into
    // account when adding edges from this source group and previous groups'
    // plugins.
    // Also record the plugins in the edge's source group, unless the source
    // group should be ignored (e.g. because the visitor has been configured
    // to ignore the default group's plugins as sources).
    edgeStack_.push_back(std::make_pair(
        edge,
        ShouldIgnoreSourceVertex(source) ? std::vector<vertex_t>()
                                         : FindPluginsInGroup(source, graph)));

    // Find the plugins in the target group.
    const auto targetPlugins = FindPluginsInGroup(target, graph);

    // Add edges going from all the plugins in the groups in the path being
    // currently walked, to the plugins in the current target group's plugins.
    for (size_t i = 0; i < edgeStack_.size(); i += 1) {
      AddPluginGraphEdges(i, targetPlugins, graph);
    }
  }

  void forward_or_cross_edge(GroupGraphEdge edge, const GroupGraph& graph) {
    // Mark the source vertex as unfinishable, because none of the plugins in
    // in the path so far can have edges added to plugins past the target
    // vertex.
    unfinishableVertices_.insert(boost::source(edge, graph));
  }

  void finish_vertex(GroupGraphVertex vertex, const GroupGraph&) {
    // Now that this vertex's DFS-tree has been fully explored, mark it as
    // finished so that it won't have edges added from its plugins again in a
    // different DFS that uses the same finished vertices set.
    if (vertex != vertexToIgnoreAsSource_ &&
        unfinishableVertices_.count(vertex) == 0) {
      finishedVertices_->insert(vertex);
    }

    // Since this vertex has been fully explored, pop the edge stack to remove
    // the edge that has this vertex as its target.
    PopEdgeStack();
  }

private:
  bool PathToGroupInvolvesUserMetadata(const size_t sourceGroupEdgeStackIndex,
                                       const GroupGraph& graph) const {
    if (sourceGroupEdgeStackIndex >= edgeStack_.size()) {
      throw std::logic_error("Given index is past the end of the path stack");
    }

    // Check if any of the edges in the current stack are user edges, going
    // from the given edge index to the end of the stack.
    const auto begin = std::next(edgeStack_.begin(), sourceGroupEdgeStackIndex);

    return std::any_of(begin, edgeStack_.end(), [&](const auto& entry) {
      return graph[entry.first] == EdgeType::userLoadAfter;
    });
  }

  std::vector<vertex_t> FindPluginsInGroup(const GroupGraphVertex vertex,
                                           const GroupGraph& graph) {
    const auto targetPluginsIt = groupsPlugins_->find(graph[vertex]);
    return targetPluginsIt == groupsPlugins_->end() ? std::vector<vertex_t>()
                                                    : targetPluginsIt->second;
  }

  void AddPluginGraphEdges(const size_t sourceGroupEdgeStackIndex,
                           const std::vector<vertex_t>& toPluginVertices,
                           const GroupGraph& graph) {
    const auto& fromPluginVertices =
        edgeStack_[sourceGroupEdgeStackIndex].second;

    const auto groupPathInvolvesUserMetadata =
        PathToGroupInvolvesUserMetadata(sourceGroupEdgeStackIndex, graph);

    for (const auto& pluginVertex : fromPluginVertices) {
      AddPluginGraphEdges(
          pluginVertex, toPluginVertices, groupPathInvolvesUserMetadata);
    }
  }

  void AddPluginGraphEdges(const vertex_t& fromPluginVertex,
                           const std::vector<vertex_t>& toPluginVertices,
                           const bool groupPathInvolvesUserMetadata) {
    if (toPluginVertices.empty()) {
      return;
    }

    const auto& fromPlugin = pluginGraph_->GetPlugin(fromPluginVertex);

    for (const auto& toVertex : toPluginVertices) {
      const auto& toPlugin = pluginGraph_->GetPlugin(toVertex);

      if (!pluginGraph_->IsPathCached(fromPluginVertex, toVertex) &&
          !pluginGraph_->PathExists(toVertex, fromPluginVertex)) {
        const auto involvesUserMetadata = groupPathInvolvesUserMetadata ||
                                          fromPlugin.IsGroupUserMetadata() ||
                                          toPlugin.IsGroupUserMetadata();

        const auto edgeType = involvesUserMetadata ? EdgeType::userGroup
                                                   : EdgeType::masterlistGroup;

        pluginGraph_->AddEdge(fromPluginVertex, toVertex, edgeType);
      } else {
        if (logger_) {
          logger_->debug(
              "Skipping group edge from \"{}\" to \"{}\" as it would "
              "create a cycle.",
              fromPlugin.GetName(),
              toPlugin.GetName());
        }
      }
    }
  }

  bool ShouldIgnoreSourceVertex(const GroupGraphVertex& groupVertex) {
    return groupVertex == vertexToIgnoreAsSource_ ||
           finishedVertices_->count(groupVertex) == 1;
  }

  void PopEdgeStack() {
    if (!edgeStack_.empty()) {
      edgeStack_.pop_back();
    }
  }

  PluginGraph* pluginGraph_{nullptr};
  std::unordered_set<GroupGraphVertex>* finishedVertices_;
  const std::unordered_map<std::string, std::vector<vertex_t>>* groupsPlugins_{
      nullptr};
  std::optional<GroupGraphVertex> vertexToIgnoreAsSource_;
  std::shared_ptr<spdlog::logger> logger_;

  // This represents the path to the current target vertex in the group graph,
  // along with the plugins in each edge's source vertex (group).
  std::vector<std::pair<GroupGraphEdge, std::vector<vertex_t>>> edgeStack_;

  std::unordered_set<GroupGraphVertex> unfinishableVertices_;
};

void DepthFirstVisit(
    const GroupGraph& graph,
    const boost::graph_traits<GroupGraph>::vertex_descriptor& startingVertex,
    GroupsPathVisitor& visitor) {
  std::vector<boost::default_color_type> colorVec(boost::num_vertices(graph));
  const auto colorMap = boost::make_iterator_property_map(
      colorVec.begin(), boost::get(boost::vertex_index, graph), colorVec.at(0));

  boost::depth_first_visit(graph, startingVertex, visitor, colorMap);
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
  std::queue<vertex_t, boost::container::deque<vertex_t>> forwardQueue;
  std::queue<vertex_t, boost::container::deque<vertex_t>> reverseQueue;
  boost::unordered_flat_set<vertex_t> forwardVisited;
  boost::unordered_flat_set<vertex_t> reverseVisited;

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
  const auto descendants = pathsCache_.find(fromVertex);

  if (descendants == pathsCache_.end()) {
    return false;
  }

  return descendants->second.contains(toVertex);
}

void PathsCache::CachePath(const vertex_t& fromVertex,
                           const vertex_t& toVertex) {
  const auto descendants = pathsCache_.find(fromVertex);

  if (descendants == pathsCache_.end()) {
    pathsCache_.emplace(fromVertex, boost::unordered_flat_set{toVertex});
  } else {
    descendants->second.emplace(toVertex);
  }
}

#if _WIN32
const std::wstring& WideStringsCache::Get(const std::string& narrowString) {
  auto vertexNameIt = wideStringsCache_.find(narrowString);
  if (vertexNameIt == wideStringsCache_.end()) {
    throw std::invalid_argument("Given string was not already cached");
  }

  return vertexNameIt->second;
}

const std::wstring& WideStringsCache::GetOrInsert(
    const std::string& narrowString) {
  auto vertexNameIt = wideStringsCache_.find(narrowString);
  if (vertexNameIt == wideStringsCache_.end()) {
    vertexNameIt =
        wideStringsCache_.emplace(narrowString, ToWinWide(narrowString)).first;
  }

  return vertexNameIt->second;
}

void WideStringsCache::Insert(const std::string& narrowString) {
  if (!wideStringsCache_.contains(narrowString)) {
    wideStringsCache_.emplace(narrowString, ToWinWide(narrowString));
  }
}
#endif

size_t PluginGraph::CountVertices() const {
  return boost::num_vertices(graph_);
}

std::pair<vertex_it, vertex_it> PluginGraph::GetVertices() const {
  return boost::vertices(graph_);
}

std::optional<vertex_t> PluginGraph::GetVertexByName(
    const std::string& name) const {
  for (const auto& vertex : boost::make_iterator_range(GetVertices())) {
#if _WIN32
    const auto& vertexName = GetPlugin(vertex).GetName();
    wideStringCache_.Insert(vertexName);
    auto& wideName = wideStringCache_.GetOrInsert(name);
    auto& wideVertexName = wideStringCache_.Get(vertexName);

    int comparison = CompareFilenames(wideVertexName, wideName);
#else
    int comparison = CompareFilenames(GetPlugin(vertex).GetName(), name);
#endif
    if (comparison == 0) {
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

bool PluginGraph::IsPathCached(const vertex_t& fromVertex,
                               const vertex_t& toVertex) {
  return pathsCache_.IsPathCached(fromVertex, toVertex);
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

void PluginGraph::AddGroupEdges(const GroupGraph& groupGraph) {
  typedef boost::graph_traits<GroupGraph>::vertex_descriptor GroupGraphVertex;

  const auto logger = getLogger();
  if (logger) {
    logger->trace("Adding edges based on plugin group memberships...");
  }

  // First build a map from groups to the plugins in those groups.
  const auto groupsPlugins = GetGroupsPlugins(*this);

  // Get the default group's vertex because it's needed for the DFSes.
  const auto defaultVertex = GetDefaultVertex(groupGraph);

  // The vertex sort order prioritises resolving potential cycles in
  // favour of earlier-loading groups. It does not guarantee that the
  // longest paths will be walked first, because a root vertex may be in
  // more than one path and the vertex sort order here does not influence
  // which path the DFS takes.
  const auto groupVertices = GetSortedGroupVertices(groupGraph);

  // Now loop over the vertices in the groups graph.
  // Keep a record of which vertices have already been fully explored to avoid
  // adding edges from their plugins more than once.
  std::unordered_set<GroupGraphVertex> finishedVertices;
  for (const auto& groupVertex : groupVertices) {
    // Run a DFS from each vertex in the group graph, adding edges except from
    // plugins in the default group. This could be run only on the root
    // vertices, except that the DFS only visits each vertex once, so a branch
    // and merge inside a given root's DAG would result in plugins from one of
    // the branches not being carried forwards past the point at which the
    // branches merge.
    GroupsPathVisitor visitor(
        *this, finishedVertices, groupsPlugins, defaultVertex);

    DepthFirstVisit(groupGraph, groupVertex, visitor);
  }

  // Now do one last DFS starting from the default group and not ignoring its
  // plugins.
  GroupsPathVisitor visitor(*this, finishedVertices, groupsPlugins);

  DepthFirstVisit(groupGraph, defaultVertex, visitor);
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

      if (!IsPathCached(fromVertex, toVertex) &&
          !PathExists(toVertex, fromVertex)) {
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
