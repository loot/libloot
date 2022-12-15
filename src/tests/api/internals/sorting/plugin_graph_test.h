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

#include "api/sorting/plugin_graph.h"

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

  std::vector<std::string> GetMasters() const override {
    return std::vector<std::string>();
  }

  std::vector<Tag> GetBashTags() const override { return std::vector<Tag>(); }

  std::optional<uint32_t> GetCRC() const override {
    return std::optional<uint32_t>();
  }

  bool IsMaster() const override { return false; }

  bool IsLightPlugin() const override { return false; }

  bool IsValidAsLightPlugin() const override { return false; }

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

  size_t GetOverlapSize(
      const std::vector<const PluginInterface*>&) const override {
    return size_t();
  }

  size_t GetAssetCount() const override { return assetCount_; };

  bool DoAssetsOverlap(const PluginSortingInterface& plugin) const override {
    const auto otherPlugin = dynamic_cast<const TestPlugin*>(&plugin);
    return assetsOverlapWith.count(&plugin) != 0 ||
           otherPlugin->assetsOverlapWith.count(this) != 0;
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
  std::set<const PluginInterface*> recordsOverlapWith;
  std::set<const PluginSortingInterface*> assetsOverlapWith;
  size_t overrideRecordCount_{0};
  size_t assetCount_{0};
};
}

class PluginGraphTest : public ::testing::Test {
public:
  PluginSortingData CreatePluginSortingData(const std::string& name) {
    const auto plugin = GetPlugin(name);

    return PluginSortingData(
        plugin, PluginMetadata(), PluginMetadata(), {}, GameType::tes4, {});
  }

  plugingraph::TestPlugin* GetPlugin(const std::string& name) {
    auto it = plugins.find(name);

    if (it != plugins.end()) {
      return it->second.get();
    }

    const auto plugin = std::make_shared<plugingraph::TestPlugin>(name);

    return plugins.insert_or_assign(name, plugin).first->second.get();
  }

private:
  std::map<std::string, std::shared_ptr<plugingraph::TestPlugin>> plugins;
};

TEST_F(PluginGraphTest,
       topologicalSortWithNoLoadedPluginsShouldReturnAnEmptyList) {
  PluginGraph graph;
  std::vector<vertex_t> sorted = graph.TopologicalSort();

  EXPECT_TRUE(sorted.empty());
}

TEST_F(PluginGraphTest,
       addOverlapEdgesShouldNotAddEdgesBetweenNonOverlappingPlugins) {
  PluginGraph graph;

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

  graph.AddOverlapEdges();

  EXPECT_TRUE(graph.EdgeExists(v1, v2));
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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

  graph.AddOverlapEdges();

  EXPECT_TRUE(graph.EdgeExists(v1, v2));
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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

  graph.AddOverlapEdges();

  EXPECT_TRUE(graph.EdgeExists(v1, v2));
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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

  graph.AddOverlapEdges();

  EXPECT_TRUE(graph.EdgeExists(v1, v2));
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

  graph.AddVertex(CreatePluginSortingData("1.esp"));
  graph.AddVertex(CreatePluginSortingData("2.esp"));

  const auto v1 = graph.GetVertexByName("1.esp").value();
  const auto v2 = graph.GetVertexByName("2.esp").value();

  graph.AddOverlapEdges();

  EXPECT_TRUE(graph.EdgeExists(v1, v2));
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

  graph.AddEdge(six, seven, EdgeType::overlap);
  graph.AddEdge(seven, eight, EdgeType::overlap);
  graph.AddEdge(eight, five, EdgeType::overlap);

  // Also add a path going from 6 to 3 and another from 8 to 4.
  vertex_t three = graph.GetVertexByName("3.esp").value();
  vertex_t four = graph.GetVertexByName("4.esp").value();

  graph.AddEdge(six, three, EdgeType::overlap);
  graph.AddEdge(eight, four, EdgeType::overlap);

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

  graph.AddEdge(one, two, EdgeType::overlap);
  graph.AddEdge(two, three, EdgeType::overlap);
  graph.AddEdge(three, zero, EdgeType::overlap);

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
