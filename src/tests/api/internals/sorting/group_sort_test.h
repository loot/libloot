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

#include <gtest/gtest.h>

#include "api/sorting/group_sort.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

namespace loot {
namespace test {
TEST(BuildGroupGraph, shouldThrowIfAnAfterGroupDoesNotExist) {
  std::vector<Group> groups({Group("b", {"a"})});

  EXPECT_THROW(BuildGroupGraph(groups, {}), UndefinedGroupError);
}

TEST(GetGroupsPath, shouldThrowIfTheFromGroupDoesNotExist) {
  std::vector<Group> groups({Group("a"), Group("b", {"a"})});
  std::vector<Group> userGroups({Group("a", {"c"}), Group("c", {"b"})});

  EXPECT_THROW(GetGroupsPath(groups, userGroups, "d", "a"),
               std::invalid_argument);
}

TEST(GetGroupsPath, shouldThrowIfTheToGroupDoesNotExist) {
  std::vector<Group> groups({Group("a"), Group("b", {"a"})});
  std::vector<Group> userGroups({Group("a", {"c"}), Group("c", {"b"})});

  EXPECT_THROW(GetGroupsPath(groups, userGroups, "a", "d"),
               std::invalid_argument);
}

TEST(GetGroupsPath,
     shouldReturnAnEmptyVectorIfThereIsNoPathBetweenTheTwoGroups) {
  std::vector<Group> groups({Group("a", {}),
                             Group("b", {"a"}),
                             Group("c", {"a"}),
                             Group("d", {"c"}),
                             Group("e", {"b", "d"})});

  auto path = GetGroupsPath(groups, {}, "b", "d");

  EXPECT_TRUE(path.empty());
}

TEST(GetGroupsPath,
     shouldFindThePathWithTheLeastNumberOfEdgesInAMasterlistOnlyGraph) {
  std::vector<Group> groups({Group("a", {}),
                             Group("b", {"a"}),
                             Group("c", {"a"}),
                             Group("d", {"c"}),
                             Group("e", {"b", "d"})});

  auto path = GetGroupsPath(groups, {}, "a", "e");

  ASSERT_EQ(3, path.size());
  EXPECT_EQ("a", path[0].GetName());
  EXPECT_EQ(EdgeType::masterlistLoadAfter,
            path[0].GetTypeOfEdgeToNextVertex().value());
  EXPECT_EQ("b", path[1].GetName());
  EXPECT_EQ(EdgeType::masterlistLoadAfter,
            path[1].GetTypeOfEdgeToNextVertex().value());
  EXPECT_EQ("e", path[2].GetName());
  EXPECT_FALSE(path[2].GetTypeOfEdgeToNextVertex().has_value());
}

TEST(GetGroupsPath,
     shouldFindThePathWithTheLeastNumberOfEdgesThatContainsUserMetadata) {
  std::vector<Group> groups({Group("a", {}),
                             Group("b", {"a"}),
                             Group("c", {"a"}),
                             Group("e", {"b"})});
  std::vector<Group> userGroups({Group("d", {"c"}), Group("e", {"d"})});

  auto path = GetGroupsPath(groups, userGroups, "a", "e");

  ASSERT_EQ(4, path.size());
  EXPECT_EQ("a", path[0].GetName());
  EXPECT_EQ(EdgeType::masterlistLoadAfter,
            path[0].GetTypeOfEdgeToNextVertex().value());
  EXPECT_EQ("c", path[1].GetName());
  EXPECT_EQ(EdgeType::userLoadAfter,
            path[1].GetTypeOfEdgeToNextVertex().value());
  EXPECT_EQ("d", path[2].GetName());
  EXPECT_EQ(EdgeType::userLoadAfter,
            path[2].GetTypeOfEdgeToNextVertex().value());
  EXPECT_EQ("e", path[3].GetName());
  EXPECT_FALSE(path[3].GetTypeOfEdgeToNextVertex().has_value());
}

TEST(GetGroupsPath, shouldThrowIfMasterlistGroupLoadsAfterAUserlistGroup) {
  std::vector<Group> groups({Group("a", {}),
                             Group("b", {"a"}),
                             Group("c", {"a"}),
                             Group("e", {"b", "d"})});
  std::vector<Group> userGroups({Group("d", {"c"})});

  EXPECT_THROW(GetGroupsPath(groups, userGroups, "a", "e"),
               UndefinedGroupError);
}
}
}

#endif
