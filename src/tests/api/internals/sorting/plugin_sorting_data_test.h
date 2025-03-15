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
#include "tests/api/internals/plugin_test.h"

namespace loot {
namespace test {
TEST(PluginSortingData, lightFlaggedEspFilesShouldNotBeTreatedAsMasters) {
  TestPlugin nonMaster;
  TestPlugin master;
  master.SetIsMaster(true);
  TestPlugin lightPlugin;
  lightPlugin.SetIsLightPlugin(true);
  TestPlugin lightMaster;
  lightMaster.SetIsLightPlugin(true);
  lightMaster.SetIsMaster(true);

  auto esp =
      PluginSortingData(&nonMaster, PluginMetadata(), PluginMetadata(), {});
  EXPECT_FALSE(esp.IsMaster());

  auto masterData =
      PluginSortingData(&master, PluginMetadata(), PluginMetadata(), {});
  EXPECT_TRUE(masterData.IsMaster());

  auto lightMasterData =
      PluginSortingData(&lightMaster, PluginMetadata(), PluginMetadata(), {});
  EXPECT_TRUE(lightMasterData.IsMaster());

  auto lightPluginData =
      PluginSortingData(&lightPlugin, PluginMetadata(), PluginMetadata(), {});
  EXPECT_FALSE(lightPluginData.IsMaster());
}

TEST(PluginSortingData,
     overrideRecordCountShouldEqualSizeOfOverlapWithThePluginsMasters) {
  auto count = 4;
  TestPlugin plugin;
  plugin.SetOverrideRecordCount(count);

  auto pluginData =
      PluginSortingData(&plugin, PluginMetadata(), PluginMetadata(), {});

  EXPECT_EQ(count, pluginData.GetOverrideRecordCount());
}

TEST(PluginSortingData,
     isBlueprintMasterShouldBeTrueIfPluginIsAMasterAndABlueprintPlugin) {
  TestPlugin master;
  TestPlugin blueprintPlugin;
  blueprintPlugin.SetIsBlueprintPlugin(true);
  TestPlugin blueprintMaster;
  blueprintMaster.SetIsBlueprintPlugin(true);
  blueprintMaster.SetIsMaster(true);

  auto plugin =
      PluginSortingData(&master, PluginMetadata(), PluginMetadata(), {});

  EXPECT_FALSE(plugin.IsBlueprintMaster());

  plugin = PluginSortingData(
      &blueprintPlugin, PluginMetadata(), PluginMetadata(), {});
  EXPECT_FALSE(plugin.IsBlueprintMaster());

  plugin = PluginSortingData(
      &blueprintMaster, PluginMetadata(), PluginMetadata(), {});
  EXPECT_TRUE(plugin.IsBlueprintMaster());
}
}
}

#endif
