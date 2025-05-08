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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_PLUGIN_CLEANING_DATA_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_PLUGIN_CLEANING_DATA_TEST

#include "loot/metadata/plugin_cleaning_data.h"
#include "tests/common_game_test_fixture.h"

namespace loot::test {
class PluginCleaningDataTest : public CommonGameTestFixture {
protected:
  PluginCleaningDataTest() :
      CommonGameTestFixture(GameType::tes4),
      info_(std::vector<MessageContent>({
          MessageContent("info"),
      })) {}

  const std::vector<MessageContent> info_;
};

TEST_F(PluginCleaningDataTest,
       defaultConstructorShouldLeaveAllCountsAtZeroAndTheUtilityStringEmpty) {
  PluginCleaningData info;
  EXPECT_EQ(0u, info.GetCRC());
  EXPECT_EQ(0u, info.GetITMCount());
  EXPECT_EQ(0u, info.GetDeletedReferenceCount());
  EXPECT_EQ(0u, info.GetDeletedNavmeshCount());
  EXPECT_TRUE(info.GetCleaningUtility().empty());
  EXPECT_TRUE(info.GetDetail().empty());
}

TEST_F(PluginCleaningDataTest, contentConstructorShouldStoreAllGivenData) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_EQ(0x12345678u, info.GetCRC());
  EXPECT_EQ(2u, info.GetITMCount());
  EXPECT_EQ(10u, info.GetDeletedReferenceCount());
  EXPECT_EQ(30u, info.GetDeletedNavmeshCount());
  EXPECT_EQ("cleaner", info.GetCleaningUtility());
  EXPECT_EQ(info_, info.GetDetail());
}
}

#endif
