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

#ifndef LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORTER_TEST
#define LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORTER_TEST

#include "api/sorting/plugin_sorter.h"

#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginSorterTest : public CommonGameTestFixture {
protected:
  PluginSorterTest() :
      game_(GetParam(), dataPath.parent_path(), localPath),
      masterlistPath_(metadataFilesPath / "userlist.yaml"),
      cccPath_(dataPath.parent_path() / getCCCFilename()),
      blankEslEsp("Blank.esl.esp") {}

  void loadInstalledPlugins(Game &game_, bool headersOnly) {
    std::vector<std::string> plugins({
        masterFile,
        blankEsm,
        blankDifferentEsm,
        blankMasterDependentEsm,
        blankDifferentMasterDependentEsm,
        blankEsp,
        blankDifferentEsp,
        blankMasterDependentEsp,
        blankDifferentMasterDependentEsp,
        blankPluginDependentEsp,
        blankDifferentPluginDependentEsp,
    });

    if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
      plugins.push_back(blankEsl);

      if (std::filesystem::exists(dataPath / blankEslEsp)) {
        plugins.push_back(blankEslEsp);
      }
    }

    game_.IdentifyMainMasterFile(masterFile);
    game_.LoadCurrentLoadOrderState();
    game_.LoadPlugins(plugins, headersOnly);
  }

  void GenerateMasterlist() {
    using std::endl;

    std::ofstream masterlist(masterlistPath_);
    masterlist << "groups:" << endl
               << "  - name: earliest" << endl
               << "  - name: earlier" << endl
               << "    after:" << endl
               << "      - earliest" << endl
               << "  - name: default" << endl
               << "    after:" << endl
               << "      - earlier" << endl
               << "  - name: group1" << endl
               << "  - name: group2" << endl
               << "    after:" << endl
               << "      - group1" << endl
               << "  - name: group3" << endl
               << "    after:" << endl
               << "      - group2" << endl
               << "  - name: group4" << endl
               << "    after:" << endl
               << "      - default" << endl;

    masterlist.close();
  }

  std::string getCCCFilename() {
    if (GetParam() == GameType::fo4) {
      return "Fallout4.ccc";
    }
    else {
      // Not every game has a .ccc file, but Skyrim SE does, so just assume that.
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

  Game game_;
  const std::string blankEslEsp;
  const std::filesystem::path masterlistPath_;
  const std::filesystem::path cccPath_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        PluginSorterTest,
                        ::testing::Values(GameType::tes4, GameType::fo4));

TEST_P(PluginSorterTest, sortingWithNoLoadedPluginsShouldReturnAnEmptyList) {
  PluginSorter sorter;
  std::vector<std::string> sorted = sorter.Sort(game_);

  EXPECT_TRUE(sorted.empty());
}

TEST_P(PluginSorterTest,
       lightMasterFlaggedEspFilesShouldNotBeTreatedAsMasters) {
  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    ASSERT_NO_THROW(
        std::filesystem::copy(dataPath / blankEsl, dataPath / blankEslEsp));
  }

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  auto esp = PluginSortingData(
      *dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsp).value().get()),
      PluginMetadata());
  EXPECT_FALSE(esp.IsMaster());

  auto master = PluginSortingData(
      *dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsm).value().get()),
      PluginMetadata());
  EXPECT_TRUE(master.IsMaster());

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    auto lightMaster = PluginSortingData(
        *dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsl).value().get()),
        PluginMetadata());
    EXPECT_TRUE(lightMaster.IsMaster());

    auto lightMasterEsp = PluginSortingData(*dynamic_cast<const Plugin *>(
                              game_.GetPlugin(blankEslEsp).value().get()),
        PluginMetadata());
    EXPECT_FALSE(lightMasterEsp.IsMaster());
  }
}

TEST_P(PluginSorterTest,
       sortingShouldNotMakeUnnecessaryChangesToAnExistingLoadOrder) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder = getLoadOrder();

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_TRUE(
      std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));

  // Check stability.
  sorted = ps.Sort(game_);
  EXPECT_TRUE(
      std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));
}

TEST_P(PluginSorterTest, sortingShouldResolveGroupsAsTransitiveLoadAfterSets) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  GenerateMasterlist();
  game_.GetDatabase()->LoadLists(masterlistPath_);

  PluginMetadata plugin(blankDifferentEsm);
  plugin.SetGroup("group1");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankEsm);
  plugin.SetGroup("group3");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
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
  });

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  }

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest, sortingShouldThrowIfAPluginHasAGroupThatDoesNotExist) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginMetadata plugin(blankDifferentEsm);
  plugin.SetGroup("group1");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  EXPECT_THROW(ps.Sort(game_), UndefinedGroupError);
}

TEST_P(PluginSorterTest,
       sortingShouldIgnoreAGroupEdgeIfItWouldCauseACycleInIsolation) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  GenerateMasterlist();
  game_.GetDatabase()->LoadLists(masterlistPath_);

  PluginMetadata plugin(blankEsm);
  plugin.SetGroup("group4");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
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
  });

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 3, blankEsl);
  }

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(
    PluginSorterTest,
    sortingShouldIgnoreGroupEdgesInvolvedInABackCycleOfAGroupEdgeFromADefaultGroupPlugin) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  GenerateMasterlist();
  game_.GetDatabase()->LoadLists(masterlistPath_);

  PluginMetadata plugin(blankEsp);

  plugin = PluginMetadata(blankDifferentMasterDependentEsp);
  plugin.SetLoadAfterFiles(std::set<File>({File(blankMasterDependentEsp)}));
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankDifferentEsp);
  plugin.SetGroup("group1");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankMasterDependentEsp);
  plugin.SetGroup("group2");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
  });

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  }

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(
    PluginSorterTest,
    sortingShouldIgnoreGroupEdgesInvolvedInABackCycleOfAGroupEdgeToADefaultGroupPlugin) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  GenerateMasterlist();
  game_.GetDatabase()->LoadLists(masterlistPath_);

  PluginMetadata plugin(blankMasterDependentEsm);
  plugin.SetGroup("earliest");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankDifferentEsm);
  plugin.SetGroup("earlier");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
      blankEsm,
      blankMasterDependentEsm,
      blankDifferentEsm,
      blankDifferentMasterDependentEsm,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
  });

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin(), masterFile);
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  } else {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 3, masterFile);
  }

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(
    PluginSorterTest,
    sortingShouldThrowForAGroupEdgeThatCausesAMultiGroupCycleBetweenTwoNonDefaultGroups) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  GenerateMasterlist();
  game_.GetDatabase()->LoadLists(masterlistPath_);

  PluginMetadata plugin(blankMasterDependentEsm);
  plugin.SetGroup("earliest");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankDifferentEsm);
  plugin.SetGroup("earlier");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankEsm);
  plugin.SetGroup("group4");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  try {
    PluginSorter ps;
    ps.Sort(game_);
    FAIL();
  } catch (CyclicInteractionError &e) {
    ASSERT_EQ(3, e.GetCycle().size());
    EXPECT_EQ("Blank - Different Master Dependent.esm", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::Group, e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("Blank.esm", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::Master, e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("Blank - Master Dependent.esm", e.GetCycle()[2].GetName());
    EXPECT_EQ(EdgeType::Group, e.GetCycle()[2].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(
    PluginSorterTest,
    sortingShouldNotIgnorePluginsInTheSameGroupAsTheTargetPluginOfAGroupEdgeThatCausesACycleInIsolation) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  GenerateMasterlist();
  game_.GetDatabase()->LoadLists(masterlistPath_);

  PluginMetadata plugin(blankEsm);
  plugin.SetGroup("group4");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  plugin = PluginMetadata(blankDifferentMasterDependentEsm);
  plugin.SetGroup("group4");
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
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
  });

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 2, blankEsl);
  }

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest,
       sortingShouldUseLoadAfterMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsp);
  plugin.SetLoadAfterFiles({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankDifferentPluginDependentEsp,
      blankEsp,
      blankPluginDependentEsp,
  });

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  }

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest,
       sortingShouldUseRequirementMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsp);
  plugin.SetRequirements({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankDifferentPluginDependentEsp,
      blankEsp,
      blankPluginDependentEsp,
  });

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedSortedOrder.insert(expectedSortedOrder.begin() + 5, blankEsl);
  }

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest,
       sortingShouldUseTheGameCCCFileToEnforceHardcodedLoadOrderPositions) {
  if (GetParam() != GameType::fo4) {
    return;
  }

  // Need to generate CCC file then recreate game object as the file is only
  // read during intialisation.
  GenerateCCCFile();
  Game newGame(GetParam(), dataPath.parent_path(), localPath);
  ASSERT_NO_THROW(loadInstalledPlugins(newGame, false));

  PluginSorter ps;
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

  std::vector<std::string> sorted = ps.Sort(newGame);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest, sortingShouldThrowIfACyclicInteractionIsEncountered) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsm);
  plugin.SetLoadAfterFiles({File(blankMasterDependentEsm)});
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  EXPECT_THROW(ps.Sort(game_), CyclicInteractionError);
}
}
}

#endif
