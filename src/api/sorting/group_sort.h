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

#ifndef LOOT_API_SORTING_GROUP_SORT
#define LOOT_API_SORTING_GROUP_SORT

#include <string>
#include <unordered_map>
#include <vector>

#include "loot/metadata/group.h"
#include "loot/vertex.h"

namespace loot {
struct PredecessorGroup {
  std::string name;
  bool pathInvolvesUserMetadata{false};
};

// Map entries are a group name and names of transitive load after groups.
std::unordered_map<std::string, std::vector<PredecessorGroup>>
GetPredecessorGroups(const std::vector<Group>& masterlistGroups,
                     const std::vector<Group>& userGroups);

std::vector<Vertex> GetGroupsPath(const std::vector<Group>& masterlistGroups,
                                  const std::vector<Group>& userGroups,
                                  const std::string& fromGroupName,
                                  const std::string& toGroupName);

std::vector<Group> MergeGroups(const std::vector<Group>& masterlistGroups,
                               const std::vector<Group>& userGroups);
}
#endif
