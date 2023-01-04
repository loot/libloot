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

#include <boost/graph/adjacency_list.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "loot/metadata/group.h"
#include "loot/vertex.h"

namespace loot {
typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::bidirectionalS,
                              std::string,
                              EdgeType>
    GroupGraph;

GroupGraph BuildGroupGraph(const std::vector<Group>& masterlistGroups,
                           const std::vector<Group>& userGroups);

std::vector<Vertex> GetGroupsPath(const GroupGraph& groupGraph,
                                  const std::string& fromGroupName,
                                  const std::string& toGroupName);
}
#endif
