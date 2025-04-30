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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_CONDITIONAL_METADATA_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_CONDITIONAL_METADATA_TEST

#include "loot/metadata/conditional_metadata.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class ConditionalMetadataTest : public CommonGameTestFixture,
                                public testing::WithParamInterface<GameType> {
protected:
  ConditionalMetadataTest() : CommonGameTestFixture(GetParam()) {}
  ConditionalMetadata conditionalMetadata_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         ConditionalMetadataTest,
                         ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(ConditionalMetadataTest,
       defaultConstructorShouldSetEmptyConditionString) {
  EXPECT_TRUE(conditionalMetadata_.GetCondition().empty());
}

TEST_P(ConditionalMetadataTest,
       stringConstructorShouldSetConditionToGivenString) {
  std::string condition("condition");
  conditionalMetadata_ = ConditionalMetadata(condition);

  EXPECT_EQ(condition, conditionalMetadata_.GetCondition());
}

TEST_P(ConditionalMetadataTest,
       isConditionalShouldBeFalseForAnEmptyConditionString) {
  EXPECT_FALSE(conditionalMetadata_.IsConditional());
}

TEST_P(ConditionalMetadataTest,
       isConditionalShouldBeTrueForANonEmptyConditionString) {
  conditionalMetadata_ = ConditionalMetadata("condition");
  EXPECT_TRUE(conditionalMetadata_.IsConditional());
}
}
}

#endif
