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

#ifndef LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORT_TEST
#define LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORT_TEST

#include "api/sorting/plugin_sort.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"
#include "tests/api/internals/plugin_test.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class GetPluginsSortingDataTest : public CommonGameTestFixture {
protected:
  GetPluginsSortingDataTest() : CommonGameTestFixture(GameType::tes4) {}
};

TEST_F(GetPluginsSortingDataTest, shouldFilterOutFilesWithFalseConstraints) {
  Game game(GameType::tes4, gamePath, localPath);

  game.LoadPlugins({blankEsp}, true);

  const auto plugin = game.GetPlugin(blankEsp);

  const auto trueConstraint = "file(\"Blank.esm\")";
  const auto falseConstraint = "file(\"missing.esp\")";

  std::filesystem::path masterlistPath = localPath / "masterlist.yaml";
  std::ofstream out(masterlistPath);
  out << "{plugins: [{name: Blank.esp, after: [{name: A.esp, constraint: '"
      << trueConstraint << "'}, {name: B.esp, constraint: '" << falseConstraint
      << "'}], req: [{name: C.esp, constraint: '" << trueConstraint
      << "'}, {name: D.esp, constraint: '" << falseConstraint << "'}]}]}";
  out.close();

  game.GetDatabase().LoadMasterlist(masterlistPath);

  PluginMetadata userMetadata(blankEsp);
  userMetadata.SetLoadAfterFiles(
      {File(blankEsm, "", "", {}, trueConstraint),
       File(blankDifferentEsm, "", "", {}, falseConstraint)});
  userMetadata.SetRequirements(
      {File(blankDifferentEsp, "", "", {}, trueConstraint),
       File(blankMasterDependentEsm, "", "", {}, falseConstraint)});

  game.GetDatabase().SetPluginUserMetadata(userMetadata);

  const auto pluginsSortingData = GetPluginsSortingData(
      game.GetDatabase(), {reinterpret_cast<const Plugin*>(plugin.get())});

  ASSERT_EQ(1, pluginsSortingData.size());
  EXPECT_EQ(std::vector<File>{File("A.esp", "", "", {}, trueConstraint)},
            pluginsSortingData[0].GetMasterlistLoadAfterFiles());
  EXPECT_EQ(std::vector<File>{File("C.esp", "", "", {}, trueConstraint)},
            pluginsSortingData[0].GetMasterlistRequirements());
  EXPECT_EQ(std::vector<File>{userMetadata.GetLoadAfterFiles()[0]},
            pluginsSortingData[0].GetUserLoadAfterFiles());
  EXPECT_EQ(std::vector<File>{userMetadata.GetRequirements()[0]},
            pluginsSortingData[0].GetUserRequirements());
}

class SortPluginsTest : public ::testing::Test {
protected:
  PluginSortingData CreatePluginSortingData(const std::string& name,
                                            const size_t loadOrderIndex) {
    const auto plugin = GetPlugin(name);

    return PluginSortingData(
        plugin, PluginMetadata(), PluginMetadata(), loadOrderIndex);
  }

  TestPlugin* GetPlugin(const std::string& name) {
    auto it = testPlugins_.find(name);

    if (it != testPlugins_.end()) {
      return it->second.get();
    }

    const auto plugin = std::make_shared<TestPlugin>(name);

    return testPlugins_.insert_or_assign(name, plugin).first->second.get();
  }

private:
  std::map<std::string, std::shared_ptr<TestPlugin>> testPlugins_;
};

TEST_F(SortPluginsTest, shouldNotChangeTheResultIfGivenItsOwnOutputLoadOrder) {
  // Can't test with the test plugin files, so use the other SortPlugins()
  // overload to provide stubs.
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");
  const auto p3 = GetPlugin("3.esp");

  p1->AddMaster(p3->GetName());

  p1->AddOverlappingRecords(*p2);
  p1->AddOverlappingRecords(*p3);
  p2->AddOverlappingRecords(*p3);
  p1->SetOverrideRecordCount(3);
  p2->SetOverrideRecordCount(2);
  p3->SetOverrideRecordCount(1);

  // Define the initial load order.
  std::vector<std::string> loadOrder{
      p1->GetName(), p2->GetName(), p3->GetName()};

  const std::vector<std::string> expectedSortedOrder{
      p3->GetName(), p1->GetName(), p2->GetName()};

  // Now sort the plugins.
  {
    std::vector<PluginSortingData> pluginsSortingData{
        CreatePluginSortingData(p1->GetName(), 0),
        CreatePluginSortingData(p2->GetName(), 1),
        CreatePluginSortingData(p3->GetName(), 2)};

    auto sorted = SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    ASSERT_EQ(expectedSortedOrder, sorted);

    loadOrder = sorted;
  }

  // Now do it again but supplying the sorted load order as the current load
  // order.
  {
    std::vector<PluginSortingData> pluginsSortingData{
        CreatePluginSortingData(p1->GetName(), 1),
        CreatePluginSortingData(p2->GetName(), 2),
        CreatePluginSortingData(p3->GetName(), 0)};

    auto sorted = SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    ASSERT_EQ(expectedSortedOrder, sorted);
  }
}

TEST_F(SortPluginsTest,
       shouldUseGroupMetadataWhenDecidingRelativePluginPositions) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetGroup("B");
  auto p2Metadata = PluginMetadata();
  p2Metadata.SetGroup("A");

  const auto p1Data = PluginSortingData(p1, p1Metadata, PluginMetadata(), 0);
  const auto p2Data = PluginSortingData(p2, p2Metadata, PluginMetadata(), 1);

  const auto sorted = SortPlugins(
      {p1Data, p2Data}, {Group(), Group("A"), Group("B", {"A"})}, {}, {});

  const auto expected = std::vector<std::string>{p2->GetName(), p1->GetName()};
  EXPECT_EQ(expected, sorted);
}

TEST_F(SortPluginsTest,
       shouldAccountForUserGroupMetadataWhenTryingToAvoidCycles) {
  const std::vector<Group> masterlistGroups{Group(), Group("B", {"default"})};
  const std::vector<Group> userGroups{Group("A", {"default"}),
                                      Group("B", {"A"})};

  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");
  const auto p3 = GetPlugin("3.esp");

  p2->AddMaster(p1->GetName());

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetGroup("B");
  auto p2Metadata = PluginMetadata();
  p2Metadata.SetGroup("default");
  auto p3Metadata = PluginMetadata();
  p3Metadata.SetGroup("A");

  const auto p1Data = PluginSortingData(p1, p1Metadata, PluginMetadata(), 0);
  const auto p2Data = PluginSortingData(p2, p2Metadata, PluginMetadata(), 1);
  const auto p3Data = PluginSortingData(p3, p3Metadata, PluginMetadata(), 2);

  const auto sorted =
      SortPlugins({p1Data, p2Data, p3Data}, masterlistGroups, userGroups, {});

  const std::vector<std::string> expected{
      p3->GetName(), p1->GetName(), p2->GetName()};

  EXPECT_EQ(expected, sorted);
}

TEST_F(SortPluginsTest, shouldThrowIfAPluginHasAGroupThatDoesNotExist) {
  const auto p1 = GetPlugin("1.esp");

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetGroup("A");

  const auto p1Data = PluginSortingData(p1, p1Metadata, PluginMetadata(), 0);

  EXPECT_THROW(SortPlugins({p1Data}, {Group()}, {}, {}), UndefinedGroupError);
}

TEST_F(SortPluginsTest,
       shouldUseLoadAfterMetadataWhenDecidingRelativePluginPositions) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2Data = CreatePluginSortingData("2.esp", 1);

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetLoadAfterFiles({File(p2Data.GetName())});

  const auto p1Data = PluginSortingData(p1, p1Metadata, PluginMetadata(), 0);

  const auto sorted = SortPlugins({p1Data, p2Data}, {Group()}, {}, {});

  const auto expected =
      std::vector<std::string>{p2Data.GetName(), p1->GetName()};
  EXPECT_EQ(expected, sorted);
}

TEST_F(SortPluginsTest,
       shouldUseRequirementMetadataWhenDecidingRelativePluginPositions) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2Data = CreatePluginSortingData("2.esp", 1);

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetRequirements({File(p2Data.GetName())});

  const auto p1Data = PluginSortingData(p1, p1Metadata, PluginMetadata(), 0);

  const auto sorted = SortPlugins({p1Data, p2Data}, {Group()}, {}, {});

  const auto expected =
      std::vector<std::string>{p2Data.GetName(), p1->GetName()};
  EXPECT_EQ(expected, sorted);
}

TEST_F(SortPluginsTest,
       shouldUseTheGameCCCFileToEnforceHardcodedLoadOrderPositions) {
  const auto p1Data = CreatePluginSortingData("1.esp", 0);
  const auto p2Data = CreatePluginSortingData("2.esp", 1);

  const auto sorted =
      SortPlugins({p1Data, p2Data}, {Group()}, {}, {p2Data.GetName()});

  const auto expected =
      std::vector<std::string>{p2Data.GetName(), p1Data.GetName()};
  EXPECT_EQ(expected, sorted);
}

TEST_F(SortPluginsTest, shouldThrowIfACyclicInteractionIsEncountered) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");
  p1->AddMaster(p2->GetName());
  p2->AddMaster(p1->GetName());

  const auto p1Data = CreatePluginSortingData(p1->GetName(), 0);
  const auto p2Data = CreatePluginSortingData(p2->GetName(), 1);

  EXPECT_THROW(SortPlugins({p1Data, p2Data}, {Group()}, {}, {}),
               CyclicInteractionError);
}

TEST_F(SortPluginsTest, shouldThrowIfMasterEdgeWouldContradictMasterFlags) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsMaster(true);
  p1->AddMaster(p2->GetName());

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldThrowIfMasterlistRequirementEdgeWouldContradictMasterFlags) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsMaster(true);

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetRequirements({File(p2->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      PluginSortingData(p1, p1Metadata, PluginMetadata(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldThrowIfUserRequirementEdgeWouldContradictMasterFlags) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsMaster(true);

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetRequirements({File(p2->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      PluginSortingData(p1, PluginMetadata(), p1Metadata, 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldThrowIfMasterlistLoadAfterEdgeWouldContradictMasterFlags) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsMaster(true);

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetLoadAfterFiles({File(p2->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      PluginSortingData(p1, p1Metadata, PluginMetadata(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldThrowIfUserLoadAfterEdgeWouldContradictMasterFlags) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsMaster(true);

  auto p1Metadata = PluginMetadata();
  p1Metadata.SetLoadAfterFiles({File(p2->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      PluginSortingData(p1, PluginMetadata(), p1Metadata, 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest, shouldThrowIfHardcodedEdgeWouldContradictMasterFlags) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {p2->GetName()});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::hardcoded, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldNotThrowIfAMasterEdgeWouldPutABlueprintMasterBeforeAMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);
  p2->AddMaster(p1->GetName());
  p2->SetIsMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  const auto sorted =
      SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});

  const auto expected = std::vector<std::string>{p2->GetName(), p1->GetName()};
  EXPECT_EQ(expected, sorted);
}

TEST_F(SortPluginsTest,
       shouldNotThrowIfAMasterEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);
  p2->AddMaster(p1->GetName());

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  const auto sorted =
      SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});

  const auto expected = std::vector<std::string>{p2->GetName(), p1->GetName()};
  EXPECT_EQ(expected, sorted);
}

TEST_F(
    SortPluginsTest,
    shouldThrowIfAMasterlistRequirementEdgeWouldPutABlueprintMasterBeforeAMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);
  p2->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetRequirements({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, p2Metadata, PluginMetadata(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    SortPluginsTest,
    shouldThrowIfAMasterlistRequirementEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetRequirements({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, p2Metadata, PluginMetadata(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldThrowIfAUserRequirementEdgeWouldPutABlueprintMasterBeforeAMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);
  p2->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetRequirements({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, PluginMetadata(), p2Metadata, 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    SortPluginsTest,
    shouldThrowIfAUserRequirementEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetRequirements({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, PluginMetadata(), p2Metadata, 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    SortPluginsTest,
    shouldThrowIfAMasterlistLoadAfterEdgeWouldPutABlueprintMasterBeforeAMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);
  p2->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetLoadAfterFiles({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, p2Metadata, PluginMetadata(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    SortPluginsTest,
    shouldThrowIfAMasterlistLoadAfterEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetLoadAfterFiles({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, p2Metadata, PluginMetadata(), 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldThrowIfAUserLoadAfterEdgeWouldPutABlueprintMasterBeforeAMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);
  p2->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetLoadAfterFiles({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, PluginMetadata(), p2Metadata, 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(
    SortPluginsTest,
    shouldThrowIfAUserLoadAfterEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);

  auto p2Metadata = PluginMetadata();
  p2Metadata.SetLoadAfterFiles({File(p1->GetName())});

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      PluginSortingData(p2, PluginMetadata(), p2Metadata, 1)};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(p1->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(p2->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_F(SortPluginsTest,
       shouldNotThrowIfAHardcodedEdgeWouldPutABlueprintMasterBeforeAMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);
  p2->SetIsMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  const auto sorted = SortPlugins(
      std::move(pluginsSortingData), {Group()}, {}, {p1->GetName()});

  EXPECT_EQ(std::vector<std::string>({
                p2->GetName(),
                p1->GetName(),
            }),
            sorted);
}

TEST_F(SortPluginsTest,
       shouldNotThrowIfAHardcodedEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  const auto p1 = GetPlugin("1.esp");
  const auto p2 = GetPlugin("2.esp");

  p1->SetIsBlueprintPlugin(true);
  p1->SetIsMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(p1->GetName(), 0),
      CreatePluginSortingData(p2->GetName(), 1)};

  const auto sorted = SortPlugins(
      std::move(pluginsSortingData), {Group()}, {}, {p1->GetName()});

  EXPECT_EQ(std::vector<std::string>({
                p2->GetName(),
                p1->GetName(),
            }),
            sorted);
}
}
}

#endif
