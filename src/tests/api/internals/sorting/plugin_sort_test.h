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
#include "tests/api/internals/sorting/plugin_graph_test.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginSortTest : public CommonGameTestFixture {
protected:
  PluginSortTest() :
      game_(GetParam(), dataPath.parent_path(), localPath),
      blankEslEsp("Blank.esl.esp"),
      cccPath_(dataPath.parent_path() / getCCCFilename()) {}

  void loadInstalledPlugins(Game& game, bool headersOnly) {
    auto plugins = GetInstalledPlugins();

    if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
      plugins.push_back(blankEsl);

      if (std::filesystem::exists(dataPath / blankEslEsp)) {
        plugins.push_back(blankEslEsp);
      }
    }

    game.IdentifyMainMasterFile(masterFile);
    game.LoadCurrentLoadOrderState();
    game.LoadPlugins(plugins, headersOnly);
  }

  std::string getCCCFilename() {
    if (GetParam() == GameType::fo4) {
      return "Fallout4.ccc";
    } else {
      // Not every game has a .ccc file, but Skyrim SE does, so just assume
      // that.
      return "Skyrim.ccc";
    }
  }

  void GenerateCCCFile() {
    using std::endl;

    if (GetParam() == GameType::fo4) {
      std::ofstream ccc(cccPath_);
      ccc << blankDifferentEsm << endl
          << blankDifferentMasterDependentEsm << endl;

      ccc.close();
    }
  }

  PluginSortingData CreatePluginSortingData(
      const std::string& name,
      const std::vector<std::string>& loadOrder = {}) {
    const auto plugin = GetPlugin(name);

    return PluginSortingData(
        plugin, PluginMetadata(), PluginMetadata(), loadOrder);
  }

  plugingraph::TestPlugin* GetPlugin(const std::string& name) {
    auto it = testPlugins_.find(name);

    if (it != testPlugins_.end()) {
      return it->second.get();
    }

    const auto plugin = std::make_shared<plugingraph::TestPlugin>(name);

    return testPlugins_.insert_or_assign(name, plugin).first->second.get();
  }

  Game game_;
  const std::string blankEslEsp;
  const std::filesystem::path cccPath_;

private:
  std::map<std::string, std::shared_ptr<plugingraph::TestPlugin>> testPlugins_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         PluginSortTest,
                         ::testing::Values(GameType::tes3,
                                           GameType::tes4,
                                           GameType::fo4,
                                           GameType::starfield));

TEST_P(PluginSortTest, sortingWithNoLoadedPluginsShouldReturnAnEmptyList) {
  std::vector<std::string> sorted = SortPlugins(game_, game_.GetLoadOrder());

  EXPECT_TRUE(sorted.empty());
}

TEST_P(PluginSortTest,
       sortingShouldNotMakeUnnecessaryChangesToAnExistingLoadOrder) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  std::vector<std::string> expectedSortedOrder = getLoadOrder();

  // Check stability by running the sort 100 times.
  for (int i = 0; i < 100; i++) {
    std::vector<std::string> sorted = SortPlugins(game_, game_.GetLoadOrder());
    ASSERT_EQ(expectedSortedOrder, sorted) << " for sort " << i;
  }
}

TEST_P(PluginSortTest,
       sortingShouldNotChangeTheResultIfGivenItsOwnOutputLoadOrder) {
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
        CreatePluginSortingData(p1->GetName(), loadOrder),
        CreatePluginSortingData(p2->GetName(), loadOrder),
        CreatePluginSortingData(p3->GetName(), loadOrder)};

    auto sorted = SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    ASSERT_EQ(expectedSortedOrder, sorted);

    loadOrder = sorted;
  }

  // Now do it again but supplying the sorted load order as the current load
  // order.
  {
    std::vector<PluginSortingData> pluginsSortingData{
        CreatePluginSortingData(p1->GetName(), loadOrder),
        CreatePluginSortingData(p2->GetName(), loadOrder),
        CreatePluginSortingData(p3->GetName(), loadOrder)};

    auto sorted = SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    ASSERT_EQ(expectedSortedOrder, sorted);
  }
}

TEST_P(PluginSortTest,
       sortingShouldUseGroupMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  game_.GetDatabase().SetUserGroups({Group("A"), Group("B", {"A"})});

  PluginMetadata plugin(blankDifferentEsm);
  plugin.SetGroup("A");
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankEsm);
  plugin.SetGroup("B");
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  std::vector<std::string> expectedSortedOrder;
  if (GetParam() == GameType::starfield) {
    expectedSortedOrder = {
        masterFile,
        blankDifferentEsm,
        blankEsm,
        blankFullEsm,
        blankMasterDependentEsm,
        blankMediumEsm,
        blankEsl,
        blankEsp,
        blankDifferentEsp,
        blankMasterDependentEsp,
    };
  } else {
    expectedSortedOrder = {
        masterFile,
        blankDifferentEsm,
        blankEsm,
        blankMasterDependentEsm,
        blankDifferentMasterDependentEsm,
        blankEsp,
        blankDifferentEsp,
        blankMasterDependentEsp,
        blankDifferentMasterDependentEsp,
        blankPluginDependentEsp,
        blankDifferentPluginDependentEsp,
    };
  }

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  }

  std::vector<std::string> sorted = SortPlugins(game_, game_.GetLoadOrder());
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSortTest,
       sortingShouldAccountForUserGroupMetadataWhenTryingToAvoidCycles) {
  using std::endl;

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto masterlistPath = metadataFilesPath / "masterlist.yaml";
  std::ofstream masterlist(masterlistPath);
  masterlist << "groups:" << endl
             << "  - name: default" << endl
             << "  - name: B" << endl
             << "    after:" << endl
             << "      - default" << endl;
  masterlist.close();

  game_.GetDatabase().LoadLists(masterlistPath);

  game_.GetDatabase().SetUserGroups(
      {Group("A", {"default"}), Group("B", {"A"})});

  PluginMetadata plugin(blankEsm);
  plugin.SetGroup("B");
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankMasterDependentEsm);
  plugin.SetGroup("default");
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankDifferentEsm);
  plugin.SetGroup("A");
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  std::vector<std::string> expectedSortedOrder;
  if (GetParam() == GameType::starfield) {
    expectedSortedOrder = {
        masterFile,
        blankFullEsm,
        blankMasterDependentEsm,
        blankMediumEsm,
        blankEsl,
        blankDifferentEsm,
        blankEsm,
        blankEsp,
        blankDifferentEsp,
        blankMasterDependentEsp,
    };
  } else {
    expectedSortedOrder = {
        masterFile,
        blankDifferentEsm,
        blankDifferentMasterDependentEsm,
        blankEsm,
        blankMasterDependentEsm,
        blankEsp,
        blankDifferentEsp,
        blankMasterDependentEsp,
        blankDifferentMasterDependentEsp,
        blankPluginDependentEsp,
        blankDifferentPluginDependentEsp,
    };
  }

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 1, blankEsl);
  }

  std::vector<std::string> sorted = SortPlugins(game_, game_.GetLoadOrder());
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSortTest, sortingShouldThrowIfAPluginHasAGroupThatDoesNotExist) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankEsm);
  plugin.SetGroup("group1");
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  EXPECT_THROW(SortPlugins(game_, game_.GetLoadOrder()), UndefinedGroupError);
}

TEST_P(PluginSortTest,
       sortingShouldUseLoadAfterMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsp);
  plugin.SetLoadAfterFiles({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  std::vector<std::string> expectedSortedOrder;
  if (GetParam() == GameType::starfield) {
    expectedSortedOrder = {
        masterFile,
        blankEsm,
        blankDifferentEsm,
        blankFullEsm,
        blankMasterDependentEsm,
        blankMediumEsm,
        blankEsl,
        blankDifferentEsp,
        blankEsp,
        blankMasterDependentEsp,
    };
  } else {
    expectedSortedOrder = {
        masterFile,
        blankEsm,
        blankDifferentEsm,
        blankMasterDependentEsm,
        blankDifferentMasterDependentEsm,
        blankDifferentEsp,
        blankDifferentPluginDependentEsp,
        blankEsp,
        blankMasterDependentEsp,
        blankDifferentMasterDependentEsp,
        blankPluginDependentEsp,
    };
  }

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  }

  std::vector<std::string> sorted = SortPlugins(game_, game_.GetLoadOrder());
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSortTest,
       sortingShouldUseRequirementMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsp);
  plugin.SetRequirements({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  std::vector<std::string> expectedSortedOrder;
  if (GetParam() == GameType::starfield) {
    expectedSortedOrder = {
        masterFile,
        blankEsm,
        blankDifferentEsm,
        blankFullEsm,
        blankMasterDependentEsm,
        blankMediumEsm,
        blankEsl,
        blankDifferentEsp,
        blankEsp,
        blankMasterDependentEsp,
    };
  } else {
    expectedSortedOrder = {
        masterFile,
        blankEsm,
        blankDifferentEsm,
        blankMasterDependentEsm,
        blankDifferentMasterDependentEsm,
        blankDifferentEsp,
        blankDifferentPluginDependentEsp,
        blankEsp,
        blankMasterDependentEsp,
        blankDifferentMasterDependentEsp,
        blankPluginDependentEsp,
    };
  }

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  }

  std::vector<std::string> sorted = SortPlugins(game_, game_.GetLoadOrder());
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSortTest,
       sortingShouldUseTheGameCCCFileToEnforceHardcodedLoadOrderPositions) {
  if (GetParam() != GameType::fo4) {
    return;
  }

  // Need to generate CCC file then recreate game object as the file is only
  // read during intialisation.
  GenerateCCCFile();
  Game newGame(GetParam(), dataPath.parent_path(), localPath);
  ASSERT_NO_THROW(loadInstalledPlugins(newGame, false));

  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankDifferentEsm,
      blankDifferentMasterDependentEsm,
      blankEsm,
      blankMasterDependentEsm,
      blankEsl,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
  });

  std::vector<std::string> sorted =
      SortPlugins(newGame, newGame.GetLoadOrder());
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSortTest, sortingShouldPutBlueprintPluginsAfterAllOthers) {
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankEsm);

  Game newGame(GetParam(), dataPath.parent_path(), localPath);
  ASSERT_NO_THROW(loadInstalledPlugins(newGame, false));

  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankDifferentEsm,
      blankFullEsm,
      blankMasterDependentEsm,
      blankMediumEsm,
      blankEsl,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankEsm,
  });

  const auto sorted = SortPlugins(newGame, newGame.GetLoadOrder());
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSortTest, sortingShouldThrowIfACyclicInteractionIsEncountered) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto pluginName =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsm;
  PluginMetadata plugin(pluginName);
  plugin.SetLoadAfterFiles({File(blankMasterDependentEsm)});
  game_.GetDatabase().SetPluginUserMetadata(plugin);

  EXPECT_THROW(SortPlugins(game_, game_.GetLoadOrder()),
               CyclicInteractionError);
}

TEST_P(PluginSortTest,
       sortingShouldThrowIfMasterEdgeWouldContradictMasterFlags) {
  // Can't test with the test plugin files, so use the other SortPlugins()
  // overload to provide stubs.
  const auto esm = GetPlugin(blankEsm);
  const auto esp = GetPlugin(blankEsp);

  esm->SetIsMaster(true);
  esm->AddMaster(esp->GetName());

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(esm->GetName()),
      CreatePluginSortingData(esp->GetName())};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(esp->GetName(), e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(esm->GetName(), e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfMasterlistRequirementEdgeWouldContradictMasterFlags) {
  using std::endl;

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto masterlistPath = metadataFilesPath / "masterlist.yaml";
  std::ofstream masterlist(masterlistPath);
  masterlist << "plugins:" << endl
             << "  - name: " << blankEsm << endl
             << "    req:" << endl
             << "      - " << blankEsp << endl;
  masterlist.close();

  game_.GetDatabase().LoadLists(masterlistPath);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankEsp, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(PluginSortTest,
       sortingShouldThrowIfUserRequirementEdgeWouldContradictMasterFlags) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankEsm);
  plugin.SetRequirements({File(blankEsp)});

  game_.GetDatabase().SetPluginUserMetadata(plugin);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankEsp, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(PluginSortTest,
       sortingShouldThrowIfMasterlistLoadAfterEdgeWouldContradictMasterFlags) {
  using std::endl;

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto masterlistPath = metadataFilesPath / "masterlist.yaml";
  std::ofstream masterlist(masterlistPath);
  masterlist << "plugins:" << endl
             << "  - name: " << blankEsm << endl
             << "    after:" << endl
             << "      - " << blankEsp << endl;
  masterlist.close();

  game_.GetDatabase().LoadLists(masterlistPath);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankEsp, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(PluginSortTest,
       sortingShouldThrowIfUserLoadAfterEdgeWouldContradictMasterFlags) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankEsm);
  plugin.SetLoadAfterFiles({File(blankEsp)});

  game_.GetDatabase().SetPluginUserMetadata(plugin);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankEsp, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(PluginSortTest,
       sortingShouldThrowIfHardcodedEdgeWouldContradictMasterFlags) {
  // Can't test with the test plugin files, so use the other SortPlugins()
  // overload to provide stubs.
  const auto esm = GetPlugin(blankEsm);
  const auto esp = GetPlugin(blankEsp);

  esm->SetIsMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(esm->GetName()),
      CreatePluginSortingData(esp->GetName())};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {esp->GetName()});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankEsp, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::hardcoded, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterFlag,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(PluginSortTest,
       sortingShouldThrowIfAMasterEdgeWouldPutABlueprintMasterBeforeAMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankFullEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankFullEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankMasterDependentEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(PluginSortTest,
       sortingShouldThrowIfAMasterEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }

  // Can't test with the test plugin files, so use the other SortPlugins()
  // overload to provide stubs.
  const auto esp = GetPlugin(blankMasterDependentEsp);
  const auto blueprint = GetPlugin(blankFullEsm);

  esp->AddMaster(blankFullEsm);
  blueprint->SetIsMaster(true);
  blueprint->SetIsBlueprintMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(esp->GetName()),
      CreatePluginSortingData(blueprint->GetName())};

  try {
    SortPlugins(std::move(pluginsSortingData), {Group()}, {}, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankFullEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::master, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankMasterDependentEsp, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAMasterlistRequirementEdgeWouldPutABlueprintMasterBeforeAMaster) {
  using std::endl;
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto masterlistPath = metadataFilesPath / "masterlist.yaml";
  std::ofstream masterlist(masterlistPath);
  masterlist << "plugins:" << endl
             << "  - name: " << blankEsm << endl
             << "    req:" << endl
             << "      - " << blankDifferentEsm << endl;
  masterlist.close();

  game_.GetDatabase().LoadLists(masterlistPath);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAMasterlistRequirementEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  using std::endl;
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto masterlistPath = metadataFilesPath / "masterlist.yaml";
  std::ofstream masterlist(masterlistPath);
  masterlist << "plugins:" << endl
             << "  - name: " << blankEsp << endl
             << "    req:" << endl
             << "      - " << blankDifferentEsm << endl;
  masterlist.close();

  game_.GetDatabase().LoadLists(masterlistPath);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsp, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAUserRequirementEdgeWouldPutABlueprintMasterBeforeAMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankEsm);
  plugin.SetRequirements({File(blankDifferentEsm)});

  game_.GetDatabase().SetPluginUserMetadata(plugin);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAUserRequirementEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankEsp);
  plugin.SetRequirements({File(blankDifferentEsm)});

  game_.GetDatabase().SetPluginUserMetadata(plugin);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userRequirement,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsp, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAMasterlistLoadAfterEdgeWouldPutABlueprintMasterBeforeAMaster) {
  using std::endl;
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto masterlistPath = metadataFilesPath / "masterlist.yaml";
  std::ofstream masterlist(masterlistPath);
  masterlist << "plugins:" << endl
             << "  - name: " << blankEsm << endl
             << "    after:" << endl
             << "      - " << blankDifferentEsm << endl;
  masterlist.close();

  game_.GetDatabase().LoadLists(masterlistPath);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAMasterlistLoadAfterEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  using std::endl;
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  const auto masterlistPath = metadataFilesPath / "masterlist.yaml";
  std::ofstream masterlist(masterlistPath);
  masterlist << "plugins:" << endl
             << "  - name: " << blankEsp << endl
             << "    after:" << endl
             << "      - " << blankDifferentEsm << endl;
  masterlist.close();

  game_.GetDatabase().LoadLists(masterlistPath);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsp, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAUserLoadAfterEdgeWouldPutABlueprintMasterBeforeAMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankEsm);
  plugin.SetLoadAfterFiles({File(blankDifferentEsm)});

  game_.GetDatabase().SetPluginUserMetadata(plugin);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsm, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldThrowIfAUserLoadAfterEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }

  SetBlueprintFlag(dataPath / blankDifferentEsm);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankEsp);
  plugin.SetLoadAfterFiles({File(blankDifferentEsm)});

  game_.GetDatabase().SetPluginUserMetadata(plugin);

  try {
    SortPlugins(game_, game_.GetLoadOrder());
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ(blankDifferentEsm, e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(blankEsp, e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::blueprintMaster,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSortTest,
    sortingShouldNotThrowIfAHardcodedEdgeWouldPutABlueprintMasterBeforeAMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }
  // Can't test with the test plugin files, so use the other SortPlugins()
  // overload to provide stubs.
  const auto esm = GetPlugin(blankEsm);
  const auto blueprint = GetPlugin(blankDifferentEsm);

  esm->SetIsMaster(true);
  blueprint->SetIsMaster(true);
  blueprint->SetIsBlueprintMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(esm->GetName()),
      CreatePluginSortingData(blueprint->GetName())};

  const auto sorted = SortPlugins(
      std::move(pluginsSortingData), {Group()}, {}, {blueprint->GetName()});

  EXPECT_EQ(std::vector<std::string>({
                blankEsm, blankDifferentEsm,
            }),
            sorted);
}

TEST_P(
    PluginSortTest,
    sortingShouldNotThrowIfAHardcodedEdgeWouldPutABlueprintMasterBeforeANonMaster) {
  if (GetParam() != GameType::starfield) {
    return;
  }
  // Can't test with the test plugin files, so use the other SortPlugins()
  // overload to provide stubs.
  const auto esm = GetPlugin(blankEsp);
  const auto blueprint = GetPlugin(blankDifferentEsm);

  esm->SetIsMaster(true);
  blueprint->SetIsMaster(true);
  blueprint->SetIsBlueprintMaster(true);

  std::vector<PluginSortingData> pluginsSortingData{
      CreatePluginSortingData(esm->GetName()),
      CreatePluginSortingData(blueprint->GetName())};

  const auto sorted = SortPlugins(
      std::move(pluginsSortingData), {Group()}, {}, {blueprint->GetName()});

  EXPECT_EQ(std::vector<std::string>({
                blankEsp,
                blankDifferentEsm,
            }),
            sorted);
}
}
}

#endif
