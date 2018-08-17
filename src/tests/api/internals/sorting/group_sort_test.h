/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2018    WrinklyNinja

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

#ifndef LOOT_TESTS_API_INTERNALS_SORTING_GROUP_SORT_TEST
#define LOOT_TESTS_API_INTERNALS_SORTING_GROUP_SORT_TEST

#include "api/sorting/group_sort.h"

#include <gtest/gtest.h>

#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
namespace test {
TEST(GetTransitiveAfterGroups, shouldMapGroupsToTheirTransitiveAfterGroups) {
  std::unordered_set<Group> groups({
    Group("a"),
    Group("b", std::unordered_set<std::string>({ "a" })),
    Group("c", std::unordered_set<std::string>({"b"}))
  });

  auto mapped = GetTransitiveAfterGroups(groups);

  EXPECT_TRUE(mapped["a"].empty());
  EXPECT_EQ(std::unordered_set<std::string>({ "a" }), mapped["b"]);
  EXPECT_EQ(std::unordered_set<std::string>({ "a", "b" }), mapped["c"]);
}

TEST(GetTransitiveAfterGroups, shouldThrowIfAnAfterGroupDoesNotExist) {
  std::unordered_set<Group> groups({
    Group("b", std::unordered_set<std::string>({ "a" }))
  });

  EXPECT_THROW(GetTransitiveAfterGroups(groups), UndefinedGroupError);
}

TEST(GetTransitiveAfterGroups, shouldThrowIfAfterGroupsAreCyclic) {
  std::unordered_set<Group> groups({
    Group("a", std::unordered_set<std::string>({ "c" })),
    Group("b", std::unordered_set<std::string>({ "a" })),
    Group("c", std::unordered_set<std::string>({ "b" }))
  });

  EXPECT_THROW(GetTransitiveAfterGroups(groups), CyclicInteractionError);
}
}
}

#endif
