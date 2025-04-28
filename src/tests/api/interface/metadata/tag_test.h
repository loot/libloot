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

TEST(Tag, equalityShouldBeCaseSensitiveOnNameAndCondition) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_TRUE(tag1 == tag2);

  tag1 = Tag("name");
  tag2 = Tag("Name");

  EXPECT_FALSE(tag1 == tag2);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "Condition");

  EXPECT_FALSE(tag1 == tag2);

  tag1 = Tag("name1");
  tag2 = Tag("name2");

  EXPECT_FALSE(tag1 == tag2);

  tag1 = Tag("name", true, "condition1");
  tag2 = Tag("name", true, "condition2");

  EXPECT_FALSE(tag1 == tag2);
}

TEST(Tag, equalityShouldRequireEqualAdditionStates) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_TRUE(tag1 == tag2);

  tag1 = Tag("name", true);
  tag2 = Tag("name", false);

  EXPECT_FALSE(tag1 == tag2);
}

TEST(Tag, inequalityShouldBeTheInverseOfEquality) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_FALSE(tag1 != tag2);

  tag1 = Tag("name");
  tag2 = Tag("Name");

  EXPECT_TRUE(tag1 != tag2);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "Condition");

  EXPECT_TRUE(tag1 != tag2);

  tag1 = Tag("name1");
  tag2 = Tag("name2");

  EXPECT_TRUE(tag1 != tag2);

  tag1 = Tag("name", true, "condition1");
  tag2 = Tag("name", true, "condition2");

  EXPECT_TRUE(tag1 != tag2);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "condition");

  EXPECT_FALSE(tag1 != tag2);

  tag1 = Tag("name", true);
  tag2 = Tag("name", false);

  EXPECT_TRUE(tag1 != tag2);
}

TEST(
    Tag,
    lessThanOperatorShouldUseCaseSensitiveLexicographicalComparisonForNameAndCondition) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_FALSE(tag1 < tag2);
  EXPECT_FALSE(tag2 < tag1);

  tag1 = Tag("name");
  tag2 = Tag("Name");

  EXPECT_FALSE(tag1 < tag2);
  EXPECT_TRUE(tag2 < tag1);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "Condition");

  EXPECT_FALSE(tag1 < tag2);
  EXPECT_TRUE(tag2 < tag1);

  tag1 = Tag("name1");
  tag2 = Tag("name2");

  EXPECT_TRUE(tag1 < tag2);
  EXPECT_FALSE(tag2 < tag1);

  tag1 = Tag("name", true, "condition1");
  tag2 = Tag("name", true, "condition2");

  EXPECT_TRUE(tag1 < tag2);
  EXPECT_FALSE(tag2 < tag1);
}

TEST(Tag, lessThanOperatorShouldTreatTagAdditionsAsBeingLessThanRemovals) {
  Tag tag1("name", true);
  Tag tag2("name", false);

  EXPECT_TRUE(tag1 < tag2);
  EXPECT_FALSE(tag2 < tag1);
}

TEST(Tag, greaterThanOperatorShouldReturnTrueIfTheSecondTagIsLessThanTheFirst) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_FALSE(tag1 > tag2);
  EXPECT_FALSE(tag2 > tag1);

  tag1 = Tag("name");
  tag2 = Tag("Name");

  EXPECT_TRUE(tag1 > tag2);
  EXPECT_FALSE(tag2 > tag1);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "Condition");

  EXPECT_TRUE(tag1 > tag2);
  EXPECT_FALSE(tag2 > tag1);

  tag1 = Tag("name1");
  tag2 = Tag("name2");

  EXPECT_FALSE(tag1 > tag2);
  EXPECT_TRUE(tag2 > tag1);

  tag1 = Tag("name", true, "condition1");
  tag2 = Tag("name", true, "condition2");

  EXPECT_FALSE(tag1 > tag2);
  EXPECT_TRUE(tag2 > tag1);

  tag1 = Tag("name", true);
  tag2 = Tag("name", false);

  EXPECT_FALSE(tag1 > tag2);
  EXPECT_TRUE(tag2 > tag1);
}

TEST(
    Tag,
    lessThanOrEqualOperatorShouldReturnTrueIfTheFirstTagIsNotGreaterThanTheSecond) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_TRUE(tag1 <= tag2);
  EXPECT_TRUE(tag2 <= tag1);

  tag1 = Tag("name");
  tag2 = Tag("Name");

  EXPECT_FALSE(tag1 <= tag2);
  EXPECT_TRUE(tag2 <= tag1);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "Condition");

  EXPECT_FALSE(tag1 <= tag2);
  EXPECT_TRUE(tag2 <= tag1);

  tag1 = Tag("name1");
  tag2 = Tag("name2");

  EXPECT_TRUE(tag1 <= tag2);
  EXPECT_FALSE(tag2 <= tag1);

  tag1 = Tag("name", true, "condition1");
  tag2 = Tag("name", true, "condition2");

  EXPECT_TRUE(tag1 <= tag2);
  EXPECT_FALSE(tag2 <= tag1);

  tag1 = Tag("name", true);
  tag2 = Tag("name", false);

  EXPECT_TRUE(tag1 <= tag2);
  EXPECT_FALSE(tag2 <= tag1);
}

TEST(
    Tag,
    greaterThanOrEqualToOperatorShouldReturnTrueIfTheFirstTagIsNotLessThanTheSecond) {
  Tag tag1("name", true, "condition");
  Tag tag2("name", true, "condition");

  EXPECT_TRUE(tag1 >= tag2);
  EXPECT_TRUE(tag2 >= tag1);

  tag1 = Tag("name");
  tag2 = Tag("Name");

  EXPECT_TRUE(tag1 >= tag2);
  EXPECT_FALSE(tag2 >= tag1);

  tag1 = Tag("name", true, "condition");
  tag2 = Tag("name", true, "Condition");

  EXPECT_TRUE(tag1 >= tag2);
  EXPECT_FALSE(tag2 >= tag1);

  tag1 = Tag("name1");
  tag2 = Tag("name2");

  EXPECT_FALSE(tag1 >= tag2);
  EXPECT_TRUE(tag2 >= tag1);

  tag1 = Tag("name", true, "condition1");
  tag2 = Tag("name", true, "condition2");

  EXPECT_FALSE(tag1 >= tag2);
  EXPECT_TRUE(tag2 >= tag1);

  tag1 = Tag("name", true);
  tag2 = Tag("name", false);

  EXPECT_FALSE(tag1 >= tag2);
  EXPECT_TRUE(tag2 >= tag1);
}
}

#endif
