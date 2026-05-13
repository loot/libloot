/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2026 Oliver Hamlet

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

#include <gtest/gtest.h>

#include "loot/metadata/plugin_cleaning_data.h"

namespace loot::test {
TEST(PluginCleaningData,
     defaultConstructorShouldLeaveAllCountsAtZeroAndTheUtilityStringEmpty) {
  PluginCleaningData data;
  EXPECT_EQ(0u, data.GetCRC());
  EXPECT_EQ(0u, data.GetITMCount());
  EXPECT_EQ(0u, data.GetDeletedReferenceCount());
  EXPECT_EQ(0u, data.GetDeletedNavmeshCount());
  EXPECT_TRUE(data.GetCleaningUtility().empty());
  EXPECT_TRUE(data.GetDetail().empty());
  EXPECT_TRUE(data.GetCondition().empty());
}

TEST(PluginCleaningData, contentConstructorShouldStoreAllGivenData) {
  const std::vector<MessageContent> info{MessageContent("info")};

  PluginCleaningData data(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_EQ(0x12345678u, data.GetCRC());
  EXPECT_EQ(2u, data.GetITMCount());
  EXPECT_EQ(10u, data.GetDeletedReferenceCount());
  EXPECT_EQ(30u, data.GetDeletedNavmeshCount());
  EXPECT_EQ("cleaner", data.GetCleaningUtility());
  EXPECT_EQ(info, data.GetDetail());
  EXPECT_EQ("condition", data.GetCondition());
}

TEST(PluginCleaningData, equalityShouldCheckEqualityOfAllFields) {
  const std::vector<MessageContent> info{MessageContent("info")};

  PluginCleaningData data1(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  PluginCleaningData data2(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x87654321, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "Cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner1", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner2", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 = PluginCleaningData(0x12345678,
                             "cleaner",
                             std::vector<MessageContent>(),
                             2,
                             10,
                             30,
                             "condition");
  EXPECT_FALSE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 4, 10, 30, "condition");
  EXPECT_FALSE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 20, 30, "condition");
  EXPECT_FALSE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 60, "condition");
  EXPECT_FALSE(data1 == data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "Condition");
  EXPECT_FALSE(data1 == data2);
}

TEST(PluginCleaningData, inequalityShouldBeTheInverseOfEquality) {
  const std::vector<MessageContent> info{MessageContent("info")};

  PluginCleaningData data1(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  PluginCleaningData data2(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x87654321, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "Cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner1", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner2", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 = PluginCleaningData(0x12345678,
                             "cleaner",
                             std::vector<MessageContent>(),
                             2,
                             10,
                             30,
                             "condition");
  EXPECT_TRUE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 4, 10, 30, "condition");
  EXPECT_TRUE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 20, 30, "condition");
  EXPECT_TRUE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 60, "condition");
  EXPECT_TRUE(data1 != data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "Condition");
  EXPECT_TRUE(data1 != data2);
}

TEST(PluginCleaningData, lessThanOperatorShouldCompareAllFields) {
  const std::vector<MessageContent> info{MessageContent("info")};

  PluginCleaningData data1(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  PluginCleaningData data2(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 < data2);
  EXPECT_FALSE(data2 < data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x87654321, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 < data2);
  EXPECT_FALSE(data2 < data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "Cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data2 < data1);
  EXPECT_FALSE(data1 < data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner1", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner2", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 < data2);
  EXPECT_FALSE(data2 < data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 = PluginCleaningData(0x12345678,
                             "cleaner",
                             std::vector<MessageContent>(),
                             2,
                             10,
                             30,
                             "condition");
  EXPECT_TRUE(data2 < data1);
  EXPECT_FALSE(data1 < data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 4, 10, 30, "condition");
  EXPECT_TRUE(data1 < data2);
  EXPECT_FALSE(data2 < data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 20, 30, "condition");
  EXPECT_TRUE(data1 < data2);
  EXPECT_FALSE(data2 < data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 60, "condition");
  EXPECT_TRUE(data1 < data2);
  EXPECT_FALSE(data2 < data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "Condition");
  EXPECT_TRUE(data2 < data1);
  EXPECT_FALSE(data1 < data2);
}

TEST(
    PluginCleaningData,
    greaterThanOperatorShouldReturnTrueIfTheSecondPluginCleaningDataIsLessThanTheFirst) {
  const std::vector<MessageContent> info{MessageContent("info")};

  PluginCleaningData data1(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  PluginCleaningData data2(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 > data2);
  EXPECT_FALSE(data2 > data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x87654321, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 > data2);
  EXPECT_TRUE(data2 > data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "Cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data2 > data1);
  EXPECT_TRUE(data1 > data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner1", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner2", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 > data2);
  EXPECT_TRUE(data2 > data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 = PluginCleaningData(0x12345678,
                             "cleaner",
                             std::vector<MessageContent>(),
                             2,
                             10,
                             30,
                             "condition");
  EXPECT_FALSE(data2 > data1);
  EXPECT_TRUE(data1 > data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 4, 10, 30, "condition");
  EXPECT_FALSE(data1 > data2);
  EXPECT_TRUE(data2 > data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 20, 30, "condition");
  EXPECT_FALSE(data1 > data2);
  EXPECT_TRUE(data2 > data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 60, "condition");
  EXPECT_FALSE(data1 > data2);
  EXPECT_TRUE(data2 > data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "Condition");
  EXPECT_FALSE(data2 > data1);
  EXPECT_TRUE(data1 > data2);
}

TEST(
    PluginCleaningData,
    lessThanOrEqualOperatorShouldReturnTrueIfTheFirstPluginCleaningDataIsNotGreaterThanTheSecond) {
  const std::vector<MessageContent> info{MessageContent("info")};

  PluginCleaningData data1(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  PluginCleaningData data2(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 <= data2);
  EXPECT_TRUE(data2 <= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x87654321, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 <= data2);
  EXPECT_FALSE(data2 <= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "Cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data2 <= data1);
  EXPECT_FALSE(data1 <= data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner1", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner2", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 <= data2);
  EXPECT_FALSE(data2 <= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 = PluginCleaningData(0x12345678,
                             "cleaner",
                             std::vector<MessageContent>(),
                             2,
                             10,
                             30,
                             "condition");
  EXPECT_TRUE(data2 <= data1);
  EXPECT_FALSE(data1 <= data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 4, 10, 30, "condition");
  EXPECT_TRUE(data1 <= data2);
  EXPECT_FALSE(data2 <= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 20, 30, "condition");
  EXPECT_TRUE(data1 <= data2);
  EXPECT_FALSE(data2 <= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 60, "condition");
  EXPECT_TRUE(data1 <= data2);
  EXPECT_FALSE(data2 <= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "Condition");
  EXPECT_TRUE(data2 <= data1);
  EXPECT_FALSE(data1 <= data2);
}

TEST(
    PluginCleaningData,
    greaterThanOrEqualToOperatorShouldReturnTrueIfTheFirstPluginCleaningDataIsNotLessThanTheSecond) {
  const std::vector<MessageContent> info{MessageContent("info")};

  PluginCleaningData data1(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  PluginCleaningData data2(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_TRUE(data1 >= data2);
  EXPECT_TRUE(data2 >= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x87654321, "cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 >= data2);
  EXPECT_TRUE(data2 >= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "Cleaner", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data2 >= data1);
  EXPECT_TRUE(data1 >= data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner1", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner2", info, 2, 10, 30, "condition");
  EXPECT_FALSE(data1 >= data2);
  EXPECT_TRUE(data2 >= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 = PluginCleaningData(0x12345678,
                             "cleaner",
                             std::vector<MessageContent>(),
                             2,
                             10,
                             30,
                             "condition");
  EXPECT_FALSE(data2 >= data1);
  EXPECT_TRUE(data1 >= data2);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 4, 10, 30, "condition");
  EXPECT_FALSE(data1 >= data2);
  EXPECT_TRUE(data2 >= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 20, 30, "condition");
  EXPECT_FALSE(data1 >= data2);
  EXPECT_TRUE(data2 >= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 60, "condition");
  EXPECT_FALSE(data1 >= data2);
  EXPECT_TRUE(data2 >= data1);

  data1 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "condition");
  data2 =
      PluginCleaningData(0x12345678, "cleaner", info, 2, 10, 30, "Condition");
  EXPECT_FALSE(data2 >= data1);
  EXPECT_TRUE(data1 >= data2);
}
}

#endif
