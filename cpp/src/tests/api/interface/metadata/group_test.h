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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_GROUP_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_GROUP_TEST

#include <gtest/gtest.h>

#include "loot/metadata/group.h"

namespace loot::test {
TEST(Group, defaultConstructorShouldCreateDefaultGroup) {
  Group group;

  EXPECT_EQ("default", group.GetName());
  EXPECT_TRUE(group.GetAfterGroups().empty());
}

TEST(Group,
     allArgsConstructorShouldSetDescriptionAndAfterGroupsDefaultsAsEmpty) {
  Group group("group1");

  EXPECT_EQ("group1", group.GetName());
  EXPECT_TRUE(group.GetDescription().empty());
  EXPECT_TRUE(group.GetAfterGroups().empty());
}

TEST(Group, allArgsConstructorShouldStoreGivenValues) {
  Group group("group1", {"other_group"}, "test");

  EXPECT_EQ("group1", group.GetName());
  EXPECT_EQ("test", group.GetDescription());
  EXPECT_EQ(std::vector<std::string>({"other_group"}), group.GetAfterGroups());
}
}

#endif
