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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_TAG_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_TAG_TEST

#include <gtest/gtest.h>

#include "loot/metadata/tag.h"

namespace loot::test {
TEST(Tag,
     defaultConstructorShouldSetEmptyNameAndConditionStringsForATagAddition) {
  Tag tag;

  EXPECT_TRUE(tag.GetName().empty());
  EXPECT_TRUE(tag.IsAddition());
  EXPECT_TRUE(tag.GetCondition().empty());
}

TEST(Tag, dataConstructorShouldSetFieldsToGivenValues) {
  Tag tag("name", false, "condition");

  EXPECT_EQ("name", tag.GetName());
  EXPECT_FALSE(tag.IsAddition());
  EXPECT_EQ("condition", tag.GetCondition());
}

TEST(Tag, orderingShouldCompareNamesAndConditionsLexicographically) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_EQ(std::weak_ordering::equivalent, tag1 <=> tag2);

  tag1 = Tag("name");
  tag2 = Tag("Name");

  EXPECT_EQ(std::weak_ordering::greater, tag1 <=> tag2);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "Condition");

  EXPECT_EQ(std::weak_ordering::greater, tag1 <=> tag2);

  tag1 = Tag("name1");
  tag2 = Tag("name2");

  EXPECT_EQ(std::weak_ordering::less, tag1 <=> tag2);

  tag1 = Tag("name", true, "condition1");
  tag2 = Tag("name", true, "condition2");

  EXPECT_EQ(std::weak_ordering::less, tag1 <=> tag2);
}

TEST(Tag, orderingShouldMakeAdditionsLessThanRemovals) {
  Tag tag1("name", true);
  Tag tag2("name", false);

  EXPECT_EQ(std::weak_ordering::less, tag1 <=> tag2);
}
}

#endif
