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

#ifndef LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORTING_DATA_TEST
#define LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORTING_DATA_TEST

#include "api/sorting/plugin_sorting_data.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginSortingDataTest : public CommonGameTestFixture {
protected:
  PluginSortingDataTest() :
      game_(GetParam(), dataPath.parent_path(), localPath),
      blankEslEsp("Blank.esl.esp") {}

  void loadInstalledPlugins(Game &game, bool headersOnly) {
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

    game.IdentifyMainMasterFile(masterFile);
    game.LoadCurrentLoadOrderState();
    game.LoadPlugins(plugins, headersOnly);
  }

  Game game_;
  const std::string blankEslEsp;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         PluginSortingDataTest,
                         ::testing::Values(GameType::tes3,
                                           GameType::tes4,
                                           GameType::fo4));

TEST_P(PluginSortingDataTest, lightFlaggedEspFilesShouldNotBeTreatedAsMasters) {
  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    ASSERT_NO_THROW(
        std::filesystem::copy(dataPath / blankEsl, dataPath / blankEslEsp));
  }

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  auto esp = PluginSortingData(
      *dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsp).get()),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder(),
      game_.Type(),
      game_.GetCache()->GetPlugins());
  EXPECT_FALSE(esp.IsMaster());

  auto master = PluginSortingData(
      *dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsm).get()),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder(),
      game_.Type(),
      game_.GetCache()->GetPlugins());
  EXPECT_TRUE(master.IsMaster());

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    auto lightMaster = PluginSortingData(
        *dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsl).get()),
        PluginMetadata(),
        PluginMetadata(),
        getLoadOrder(),
        game_.Type(),
        game_.GetCache()->GetPlugins());
    EXPECT_TRUE(lightMaster.IsMaster());

    auto lightPlugin = PluginSortingData(
        *dynamic_cast<const Plugin *>(game_.GetPlugin(blankEslEsp).get()),
        PluginMetadata(),
        PluginMetadata(),
        getLoadOrder(),
        game_.Type(),
        game_.GetCache()->GetPlugins());
    EXPECT_FALSE(lightPlugin.IsMaster());
  }
}

TEST_P(PluginSortingDataTest,
       numOverrideFormIdsShouldEqualSizeOfOverlapWithThePluginsMasters) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  auto plugin =
      PluginSortingData(*dynamic_cast<const Plugin *>(
                            game_.GetPlugin(blankMasterDependentEsm).get()),
                        PluginMetadata(),
                        PluginMetadata(),
                        getLoadOrder(),
                        game_.Type(),
                        game_.GetCache()->GetPlugins());
  EXPECT_EQ(4, plugin.NumOverrideFormIDs());
}

TEST_P(
    PluginSortingDataTest,
    constructorShouldUseTotalRecordCountAsOverrideFormIdCountForTes3PluginWithAMasterThatIsNotLoaded) {
  if (GetParam() != GameType::tes3) {
    return;
  }

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  // Pretend that blankEsm isn't loaded.
  auto loadedPlugins = game_.GetCache()->GetPlugins();
  for (auto it = loadedPlugins.begin(); it != loadedPlugins.end();) {
    if ((*it)->GetName() == blankEsm) {
      it = loadedPlugins.erase(it);
    } else {
      ++it;
    }
  }

  auto plugin =
      PluginSortingData(*dynamic_cast<const Plugin *>(
                            game_.GetPlugin(blankMasterDependentEsm).get()),
                        PluginMetadata(),
                        PluginMetadata(),
                        getLoadOrder(),
                        game_.Type(),
                        loadedPlugins);

  EXPECT_EQ(10, plugin.NumOverrideFormIDs());
}
}
}

#endif
