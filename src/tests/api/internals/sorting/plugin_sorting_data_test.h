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

  std::vector<const PluginInterface *> getLoadedPlugins() {
    std::vector<const PluginInterface *> loadedPluginInterfaces;
    const auto loadedPlugins = game_.GetCache().GetPlugins();
    std::transform(loadedPlugins.begin(),
                   loadedPlugins.end(),
                   std::back_inserter(loadedPluginInterfaces),
                   [](const Plugin *plugin) { return plugin; });

    return loadedPluginInterfaces;
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
                                           GameType::fo4,
                                           GameType::starfield));

TEST_P(PluginSortingDataTest, lightFlaggedEspFilesShouldNotBeTreatedAsMasters) {
  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    ASSERT_NO_THROW(
        std::filesystem::copy(dataPath / blankEsl, dataPath / blankEslEsp));
  }

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  const auto loadedPlugins = getLoadedPlugins();

  auto esp = PluginSortingData(
      dynamic_cast<const PluginSortingInterface *>(game_.GetPlugin(blankEsp)),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder());
  EXPECT_FALSE(esp.IsMaster());

  auto master = PluginSortingData(
      dynamic_cast<const PluginSortingInterface *>(game_.GetPlugin(blankEsm)),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder());
  EXPECT_TRUE(master.IsMaster());

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    auto lightMaster = PluginSortingData(
        dynamic_cast<const PluginSortingInterface *>(game_.GetPlugin(blankEsl)),
        PluginMetadata(),
        PluginMetadata(),
        getLoadOrder());
    EXPECT_TRUE(lightMaster.IsMaster());

    auto lightPlugin =
        PluginSortingData(dynamic_cast<const PluginSortingInterface *>(
                              game_.GetPlugin(blankEslEsp)),
                          PluginMetadata(),
                          PluginMetadata(),
                          getLoadOrder());
    EXPECT_FALSE(lightPlugin.IsMaster());
  }
}

TEST_P(PluginSortingDataTest,
       overrideRecordCountShouldEqualSizeOfOverlapWithThePluginsMasters) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  auto plugin = PluginSortingData(
      dynamic_cast<const Plugin *>(game_.GetPlugin(blankMasterDependentEsm)),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder());
  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(1, plugin.GetOverrideRecordCount());
  } else {
    EXPECT_EQ(4, plugin.GetOverrideRecordCount());
  }
}

TEST_P(PluginSortingDataTest,
       isBlueprintMasterShouldBeTrueIfPluginIsAMasterAndABlueprintPlugin) {
  SetBlueprintFlag(dataPath / blankEsm);
  SetBlueprintFlag(dataPath / blankEsp);

  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  auto plugin = PluginSortingData(
      dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsm)),
      PluginMetadata(),
      PluginMetadata(),
                        getLoadOrder());
  if (GetParam() == GameType::starfield) {
    EXPECT_TRUE(plugin.IsBlueprintMaster());
  } else {
    EXPECT_FALSE(plugin.IsBlueprintMaster());
  }

  plugin = PluginSortingData(
      dynamic_cast<const Plugin *>(game_.GetPlugin(blankDifferentEsm)),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder());
  EXPECT_FALSE(plugin.IsBlueprintMaster());

  plugin = PluginSortingData(dynamic_cast<const Plugin *>(game_.GetPlugin(blankEsp)),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder());
  EXPECT_FALSE(plugin.IsBlueprintMaster());

  plugin = PluginSortingData(
      dynamic_cast<const Plugin *>(game_.GetPlugin(blankDifferentEsp)),
      PluginMetadata(),
      PluginMetadata(),
      getLoadOrder());
  EXPECT_FALSE(plugin.IsBlueprintMaster());
}
}
}

#endif
