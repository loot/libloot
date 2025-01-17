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
bool operator==(const PredecessorGroup& lhs, const PredecessorGroup& rhs) {
  return lhs.pathInvolvesUserMetadata == rhs.pathInvolvesUserMetadata &&
         lhs.name == rhs.name;
}

namespace test {
TEST(BuildGroupGraph, shouldThrowIfAnAfterGroupDoesNotExist) {
  std::vector<Group> groups({Group("b", {"a"})});

  EXPECT_THROW(BuildGroupGraph(groups, {}), UndefinedGroupError);
}

TEST(BuildGroupGraph, shouldThrowIfMasterlistGroupLoadsAfterAUserlistGroup) {
  std::vector<Group> groups({Group("a", {}),
                             Group("b", {"a"}),
                             Group("c", {"a"}),
                             Group("e", {"b", "d"})});
  std::vector<Group> userGroups({Group("d", {"c"})});

  EXPECT_THROW(BuildGroupGraph(groups, userGroups), UndefinedGroupError);
}

TEST(BuildGroupGraph, shouldThrowIfAfterGroupsAreCyclic) {
  std::vector<Group> groups({Group("a"), Group("b", {"a"})});
  std::vector<Group> userGroups({Group("a", {"c"}), Group("c", {"b"})});

  try {
    const auto groupGraph = BuildGroupGraph(groups, userGroups);
    FAIL();
  } catch (CyclicInteractionError& e) {
    ASSERT_EQ(3, e.GetCycle().size());

    // Vertices can be added in any order, so which group is first is
    // undefined.
    if (e.GetCycle()[0].GetName() == "a") {
      EXPECT_EQ(EdgeType::userLoadAfter,
                e.GetCycle()[0].GetTypeOfEdgeToNextVertex());

      EXPECT_EQ("c", e.GetCycle()[1].GetName());
      EXPECT_EQ(EdgeType::userLoadAfter,
                e.GetCycle()[1].GetTypeOfEdgeToNextVertex());

      EXPECT_EQ("b", e.GetCycle()[2].GetName());
      EXPECT_EQ(EdgeType::masterlistLoadAfter,
                e.GetCycle()[2].GetTypeOfEdgeToNextVertex());
    } else if (e.GetCycle()[0].GetName() == "b") {
      EXPECT_EQ(EdgeType::masterlistLoadAfter,
                e.GetCycle()[0].GetTypeOfEdgeToNextVertex());

      EXPECT_EQ("a", e.GetCycle()[1].GetName());
      EXPECT_EQ(EdgeType::userLoadAfter,
                e.GetCycle()[1].GetTypeOfEdgeToNextVertex());

      EXPECT_EQ("c", e.GetCycle()[2].GetName());
      EXPECT_EQ(EdgeType::userLoadAfter,
                e.GetCycle()[2].GetTypeOfEdgeToNextVertex());
    } else {
      EXPECT_EQ("c", e.GetCycle()[0].GetName());
      EXPECT_EQ(EdgeType::userLoadAfter,
                e.GetCycle()[0].GetTypeOfEdgeToNextVertex());

      EXPECT_EQ("b", e.GetCycle()[1].GetName());
      EXPECT_EQ(EdgeType::masterlistLoadAfter,
                e.GetCycle()[1].GetTypeOfEdgeToNextVertex());

      EXPECT_EQ("a", e.GetCycle()[2].GetName());
      EXPECT_EQ(EdgeType::userLoadAfter,
                e.GetCycle()[2].GetTypeOfEdgeToNextVertex());
    }
  }
}

TEST(BuildGroupGraph, shouldNotThrowIfThereIsNoCycle) {
  std::vector<Group> groups({Group("a"), Group("b", {"a"})});

  EXPECT_NO_THROW(BuildGroupGraph(groups, {}));
}

TEST(BuildGroupGraph, shouldThrowIfThereIsACycle) {
  std::vector<Group> groups({Group("a", {"b"}), Group("b", {"a"})});

  try {
    BuildGroupGraph(groups, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("a", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("b", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST(BuildGroupGraph,
     exceptionThrownShouldOnlyRecordGroupsThatArePartOfTheCycle) {
  std::vector<Group> groups(
      {Group("a", {"b"}), Group("b", {"a"}), Group("c", {"b"})});

  try {
    BuildGroupGraph(groups, {});
    FAIL();
  } catch (const CyclicInteractionError& e) {
    ASSERT_EQ(2, e.GetCycle().size());
    EXPECT_EQ("a", e.GetCycle()[0].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ("b", e.GetCycle()[1].GetName());
    EXPECT_EQ(EdgeType::masterlistLoadAfter,
              e.GetCycle()[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST(GetPredecessorGroups, shouldMapGroupsToTheirPredecessorGroups) {
  std::vector<Group> groups({Group("a"), Group("b", {"a"}), Group("c", {"b"})});

  const auto groupGraph = BuildGroupGraph(groups, {});
  auto predecessors = GetPredecessorGroups(groupGraph);

  EXPECT_TRUE(predecessors["a"].empty());
  EXPECT_EQ(std::vector<PredecessorGroup>({{"a"}}), predecessors["b"]);
  EXPECT_EQ(std::vector<PredecessorGroup>({{"b"}, {"a"}}), predecessors["c"]);
}

TEST(GetPredecessorGroups,
     shouldRecordIfADirectSuccessorIsDefinedInUserMetadata) {
  std::vector<Group> masterlistGroups({Group("a")});
  std::vector<Group> userlistGroups({Group("b", {"a"})});

  const auto groupGraph = BuildGroupGraph(masterlistGroups, userlistGroups);
  auto predecessors = GetPredecessorGroups(groupGraph);

  EXPECT_EQ(std::vector<PredecessorGroup>({{"a", true}}), predecessors["b"]);
}

TEST(GetPredecessorGroups,
     shouldRecordIfADirectPredecessorIsLinkedDueToUserMetadata) {
  std::vector<Group> masterlistGroups({Group("a"), Group("b")});
  std::vector<Group> userlistGroups({Group("b", {"a"})});

  const auto groupGraph = BuildGroupGraph(masterlistGroups, userlistGroups);
  auto predecessors = GetPredecessorGroups(groupGraph);

  EXPECT_EQ(std::vector<PredecessorGroup>({{"a", true}}), predecessors["b"]);
}

TEST(GetPredecessorGroups,
     shouldRecordIfAnIndirectSuccessorIsDefinedInUserMetadata) {
  std::vector<Group> masterlistGroups({Group("a"), Group("b", {"a"})});
  std::vector<Group> userlistGroups({Group("c", {"b"})});

  const auto groupGraph = BuildGroupGraph(masterlistGroups, userlistGroups);
  auto predecessors = GetPredecessorGroups(groupGraph);

  EXPECT_EQ(std::vector<PredecessorGroup>({{"a"}}), predecessors["b"]);
  EXPECT_EQ(std::vector<PredecessorGroup>({{"b", true}, {"a", true}}),
            predecessors["c"]);
}

TEST(GetPredecessorGroups,
     shouldRecordIfAnIndirectPredecessorIsLinkedDueToUserMetadata) {
  std::vector<Group> masterlistGroups(
      {Group("a"), Group("b"), Group("c", {"b"})});
  std::vector<Group> userlistGroups({Group("b", {"a"})});

  const auto groupGraph = BuildGroupGraph(masterlistGroups, userlistGroups);
  auto predecessors = GetPredecessorGroups(groupGraph);

  EXPECT_EQ(std::vector<PredecessorGroup>({{"a", true}}), predecessors["b"]);
  EXPECT_EQ(std::vector<PredecessorGroup>({{"b"}, {"a", true}}),
            predecessors["c"]);
}

TEST(GetPredecessorGroups,
     shouldNotLeakUserMetadataInvolvementToSeparatePaths) {
  // This arrangement of groups ensures that a masterlist-sourced edge is
  // followed after a userlist-sourced edge along a different path, to check
  // encountering a userlist-sourced edge along one path does not poison
  // discovery of other paths.
  std::vector<Group> masterlistGroups(
      {Group("a"), Group("b"), Group("c"), Group("d", {"b", "c"})});
  std::vector<Group> userlistGroups({Group("b", {"a"})});

  const auto groupGraph = BuildGroupGraph(masterlistGroups, userlistGroups);
  auto predecessors = GetPredecessorGroups(groupGraph);

  EXPECT_EQ(std::vector<PredecessorGroup>({{"b"}, {"a", true}, {"c"}}),
            predecessors["d"]);
}

TEST(GetGroupsPath, shouldThrowIfTheFromGroupDoesNotExist) {
  std::vector<Group> groups({Group("a"), Group("b", {"a"})});
  std::vector<Group> userGroups({Group("a", {"c"}), Group("c")});

  const auto groupGraph = BuildGroupGraph(groups, userGroups);

  EXPECT_THROW(GetGroupsPath(groupGraph, "d", "a"), std::invalid_argument);
}

TEST(GetGroupsPath, shouldThrowIfTheToGroupDoesNotExist) {
  std::vector<Group> groups({Group("a"), Group("b", {"a"})});
  std::vector<Group> userGroups({Group("a", {"c"}), Group("c")});

  const auto groupGraph = BuildGroupGraph(groups, userGroups);

  EXPECT_THROW(GetGroupsPath(groupGraph, "a", "d"), std::invalid_argument);
}

TEST(GetGroupsPath,
     shouldReturnAnEmptyVectorIfThereIsNoPathBetweenTheTwoGroups) {
  std::vector<Group> groups({Group("a", {}),
                             Group("b", {"a"}),
                             Group("c", {"a"}),
                             Group("d", {"c"}),
                             Group("e", {"b", "d"})});

  const auto groupGraph = BuildGroupGraph(groups, {});
  auto path = GetGroupsPath(groupGraph, "b", "d");

  EXPECT_TRUE(path.empty());
}

TEST(GetGroupsPath,
     shouldFindThePathWithTheLeastNumberOfEdgesInAMasterlistOnlyGraph) {
  std::vector<Group> groups({Group("a", {}),
                             Group("b", {"a"}),
                             Group("c", {"a"}),
                             Group("d", {"c"}),
                             Group("e", {"b", "d"})});

  const auto groupGraph = BuildGroupGraph(groups, {});
  auto path = GetGroupsPath(groupGraph, "a", "e");

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

  const auto groupGraph = BuildGroupGraph(groups, userGroups);
  auto path = GetGroupsPath(groupGraph, "a", "e");

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
}
}

#endif
