/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2016    WrinklyNinja

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

#ifndef LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_GRAPH_TEST
#define LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_GRAPH_TEST

#include <gtest/gtest.h>

#include "api/sorting/group_sort.h"
#include "api/sorting/plugin_graph.h"
#include "loot/exception/cyclic_interaction_error.h"

namespace loot {
namespace test {
namespace plugingraph {
class TestPlugin : public PluginSortingInterface {
public:
  TestPlugin(const std::string& name) : name_(name) {}

  std::string GetName() const override { return name_; }

  std::optional<float> GetHeaderVersion() const override {
    return std::optional<float>();
  }

  std::optional<std::string> GetVersion() const override {
    return std::optional<std::string>();
  }

  std::vector<std::string> GetMasters() const override { return masters_; }

  std::vector<Tag> GetBashTags() const override { return std::vector<Tag>(); }

  std::optional<uint32_t> GetCRC() const override {
    return std::optional<uint32_t>();
  }

  bool IsMaster() const override { return isMaster_; }

  bool IsLightPlugin() const override { return false; }

  bool IsMediumPlugin() const override { return false; }

  bool IsUpdatePlugin() const override { return false; }

  bool IsBlueprintPlugin() const override { return isBlueprintMaster_; }

  bool IsValidAsLightPlugin() const override { return false; }

  bool IsValidAsMediumPlugin() const override { return false; }

  bool IsValidAsUpdatePlugin() const override { return false; }

  bool IsEmpty() const override { return false; }

  bool LoadsArchive() const override { return false; }

  bool DoRecordsOverlap(const PluginInterface& plugin) const override {
    const auto otherPlugin = dynamic_cast<const TestPlugin*>(&plugin);
    return recordsOverlapWith.count(&plugin) != 0 ||
           otherPlugin->recordsOverlapWith.count(this) != 0;
  }

  size_t GetOverrideRecordCount() const override {
    return overrideRecordCount_;
  }

  uint32_t GetRecordAndGroupCount() const override { return uint32_t(); }

  size_t GetAssetCount() const override { return assetCount_; };

  bool DoAssetsOverlap(const PluginSortingInterface& plugin) const override {
    const auto otherPlugin = dynamic_cast<const TestPlugin*>(&plugin);
    return assetsOverlapWith.count(&plugin) != 0 ||
           otherPlugin->assetsOverlapWith.count(this) != 0;
  }

  void AddMaster(const std::string& master) { masters_.push_back(master); }

  void SetIsMaster(bool isMaster) { isMaster_ = isMaster; }

  void SetIsBlueprintMaster(bool isBlueprintMaster) {
    isBlueprintMaster_ = isBlueprintMaster;
  }

  void AddOverlappingRecords(const PluginInterface& plugin) {
    recordsOverlapWith.insert(&plugin);
  }

  void SetOverrideRecordCount(size_t overrideRecordCount) {
    overrideRecordCount_ = overrideRecordCount;
  }

  void AddOverlappingAssets(const PluginSortingInterface& plugin) {
    assetsOverlapWith.insert(&plugin);
  }

  void SetAssetCount(size_t assetCount) { assetCount_ = assetCount; }

private:
  std::string name_;
  std::vector<std::string> masters_;
  std::set<const PluginInterface*> recordsOverlapWith;
  std::set<const PluginSortingInterface*> assetsOverlapWith;
  size_t overrideRecordCount_{0};
  size_t assetCount_{0};
  bool isMaster_{false};
  bool isBlueprintMaster_{false};
};
}

class PluginGraphTest : public ::testing::Test {
protected:
  PluginGraphTest() {
    std::vector<Group> masterlistGroups{Group("A"),
                                        Group("B", {"A"}),
                                        Group("C"),
                                        Group("default", {"C"}),
                                        Group("E", {"default"}),
                                        Group("F", {"E"})};
    std::vector<Group> userlistGroups{Group("C", {"B"})};

    groupsMap = GetGroupsMap(masterlistGroups, userlistGroups);
    const auto groupGraph = BuildGroupGraph(masterlistGroups, userlistGroups);
    predecessorGroupsMap = GetPredecessorGroups(groupGraph);
  }

  static std::unordered_map<std::string, Group> GetGroupsMap(
      const std::vector<Group> masterlistGroups,
      const std::vector<Group> userGroups) {
    const auto mergedGroups = MergeGroups(masterlistGroups, userGroups);

    std::unordered_map<std::string, Group> groupsMap;
    for (const auto& group : mergedGroups) {
      groupsMap.emplace(group.GetName(), group);
    }

    return groupsMap;
  }

  PluginSortingData CreatePluginSortingData(const std::string& name) {
    const auto plugin = GetPlugin(name);

    return PluginSortingData(plugin, PluginMetadata(), PluginMetadata(), {});
  }

  PluginSortingData CreatePluginSortingData(const std::string& name,
                                            const std::string& group,
                                            bool isGroupUserMetadata = false) {
    const auto plugin = GetPlugin(name);

    PluginMetadata masterlistMetadata;
    PluginMetadata userMetadata;

    if (isGroupUserMetadata) {
      userMetadata.SetGroup(group);
    } else {
      masterlistMetadata.SetGroup(group);
    }

    return PluginSortingData(plugin, masterlistMetadata, userMetadata, {});
  }

  plugingraph::TestPlugin* GetPlugin(const std::string& name) {
    auto it = plugins.find(name);

    if (it != plugins.end()) {
      return it->second.get();
    }

    const auto plugin = std::make_shared<plugingraph::TestPlugin>(name);

    return plugins.insert_or_assign(name, plugin).first->second.get();
  }

  std::unordered_map<std::string, Group> groupsMap;
  std::unordered_map<std::string, std::vector<PredecessorGroup>>
      predecessorGroupsMap;

private:
  std::map<std::string, std::shared_ptr<plugingraph::TestPlugin>> plugins;
};

TEST_F(PluginGraphTest, checkForCyclesShouldNotThrowIfThereIsNoCycle) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp"));

  graph.AddEdge(a, b, EdgeType::master);

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest, checkForCyclesShouldThrowIfThereIsACycle) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp"));

  graph.AddEdge(a, b, EdgeType::master);
  graph.AddEdge(b, a, EdgeType::masterFlag);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("A.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("B.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(PluginGraphTest,
       checkForCyclesShouldOnlyRecordPluginsThatArePartOfTheCycle) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp"));

  graph.AddEdge(a, b, EdgeType::master);
  graph.AddEdge(b, c, EdgeType::master);
  graph.AddEdge(b, a, EdgeType::masterFlag);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("A.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("B.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(PluginGraphTest,
       topologicalSortWithNoLoadedPluginsShouldReturnAnEmptyList) {
  PluginGraph graph;
  std::vector<vertex_t> sorted = graph.TopologicalSort();

  EXPECT_TRUE(sorted.empty());
}

TEST_F(
    PluginGraphTest,
    addHardcodedPluginEdgesShouldNotThrowIfThereAreNoVerticesOrHardcodedPlugins) {
  PluginGraph graph;

  EXPECT_NO_THROW(graph.AddHardcodedPluginEdges({}));
}

TEST_F(PluginGraphTest,
       addHardcodedPluginEdgesShouldNotThrowIfThereAreNoVertices) {
  PluginGraph graph;

  const std::vector<std::string> hardcodedPlugins{
      "1.esp", "2.esp", "3.esp", "4.esp"};

  EXPECT_NO_THROW(graph.AddHardcodedPluginEdges(hardcodedPlugins));
}

TEST_F(PluginGraphTest,
       addHardcodedPluginEdgesShouldAddNoEdgesIfThereAreNoHardcodedPlugins) {
  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v3 = graph.AddVertex(CreatePluginSortingData("3.esp"));
  const auto v4 = graph.AddVertex(CreatePluginSortingData("4.esp"));

  graph.AddHardcodedPluginEdges({});

  EXPECT_FALSE(graph.EdgeExists(v1, v3));
  EXPECT_FALSE(graph.EdgeExists(v1, v4));
  EXPECT_FALSE(graph.EdgeExists(v3, v1));
  EXPECT_FALSE(graph.EdgeExists(v3, v4));
  EXPECT_FALSE(graph.EdgeExists(v4, v1));
  EXPECT_FALSE(graph.EdgeExists(v4, v3));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest,
       addHardcodedPluginsShouldNotThrowIfTheOnlyVertexIsAHardcodedPlugin) {
  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));

  const std::vector<std::string> hardcodedPlugins{
      graph.GetPlugin(v1).GetName()};

  EXPECT_NO_THROW(graph.AddHardcodedPluginEdges(hardcodedPlugins));
}

TEST_F(
    PluginGraphTest,
    addHardcodedPluginEdgesShouldAddEdgesBetweenConsecutiveHardcodedPluginsSkippingMissingPlugins) {
  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v3 = graph.AddVertex(CreatePluginSortingData("3.esp"));
  const auto v4 = graph.AddVertex(CreatePluginSortingData("4.esp"));

  const std::vector<std::string> hardcodedPlugins{
      graph.GetPlugin(v1).GetName(),
      "2.esp",
      graph.GetPlugin(v3).GetName(),
      graph.GetPlugin(v4).GetName()};

  graph.AddHardcodedPluginEdges(hardcodedPlugins);

  EXPECT_TRUE(graph.EdgeExists(v1, v3));
  EXPECT_TRUE(graph.EdgeExists(v3, v4));
  EXPECT_FALSE(graph.EdgeExists(v1, v4));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addHardcodedPluginEdgesShouldAddEdgesFromOnlyTheLastInstalledHardcodedPluginToAllNonHardcodedPlugins) {
  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));
  const auto v4 = graph.AddVertex(CreatePluginSortingData("4.esp"));
  const auto v5 = graph.AddVertex(CreatePluginSortingData("5.esp"));

  const std::vector<std::string> hardcodedPlugins{
      graph.GetPlugin(v1).GetName(), graph.GetPlugin(v2).GetName(), "3.esp"};

  graph.AddHardcodedPluginEdges(hardcodedPlugins);

  EXPECT_TRUE(graph.EdgeExists(v1, v2));
  EXPECT_TRUE(graph.EdgeExists(v2, v4));
  EXPECT_TRUE(graph.EdgeExists(v2, v5));
  EXPECT_FALSE(graph.EdgeExists(v1, v4));
  EXPECT_FALSE(graph.EdgeExists(v1, v5));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddUserGroupEdgeIfSourcePluginIsInGroupDueToUserMetadata) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A", true));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Cause a cycle to see the edge types.
  graph.AddEdge(b, a, EdgeType::master);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("A.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userGroup, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("B.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddUserGroupEdgeIfTargetPluginIsInGroupDueToUserMetadata) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B", true));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Cause a cycle to see the edge types.
  graph.AddEdge(b, a, EdgeType::master);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("A.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userGroup, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("B.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldAddUserGroupEdgeIfGroupPathStartsWithUserMetadata) {
  PluginGraph graph;

  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Cause a cycle to see the edge types.
  graph.AddEdge(d, b, EdgeType::master);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("B.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userGroup, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("D.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldAddUserGroupEdgeIfGroupPathEndsWithUserMetadata) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Cause a cycle to see the edge types.
  graph.AddEdge(c, a, EdgeType::master);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("A.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userGroup, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("C.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldAddUserGroupEdgeIfGroupPathInvolvesUserMetadata) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Cause a cycle to see the edge types.
  graph.AddEdge(d, a, EdgeType::master);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("A.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userGroup, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("D.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldAddMasterlistGroupEdgeIfNoUserMetadataIsInvolved) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Cause a cycle to see the edge types.
  graph.AddEdge(b, a, EdgeType::master);

  try {
    graph.CheckForCycles();
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("A.esp", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistGroup,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("B.esp", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddEdgesBetweenPluginsInIndirectlyConnectedGroups_whenAnIntermediatePluginEdgeIsSkipped) {
  PluginGraph graph;

  const auto a1 = graph.AddVertex(CreatePluginSortingData("A1.esp", "A"));
  const auto a2 = graph.AddVertex(CreatePluginSortingData("A2.esp", "A"));
  const auto b1 = graph.AddVertex(CreatePluginSortingData("B1.esp", "B"));
  const auto b2 = graph.AddVertex(CreatePluginSortingData("B2.esp", "B"));
  const auto c1 = graph.AddVertex(CreatePluginSortingData("C1.esp", "C"));
  const auto c2 = graph.AddVertex(CreatePluginSortingData("C2.esp", "C"));

  graph.AddEdge(b1, a1, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A2.esp -> B1.esp -> A1.esp -> B2.esp -> C1.esp
  //                                                -> C2.esp
  EXPECT_TRUE(graph.EdgeExists(b1, a1));
  EXPECT_TRUE(graph.EdgeExists(a1, b2));
  EXPECT_TRUE(graph.EdgeExists(a2, b1));
  EXPECT_TRUE(graph.EdgeExists(a2, b2));
  EXPECT_TRUE(graph.EdgeExists(b1, c1));
  EXPECT_TRUE(graph.EdgeExists(b1, c2));
  EXPECT_TRUE(graph.EdgeExists(b2, c1));
  EXPECT_TRUE(graph.EdgeExists(b2, c2));
  EXPECT_TRUE(graph.EdgeExists(a1, c1));
  EXPECT_TRUE(graph.EdgeExists(a1, c2));
  EXPECT_FALSE(graph.EdgeExists(c1, c2));
  EXPECT_FALSE(graph.EdgeExists(c2, c1));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest, addGroupEdgesShouldAddEdgesAcrossEmptyGroups) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> C.esp
  EXPECT_TRUE(graph.EdgeExists(a, c));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldAddEdgesAcrossTheNonEmptyDefaultGroup) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> D.esp -> E.esp
  //                 ---------->
  EXPECT_TRUE(graph.EdgeExists(a, d));
  EXPECT_TRUE(graph.EdgeExists(d, e));
  EXPECT_TRUE(graph.EdgeExists(a, e));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest, addGroupEdgesShouldSkipAnEdgeThatWouldCauseACycle) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));

  graph.AddEdge(c, a, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be C.esp -> A.esp
  EXPECT_TRUE(graph.EdgeExists(c, a));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesDoesNotSkipAnEdgeThatCausesACycleInvolvingOtherNonDefaultGroups) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));

  graph.AddEdge(c, a, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be C.esp -> A.esp -> B.esp
  EXPECT_TRUE(graph.EdgeExists(c, a));
  EXPECT_TRUE(graph.EdgeExists(a, b));

  // FIXME: This should not cause a cycle.
  try {
    graph.CheckForCycles();
    FAIL();
  } catch (CyclicInteractionError& e) {
    ASSERT_EQ(3, e.GetCycle().size());
    EXPECT_EQ(graph.GetPlugin(a).GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistGroup,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(graph.GetPlugin(b).GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::userGroup, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(graph.GetPlugin(c).GetName(), e.GetCycle()[2].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[2].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldSkipOnlyEdgesToTheTargetGroupPluginsThatWouldCauseACycle) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto c1 = graph.AddVertex(CreatePluginSortingData("C1.esp", "C"));
  const auto c2 = graph.AddVertex(CreatePluginSortingData("C2.esp", "C"));

  graph.AddEdge(c1, a, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be C1.esp -> A.esp -> C2.esp
  EXPECT_TRUE(graph.EdgeExists(c1, a));
  EXPECT_TRUE(graph.EdgeExists(a, c2));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldSkipOnlyEdgesFromAncestorsToTheTargetGroupPluginsThatWouldCauseACycle) {
  PluginGraph graph;

  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d1 = graph.AddVertex(CreatePluginSortingData("D1.esp"));
  const auto d2 = graph.AddVertex(CreatePluginSortingData("D2.esp"));
  const auto d3 = graph.AddVertex(CreatePluginSortingData("D3.esp"));

  graph.AddEdge(d1, b, EdgeType::masterFlag);
  graph.AddEdge(d2, b, EdgeType::masterFlag);
  graph.AddEdge(c, b, EdgeType::masterFlag);
  graph.AddEdge(c, d2, EdgeType::master);
  graph.AddEdge(c, d3, EdgeType::masterFlag);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be: C.esp -> D2.esp -> B.esp -> D3.esp
  //                  -> D1.esp ->
  //                  -------------------->
  //                  ----------->
  EXPECT_TRUE(graph.EdgeExists(d1, b));
  EXPECT_TRUE(graph.EdgeExists(d2, b));
  EXPECT_TRUE(graph.EdgeExists(c, b));
  EXPECT_TRUE(graph.EdgeExists(c, d2));
  EXPECT_TRUE(graph.EdgeExists(c, d3));

  EXPECT_TRUE(graph.EdgeExists(b, d3));

  // FIXME: This edge should be added but isn't, it's a limitation of the
  // current implementation.
  EXPECT_FALSE(graph.EdgeExists(c, d1));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddAPluginsEdgesAcrossASuccessorIfAtLeastOneEdgeToTheSuccessorGroupWasSkipped_successiveDepths) {
  PluginGraph graph;

  const auto a1 = graph.AddVertex(CreatePluginSortingData("A1.esp", "A"));
  const auto a2 = graph.AddVertex(CreatePluginSortingData("A2.esp", "A"));
  const auto b1 = graph.AddVertex(CreatePluginSortingData("B1.esp", "B"));
  const auto b2 = graph.AddVertex(CreatePluginSortingData("B2.esp", "B"));
  const auto c1 = graph.AddVertex(CreatePluginSortingData("C1.esp", "C"));
  const auto c2 = graph.AddVertex(CreatePluginSortingData("C2.esp", "C"));

  graph.AddEdge(b1, a1, EdgeType::master);
  graph.AddEdge(c1, b2, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A2.esp -> B1.esp -> A1.esp -> C1.esp -> B2.esp -> C2.esp
  EXPECT_TRUE(graph.EdgeExists(b1, a1));
  EXPECT_TRUE(graph.EdgeExists(c1, b2));
  EXPECT_TRUE(graph.EdgeExists(a1, b2));
  EXPECT_TRUE(graph.EdgeExists(a1, c1));
  EXPECT_TRUE(graph.EdgeExists(a1, c2));
  EXPECT_TRUE(graph.EdgeExists(a2, b1));
  EXPECT_TRUE(graph.EdgeExists(a2, b2));
  EXPECT_TRUE(graph.EdgeExists(b1, c1));
  EXPECT_TRUE(graph.EdgeExists(b1, c2));
  EXPECT_TRUE(graph.EdgeExists(b2, c2));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddAPluginsEdgesAcrossASuccessorIfAtLeastOneEdgeToTheSuccessorGroupWasSkipped_successiveDepthsDifferentOrder) {
  PluginGraph graph;

  const auto a1 = graph.AddVertex(CreatePluginSortingData("A1.esp", "A"));
  const auto a2 = graph.AddVertex(CreatePluginSortingData("A2.esp", "A"));
  const auto b1 = graph.AddVertex(CreatePluginSortingData("B1.esp", "B"));
  const auto b2 = graph.AddVertex(CreatePluginSortingData("B2.esp", "B"));
  const auto c1 = graph.AddVertex(CreatePluginSortingData("C1.esp", "C"));
  const auto c2 = graph.AddVertex(CreatePluginSortingData("C2.esp", "C"));

  graph.AddEdge(b1, a1, EdgeType::master);
  graph.AddEdge(c1, b1, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A2.esp -> C1.esp -> B1.esp -> A1.esp -> B2.esp -> C2.esp
  EXPECT_TRUE(graph.EdgeExists(b1, a1));
  EXPECT_TRUE(graph.EdgeExists(c1, b1));
  EXPECT_TRUE(graph.EdgeExists(a1, b2));
  EXPECT_TRUE(graph.EdgeExists(a2, b1));
  EXPECT_TRUE(graph.EdgeExists(a2, b2));
  EXPECT_TRUE(graph.EdgeExists(a1, c2));
  EXPECT_TRUE(graph.EdgeExists(b1, c2));
  EXPECT_TRUE(graph.EdgeExists(b2, c2));
  EXPECT_TRUE(graph.EdgeExists(a2, c1));

  // FIXME: This edge is unwanted and causes a cycle.
  EXPECT_TRUE(graph.EdgeExists(b2, c1));

  EXPECT_THROW(graph.CheckForCycles(), CyclicInteractionError);
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddEdgeFromAncestorToSuccessorIfNoneOfAGroupsPluginsCan_simple) {
  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b1 = graph.AddVertex(CreatePluginSortingData("B1.esp", "B"));
  const auto b2 = graph.AddVertex(CreatePluginSortingData("B2.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));

  graph.AddEdge(c, b1, EdgeType::master);
  graph.AddEdge(c, b2, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> C1.esp -> B1.esp
  //                           -> B2.esp
  EXPECT_TRUE(graph.EdgeExists(a, b1));
  EXPECT_TRUE(graph.EdgeExists(a, b2));
  EXPECT_TRUE(graph.EdgeExists(c, b1));
  EXPECT_TRUE(graph.EdgeExists(c, b2));
  EXPECT_TRUE(graph.EdgeExists(a, c));
  EXPECT_FALSE(graph.EdgeExists(b1, b2));
  EXPECT_FALSE(graph.EdgeExists(b2, b1));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddEdgeFromAncestorToSuccessorIfNoneOfAGroupsPluginsCan_withEdgesAcrossedTheSkippedGroup) {
  PluginGraph graph;

  const auto a1 = graph.AddVertex(CreatePluginSortingData("A1.esp", "A"));
  const auto a2 = graph.AddVertex(CreatePluginSortingData("A2.esp", "A"));
  const auto b1 = graph.AddVertex(CreatePluginSortingData("B1.esp", "B"));
  const auto b2 = graph.AddVertex(CreatePluginSortingData("B2.esp", "B"));
  const auto c1 = graph.AddVertex(CreatePluginSortingData("C1.esp", "C"));
  const auto c2 = graph.AddVertex(CreatePluginSortingData("C2.esp", "C"));
  const auto d1 = graph.AddVertex(CreatePluginSortingData("D1.esp"));
  const auto d2 = graph.AddVertex(CreatePluginSortingData("D2.esp"));

  graph.AddEdge(b1, a1, EdgeType::master);
  graph.AddEdge(c1, b1, EdgeType::master);
  graph.AddEdge(d1, c1, EdgeType::master);
  graph.AddEdge(d2, c1, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be:
  // A2.esp -> D1.esp -> C1.esp -> B1.esp -> A1.esp -> B2.esp -> C2.esp
  //        -> D2.esp ->
  EXPECT_TRUE(graph.EdgeExists(b1, a1));
  EXPECT_TRUE(graph.EdgeExists(c1, b1));
  EXPECT_TRUE(graph.EdgeExists(d1, c1));
  EXPECT_TRUE(graph.EdgeExists(d2, c1));
  EXPECT_TRUE(graph.EdgeExists(a1, b2));
  EXPECT_TRUE(graph.EdgeExists(a2, b1));
  EXPECT_TRUE(graph.EdgeExists(a2, b2));
  EXPECT_TRUE(graph.EdgeExists(a1, c2));
  EXPECT_TRUE(graph.EdgeExists(b1, c2));
  EXPECT_TRUE(graph.EdgeExists(a2, c1));
  EXPECT_TRUE(graph.EdgeExists(a2, d1));
  EXPECT_TRUE(graph.EdgeExists(a2, d2));
  EXPECT_FALSE(graph.EdgeExists(d1, d2));
  EXPECT_FALSE(graph.EdgeExists(d2, d1));

  // FIXME: This edge is unwanted and causes a cycle.
  EXPECT_TRUE(graph.EdgeExists(b2, c1));

  EXPECT_THROW(graph.CheckForCycles(), CyclicInteractionError);
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldDeprioritiseEdgesFromDefaultGroupPlugins_defaultLast) {
  PluginGraph graph;

  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));

  graph.AddEdge(d, b, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be D.esp -> B.esp -> C.esp
  EXPECT_TRUE(graph.EdgeExists(b, c));
  EXPECT_TRUE(graph.EdgeExists(d, b));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldDeprioritiseEdgesFromDefaultGroupPlugins_defaultFirst) {
  PluginGraph graph;

  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));
  const auto f = graph.AddVertex(CreatePluginSortingData("F.esp", "F"));

  graph.AddEdge(f, d, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be E.esp -> F.esp -> D.esp
  EXPECT_TRUE(graph.EdgeExists(e, f));
  EXPECT_TRUE(graph.EdgeExists(f, d));
  EXPECT_FALSE(graph.EdgeExists(d, e));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldDeprioritiseEdgesFromDefaultGroupPlugins_acrossSkippedIntermediateGroups) {
  PluginGraph graph;

  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));
  const auto f = graph.AddVertex(CreatePluginSortingData("F.esp", "F"));

  graph.AddEdge(e, d, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be E.esp -> D.esp -> F.esp
  EXPECT_TRUE(graph.EdgeExists(e, d));
  EXPECT_TRUE(graph.EdgeExists(d, f));
  EXPECT_FALSE(graph.EdgeExists(f, e));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldDeprioritiseEdgesFromDefaultGroupPlugins_d1Firstd2Last) {
  PluginGraph graph;

  const auto d1 = graph.AddVertex(CreatePluginSortingData("D1.esp"));
  const auto d2 = graph.AddVertex(CreatePluginSortingData("D2.esp"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));
  const auto f = graph.AddVertex(CreatePluginSortingData("F.esp", "F"));

  graph.AddEdge(f, d2, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be D1.esp -> E.esp -> F.esp -> D2.esp
  EXPECT_TRUE(graph.EdgeExists(e, f));
  EXPECT_TRUE(graph.EdgeExists(f, d2));
  EXPECT_TRUE(graph.EdgeExists(d1, e));
  EXPECT_FALSE(graph.EdgeExists(d2, d1));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldDeprioritiseEdgesFromDefaultGroupPlugins_noIdealResult) {
  PluginGraph graph;

  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));
  const auto f = graph.AddVertex(CreatePluginSortingData("F.esp", "F"));

  graph.AddEdge(d, b, EdgeType::master);
  graph.AddEdge(f, d, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // No ideal result, expected is F.esp -> D.esp -> B.esp -> C.esp -> E.esp
  EXPECT_TRUE(graph.EdgeExists(f, d));
  EXPECT_TRUE(graph.EdgeExists(d, b));
  EXPECT_TRUE(graph.EdgeExists(b, c));
  EXPECT_TRUE(graph.EdgeExists(c, e));

  // FIXME: This edge is unwanted and causes a cycle.
  EXPECT_TRUE(graph.EdgeExists(e, f));

  EXPECT_THROW(graph.CheckForCycles(), CyclicInteractionError);
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldDeprioritiseEdgesFromDefaultGroupPlugins_defaultInMiddleDBookends) {
  PluginGraph graph;

  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d1 = graph.AddVertex(CreatePluginSortingData("D1.esp"));
  const auto d2 = graph.AddVertex(CreatePluginSortingData("D2.esp"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));
  const auto f = graph.AddVertex(CreatePluginSortingData("F.esp", "F"));

  graph.AddEdge(d2, b, EdgeType::master);
  graph.AddEdge(f, d1, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be D2.esp -> B.esp -> C.esp -> E.esp -> F.esp -> D1.esp
  EXPECT_TRUE(graph.EdgeExists(d2, b));
  EXPECT_TRUE(graph.EdgeExists(b, c));
  EXPECT_TRUE(graph.EdgeExists(c, e));
  EXPECT_TRUE(graph.EdgeExists(e, f));
  EXPECT_TRUE(graph.EdgeExists(f, d1));
  EXPECT_FALSE(graph.EdgeExists(d1, d2));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldDeprioritiseEdgesFromDefaultGroupPlugins_defaultInMiddleDThroughout) {
  PluginGraph graph;

  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d1 = graph.AddVertex(CreatePluginSortingData("D1.esp"));
  const auto d2 = graph.AddVertex(CreatePluginSortingData("D2.esp"));
  const auto d3 = graph.AddVertex(CreatePluginSortingData("D3.esp"));
  const auto d4 = graph.AddVertex(CreatePluginSortingData("D4.esp"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));
  const auto f = graph.AddVertex(CreatePluginSortingData("F.esp", "F"));

  graph.AddEdge(d2, b, EdgeType::master);
  graph.AddEdge(d4, c, EdgeType::master);
  graph.AddEdge(f, d1, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be:
  // D2.esp -> B.esp -> D4.esp -> C.esp -> D3.esp -> E.esp -> F.esp -> D1.esp
  EXPECT_TRUE(graph.EdgeExists(d2, b));
  EXPECT_TRUE(graph.EdgeExists(b, c));
  EXPECT_TRUE(graph.EdgeExists(c, d3));
  EXPECT_TRUE(graph.EdgeExists(c, e));
  EXPECT_TRUE(graph.EdgeExists(d3, e));
  EXPECT_TRUE(graph.EdgeExists(e, f));
  EXPECT_TRUE(graph.EdgeExists(f, d1));
  EXPECT_TRUE(graph.EdgeExists(d4, c));
  EXPECT_TRUE(graph.EdgeExists(b, d4));
  EXPECT_FALSE(graph.EdgeExists(d1, d2));
  EXPECT_FALSE(graph.EdgeExists(d1, d3));
  EXPECT_FALSE(graph.EdgeExists(d1, d4));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldHandleAsymmetricBranchesInTheGroupsGraph) {
  std::vector<Group> masterlistGroups{Group("A"),
                                      Group("B", {"A"}),
                                      Group("C", {"B"}),
                                      Group("D", {"A"}),
                                      Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> B.esp -> C.esp
  //                 -> D.esp
  EXPECT_TRUE(graph.EdgeExists(a, b));
  EXPECT_TRUE(graph.EdgeExists(b, c));
  EXPECT_TRUE(graph.EdgeExists(a, d));
  EXPECT_FALSE(graph.EdgeExists(d, b));
  EXPECT_FALSE(graph.EdgeExists(d, c));
  EXPECT_FALSE(graph.EdgeExists(b, d));
  EXPECT_FALSE(graph.EdgeExists(c, d));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldHandleAsymmetricBranchesInTheGroupsGraphThatMerge) {
  std::vector<Group> masterlistGroups{Group("A"),
                                      Group("B", {"A"}),
                                      Group("C", {"B"}),
                                      Group("D", {"A"}),
                                      Group("E", {"C", "D"}),
                                      Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> B.esp -> C.esp -> E.esp
  //                 -> D.esp ---------->
  EXPECT_TRUE(graph.EdgeExists(a, b));
  EXPECT_TRUE(graph.EdgeExists(b, c));
  EXPECT_TRUE(graph.EdgeExists(c, e));
  EXPECT_TRUE(graph.EdgeExists(a, d));
  EXPECT_TRUE(graph.EdgeExists(d, e));
  EXPECT_FALSE(graph.EdgeExists(d, b));
  EXPECT_FALSE(graph.EdgeExists(d, c));
  EXPECT_FALSE(graph.EdgeExists(b, d));
  EXPECT_FALSE(graph.EdgeExists(c, d));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldHandleBranchesInTheGroupsGraphThatFormADiamondPattern) {
  std::vector<Group> masterlistGroups{Group("A"),
                                      Group("B", {"A"}),
                                      Group("C", {"A"}),
                                      Group("D", {"B", "C"}),
                                      Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> B.esp -> D.esp
  //                 -> C.esp ->
  EXPECT_TRUE(graph.EdgeExists(a, b));
  EXPECT_TRUE(graph.EdgeExists(b, d));
  EXPECT_TRUE(graph.EdgeExists(a, c));
  EXPECT_TRUE(graph.EdgeExists(c, d));
  EXPECT_FALSE(graph.EdgeExists(b, c));
  EXPECT_FALSE(graph.EdgeExists(c, b));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldAddEdgesAcrossTheMergePointOfBranchesInTheGroupsGraph) {
  std::vector<Group> masterlistGroups{Group("A"),
                                      Group("B", {"A"}),
                                      Group("C", {"A"}),
                                      Group("D", {"B", "C"}),
                                      Group("E", {"D"}),
                                      Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));

  graph.AddEdge(d, c, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> B.esp -> D.esp -> C.esp -> E.esp
  EXPECT_TRUE(graph.EdgeExists(d, c));
  EXPECT_TRUE(graph.EdgeExists(a, b));
  EXPECT_TRUE(graph.EdgeExists(b, d));
  EXPECT_TRUE(graph.EdgeExists(d, e));
  EXPECT_TRUE(graph.EdgeExists(a, c));
  EXPECT_TRUE(graph.EdgeExists(c, e));
  EXPECT_FALSE(graph.EdgeExists(b, c));
  EXPECT_FALSE(graph.EdgeExists(c, b));
  EXPECT_FALSE(graph.EdgeExists(c, d));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldHandleAGroupGraphWithMultipleSuccessiveBranches) {
  PluginGraph graph;

  std::vector<Group> masterlistGroups{Group("A"),
                                      Group("B", {"A"}),
                                      Group("C", {"A"}),
                                      Group("D", {"B", "C"}),
                                      Group("E", {"D"}),
                                      Group("F", {"D"}),
                                      Group("G", {"E", "F"}),
                                      Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));
  const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));
  const auto f = graph.AddVertex(CreatePluginSortingData("F.esp", "F"));
  const auto g = graph.AddVertex(CreatePluginSortingData("G.esp", "G"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be:
  // A.esp -> B.esp -> D.esp -> E.esp -> G.esp
  //       -> C.esp ->       -> F.esp ->
  EXPECT_TRUE(graph.EdgeExists(a, b));
  EXPECT_TRUE(graph.EdgeExists(a, c));
  EXPECT_TRUE(graph.EdgeExists(b, d));
  EXPECT_TRUE(graph.EdgeExists(c, d));
  EXPECT_TRUE(graph.EdgeExists(d, e));
  EXPECT_TRUE(graph.EdgeExists(d, f));
  EXPECT_TRUE(graph.EdgeExists(e, g));
  EXPECT_TRUE(graph.EdgeExists(f, g));

  EXPECT_FALSE(graph.EdgeExists(b, c));
  EXPECT_FALSE(graph.EdgeExists(c, b));
  EXPECT_FALSE(graph.EdgeExists(e, f));
  EXPECT_FALSE(graph.EdgeExists(f, e));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest, addGroupEdgesShouldHandleIsolatedGroups) {
  std::vector<Group> masterlistGroups{
      Group("A"), Group("B", {"A"}), Group("C"), Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> B.esp
  //           C.esp
  EXPECT_TRUE(graph.EdgeExists(a, b));
  EXPECT_FALSE(graph.EdgeExists(a, c));
  EXPECT_FALSE(graph.EdgeExists(c, a));
  EXPECT_FALSE(graph.EdgeExists(b, c));
  EXPECT_FALSE(graph.EdgeExists(c, b));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest, addGroupEdgesShouldHandleDisconnectedGroupGraphs) {
  std::vector<Group> masterlistGroups{
      Group("A"), Group("B", {"A"}), Group("C"), Group("D", {"C"}), Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> B.esp
  //           C.esp -> D.esp
  EXPECT_TRUE(graph.EdgeExists(a, b));
  EXPECT_TRUE(graph.EdgeExists(c, d));
  EXPECT_FALSE(graph.EdgeExists(a, c));
  EXPECT_FALSE(graph.EdgeExists(a, d));
  EXPECT_FALSE(graph.EdgeExists(b, c));
  EXPECT_FALSE(graph.EdgeExists(b, d));
  EXPECT_FALSE(graph.EdgeExists(c, a));
  EXPECT_FALSE(graph.EdgeExists(c, b));
  EXPECT_FALSE(graph.EdgeExists(d, a));
  EXPECT_FALSE(graph.EdgeExists(d, b));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldAddEdgesAcrossTheMergePointOfTwoRootVertexPaths) {
  std::vector<Group> masterlistGroups{Group("A"),
                                      Group("B"),
                                      Group("C", {"A", "B"}),
                                      Group("D", {"C"}),
                                      Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  PluginGraph graph;

  const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
  const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
  const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
  const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));

  graph.AddEdge(c, b, EdgeType::master);

  graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

  // Should be A.esp -> C.esp -> D.esp
  //           B.esp ---------->
  EXPECT_TRUE(graph.EdgeExists(c, b));
  EXPECT_TRUE(graph.EdgeExists(a, c));
  EXPECT_TRUE(graph.EdgeExists(c, d));
  EXPECT_TRUE(graph.EdgeExists(b, d));
  EXPECT_FALSE(graph.EdgeExists(a, b));
  EXPECT_FALSE(graph.EdgeExists(b, a));

  EXPECT_NO_THROW(graph.CheckForCycles());
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldNotDependOnGroupDefinitionOrderIfThereIsASingleLinearPath) {
  std::vector<std::vector<Group>> masterlistsGroups{
      {Group("B"), Group("C", {"B"}), Group("default", {"C"})},
      {Group("C", {"B"}), Group("B"), Group("default", {"C"})}};

  for (const auto& masterlistGroups : masterlistsGroups) {
    groupsMap = GetGroupsMap(masterlistGroups, {});
    const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
    predecessorGroupsMap = GetPredecessorGroups(groupGraph);

    PluginGraph graph;

    const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
    const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
    const auto d = graph.AddVertex(CreatePluginSortingData("D.esp"));

    graph.AddEdge(d, b, EdgeType::master);

    graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

    // Should be D.esp -> B.esp -> C.esp
    EXPECT_TRUE(graph.EdgeExists(b, c));
    EXPECT_TRUE(graph.EdgeExists(d, b));
    EXPECT_FALSE(graph.EdgeExists(c, d));

    EXPECT_NO_THROW(graph.CheckForCycles());
  }
}

TEST_F(
    PluginGraphTest,
    addGroupEdgesShouldNotDependOnGroupDefinitionOrderIfThereAreMultipleRoots) {
  std::vector<std::vector<Group>> orders{
      // Create a graph with groups in one order.
      {Group("A"),
       Group("B"),
       Group("C", {"A", "B"}),
       Group("D", {"C"}),
       Group()},
      // Now do the same again, but with a different group order for A and B.
      {Group("B"),
       Group("A"),
       Group("C", {"A", "B"}),
       Group("D", {"C"}),
       Group()}};
  for (const auto& masterlistGroups : orders) {
    groupsMap = GetGroupsMap(masterlistGroups, {});
    const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
    predecessorGroupsMap = GetPredecessorGroups(groupGraph);

    PluginGraph graph;

    const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
    const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
    const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
    const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));

    graph.AddEdge(d, a, EdgeType::master);

    graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

    // Should be B.esp -> D.esp -> A.esp -> C.esp
    //           B.esp ------------------->
    EXPECT_TRUE(graph.EdgeExists(d, a));
    EXPECT_TRUE(graph.EdgeExists(a, c));
    EXPECT_TRUE(graph.EdgeExists(b, c));
    EXPECT_TRUE(graph.EdgeExists(b, d));
    EXPECT_FALSE(graph.EdgeExists(a, b));
    EXPECT_FALSE(graph.EdgeExists(b, a));

    // FIXME: This edge is unwanted and causes a cycle.
    EXPECT_TRUE(graph.EdgeExists(c, d));

    EXPECT_THROW(graph.CheckForCycles(), CyclicInteractionError);
  }
}

TEST_F(PluginGraphTest,
       addGroupEdgesShouldNotDependOnBranchingGroupDefinitionOrder) {
  std::vector<std::vector<Group>> orders{
      // Create a graph with groups in one order.
      {Group("A"),
       Group("B", {"A"}),
       Group("C", {"A"}),
       Group("D", {"B", "C"}),
       Group("E", {"D"}),
       Group()},
      // Now do the same again, but with a different group order for B and C.
      {Group("A"),
       Group("C", {"A"}),
       Group("B", {"A"}),
       Group("D", {"B", "C"}),
       Group("E", {"D"}),
       Group()}};
  for (const auto& masterlistGroups : orders) {
    groupsMap = GetGroupsMap(masterlistGroups, {});
    const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
    predecessorGroupsMap = GetPredecessorGroups(groupGraph);

    PluginGraph graph;

    const auto a = graph.AddVertex(CreatePluginSortingData("A.esp", "A"));
    const auto b = graph.AddVertex(CreatePluginSortingData("B.esp", "B"));
    const auto c = graph.AddVertex(CreatePluginSortingData("C.esp", "C"));
    const auto d = graph.AddVertex(CreatePluginSortingData("D.esp", "D"));
    const auto e = graph.AddVertex(CreatePluginSortingData("E.esp", "E"));

    graph.AddEdge(e, c, EdgeType::master);

    graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

    // Should be A.esp -> B.esp -> D.esp -> E.esp -> C.esp
    EXPECT_TRUE(graph.EdgeExists(a, b));
    EXPECT_TRUE(graph.EdgeExists(a, c));
    EXPECT_TRUE(graph.EdgeExists(a, d));
    EXPECT_TRUE(graph.EdgeExists(a, e));
    EXPECT_TRUE(graph.EdgeExists(b, d));
    EXPECT_TRUE(graph.EdgeExists(b, e));
    EXPECT_TRUE(graph.EdgeExists(d, e));
    EXPECT_TRUE(graph.EdgeExists(e, c));

    EXPECT_FALSE(graph.EdgeExists(b, c));
    EXPECT_FALSE(graph.EdgeExists(c, b));
    EXPECT_FALSE(graph.EdgeExists(c, e));
    EXPECT_FALSE(graph.EdgeExists(d, c));

    // FIXME: This edge is unwanted and causes a cycle.
    EXPECT_TRUE(graph.EdgeExists(c, d));

    EXPECT_THROW(graph.CheckForCycles(), CyclicInteractionError);
  }
}

TEST_F(PluginGraphTest, addGroupEdgesShouldNotDependOnPluginGraphVertexOrder) {
  std::vector<Group> masterlistGroups{
      Group("A"), Group("B", {"A"}), Group("C", {"B"}), Group()};

  groupsMap = GetGroupsMap(masterlistGroups, {});
  const auto groupGraph = BuildGroupGraph(masterlistGroups, {});
  predecessorGroupsMap = GetPredecessorGroups(groupGraph);

  const auto a1Data = CreatePluginSortingData("A1.esp", "A");
  const auto a2Data = CreatePluginSortingData("A2.esp", "A");
  const auto bData = CreatePluginSortingData("B.esp", "B");
  const auto cData = CreatePluginSortingData("C.esp", "C");

  const std::vector<std::vector<PluginSortingData>> variations{
      {a1Data, a2Data, bData, cData}, {a1Data, a2Data, cData, bData},
      {a1Data, bData, cData, a2Data}, {a1Data, bData, a2Data, cData},
      {a1Data, cData, a2Data, bData}, {a1Data, cData, bData, a2Data},

      {a2Data, a1Data, bData, cData}, {a2Data, a1Data, cData, bData},
      {a2Data, bData, cData, a1Data}, {a2Data, bData, a1Data, cData},
      {a2Data, cData, a1Data, bData}, {a2Data, cData, bData, a1Data},

      {bData, a2Data, a1Data, cData}, {bData, a2Data, cData, a1Data},
      {bData, a1Data, cData, a2Data}, {bData, a1Data, a2Data, cData},
      {bData, cData, a2Data, a1Data}, {bData, cData, a1Data, a2Data},

      {cData, a2Data, bData, a1Data}, {cData, a2Data, a1Data, bData},
      {cData, bData, a1Data, a2Data}, {cData, bData, a2Data, a1Data},
      {cData, a1Data, a2Data, bData}, {cData, a1Data, bData, a2Data},
  };

  for (const auto& pluginsSortingData : variations) {
    PluginGraph graph;

    for (const auto& plugin : pluginsSortingData) {
      graph.AddVertex(plugin);
    }

    const auto a1 = graph.GetVertexByName("A1.esp").value();
    const auto a2 = graph.GetVertexByName("A2.esp").value();
    const auto b = graph.GetVertexByName("B.esp").value();
    const auto c = graph.GetVertexByName("C.esp").value();

    graph.AddEdge(c, a1, EdgeType::master);

    graph.AddGroupEdges(groupsMap, predecessorGroupsMap);

    // Should be A2.esp -> C.esp -> A1.esp -> B.esp
    //           A2.esp -------------------->
    ASSERT_TRUE(graph.EdgeExists(c, a1));
    ASSERT_TRUE(graph.EdgeExists(a1, b));
    ASSERT_TRUE(graph.EdgeExists(a2, b));
    ASSERT_TRUE(graph.EdgeExists(a2, c));
    ASSERT_FALSE(graph.EdgeExists(a1, a2));
    ASSERT_FALSE(graph.EdgeExists(a2, a1));

    // FIXME: This edge is unwanted and causes a cycle.
    ASSERT_TRUE(graph.EdgeExists(b, c));

    EXPECT_THROW(graph.CheckForCycles(), CyclicInteractionError);
  }
}

TEST_F(PluginGraphTest,
       addOverlapEdgesShouldNotAddEdgesBetweenNonOverlappingPlugins) {
  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_FALSE(graph.EdgeExists(v1, v2));
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldNotAddEdgeBetweenPluginsWithOverlappingRecordsAndEqualOverrideCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->AddOverlappingRecords(*p2);
  p1->SetOverrideRecordCount(1);
  p2->SetOverrideRecordCount(1);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_FALSE(graph.EdgeExists(v1, v2));
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldAddEdgeBetweenPluginsWithOverlappingRecordsAndInequalOverrideCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->AddOverlappingRecords(*p2);
  p1->SetOverrideRecordCount(2);
  p2->SetOverrideRecordCount(1);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_EQ(EdgeType::recordOverlap, graph.GetEdgeType(v1, v2).value());
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldNotAddEdgeBetweenPluginsWithNonOverlappingRecordsAndInequalOverrideCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetOverrideRecordCount(2);
  p2->SetOverrideRecordCount(1);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_FALSE(graph.EdgeExists(v1, v2));
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldNotAddEdgeBetweenPluginsWithAssetOverlapAndEqualAssetCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->AddOverlappingAssets(*p2);
  p1->SetAssetCount(1);
  p2->SetAssetCount(1);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_FALSE(graph.EdgeExists(v1, v2));
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldNotAddEdgeBetweenPluginsWithNoAssetOverlapAndInequalAssetCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetAssetCount(2);
  p2->SetAssetCount(1);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_FALSE(graph.EdgeExists(v1, v2));
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldAddEdgeBetweenPluginsWithAssetOverlapAndInequalAssetCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->AddOverlappingAssets(*p2);
  p1->SetAssetCount(2);
  p2->SetAssetCount(1);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_EQ(EdgeType::assetOverlap, graph.GetEdgeType(v1, v2).value());
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldCheckAssetsIfRecordsOverlapWithEqualOverrideCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->AddOverlappingRecords(*p2);
  p1->AddOverlappingAssets(*p2);
  p1->SetAssetCount(2);
  p2->SetAssetCount(1);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_EQ(EdgeType::assetOverlap, graph.GetEdgeType(v1, v2).value());
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(
    PluginGraphTest,
    addOverlapEdgesShouldCheckAssetsIfRecordsDoNotOverlapWithInequalOverrideCounts) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->AddOverlappingAssets(*p2);
  p1->SetAssetCount(2);
  p2->SetAssetCount(1);
  p1->SetOverrideRecordCount(1);
  p2->SetOverrideRecordCount(2);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_EQ(EdgeType::assetOverlap, graph.GetEdgeType(v1, v2).value());
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(PluginGraphTest,
       addOverlapEdgesShouldChooseRecordOverlapOverAssetOverlap) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->AddOverlappingRecords(*p2);
  p1->SetOverrideRecordCount(2);
  p2->SetOverrideRecordCount(1);
  p1->AddOverlappingAssets(*p2);
  p1->SetAssetCount(1);
  p2->SetAssetCount(2);

  PluginGraph graph;

  const auto v1 = graph.AddVertex(CreatePluginSortingData("1.esp"));
  const auto v2 = graph.AddVertex(CreatePluginSortingData("2.esp"));

  graph.AddOverlapEdges();

  EXPECT_EQ(EdgeType::recordOverlap, graph.GetEdgeType(v1, v2).value());
  EXPECT_FALSE(graph.EdgeExists(v2, v1));
}

TEST_F(PluginGraphTest, addTieBreakEdgesShouldNotErrorOnAGraphWithOneVertex) {
  const auto plugin = CreatePluginSortingData("A.esp");

  PluginGraph graph;
  graph.AddVertex(plugin);

  graph.AddTieBreakEdges();
}

TEST_F(
    PluginGraphTest,
    addTieBreakEdgesShouldResultInASortOrderEqualToVertexCreationOrderIfThereAreNoOtherEdges) {
  PluginGraph graph;

  for (size_t i = 0; i < 10; ++i) {
    const auto plugin = CreatePluginSortingData(std::to_string(i) + ".esp");
    graph.AddVertex(plugin);
  }

  graph.AddTieBreakEdges();
  const auto sorted = graph.TopologicalSort();
  const auto names = graph.ToPluginNames(sorted);

  std::vector<std::string> expected({"0.esp",
                                     "1.esp",
                                     "2.esp",
                                     "3.esp",
                                     "4.esp",
                                     "5.esp",
                                     "6.esp",
                                     "7.esp",
                                     "8.esp",
                                     "9.esp"});

  EXPECT_FALSE(graph.IsHamiltonianPath(sorted).has_value());
  EXPECT_EQ(expected, names);
}

TEST_F(
    PluginGraphTest,
    addTieBreakEdgesShouldPinPathsThatPreventTheVertexCreationOrderBeingUsed) {
  PluginGraph graph;

  for (size_t i = 0; i < 10; ++i) {
    const auto plugin = CreatePluginSortingData(std::to_string(i) + ".esp");
    graph.AddVertex(plugin);
  }

  // Add a path 6 -> 7 -> 8 -> 5.
  vertex_t five = graph.GetVertexByName("5.esp").value();
  vertex_t six = graph.GetVertexByName("6.esp").value();
  vertex_t seven = graph.GetVertexByName("7.esp").value();
  vertex_t eight = graph.GetVertexByName("8.esp").value();

  graph.AddEdge(six, seven, EdgeType::recordOverlap);
  graph.AddEdge(seven, eight, EdgeType::recordOverlap);
  graph.AddEdge(eight, five, EdgeType::recordOverlap);

  // Also add a path going from 6 to 3 and another from 8 to 4.
  vertex_t three = graph.GetVertexByName("3.esp").value();
  vertex_t four = graph.GetVertexByName("4.esp").value();

  graph.AddEdge(six, three, EdgeType::recordOverlap);
  graph.AddEdge(eight, four, EdgeType::recordOverlap);

  graph.AddTieBreakEdges();
  const auto sorted = graph.TopologicalSort();
  const auto names = graph.ToPluginNames(sorted);

  std::vector<std::string> expected({"0.esp",
                                     "1.esp",
                                     "2.esp",
                                     "6.esp",
                                     "3.esp",
                                     "7.esp",
                                     "8.esp",
                                     "4.esp",
                                     "5.esp",
                                     "9.esp"});

  EXPECT_FALSE(graph.IsHamiltonianPath(sorted).has_value());
  EXPECT_EQ(expected, names);
}

TEST_F(
    PluginGraphTest,
    addTieBreakEdgesShouldPrefixPathToNewLoadOrderIfTheFirstPairOfVerticesCannotBeUsedInCreationOrder) {
  PluginGraph graph;

  for (size_t i = 0; i < 10; ++i) {
    const auto plugin = CreatePluginSortingData(std::to_string(i) + ".esp");
    graph.AddVertex(plugin);
  }

  // Add a path 1 -> 2 -> 3 -> 0.
  vertex_t zero = graph.GetVertexByName("0.esp").value();
  vertex_t one = graph.GetVertexByName("1.esp").value();
  vertex_t two = graph.GetVertexByName("2.esp").value();
  vertex_t three = graph.GetVertexByName("3.esp").value();

  graph.AddEdge(one, two, EdgeType::recordOverlap);
  graph.AddEdge(two, three, EdgeType::recordOverlap);
  graph.AddEdge(three, zero, EdgeType::recordOverlap);

  graph.AddTieBreakEdges();
  const auto sorted = graph.TopologicalSort();
  const auto names = graph.ToPluginNames(sorted);

  std::vector<std::string> expected({"1.esp",
                                     "2.esp",
                                     "3.esp",
                                     "0.esp",
                                     "4.esp",
                                     "5.esp",
                                     "6.esp",
                                     "7.esp",
                                     "8.esp",
                                     "9.esp"});

  EXPECT_FALSE(graph.IsHamiltonianPath(sorted).has_value());
  EXPECT_EQ(expected, names);
}
}
}

#endif
