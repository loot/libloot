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
#ifndef LOOT_METADATA_GROUP
#define LOOT_METADATA_GROUP

#include <unordered_set>
#include <string>

#include "loot/api_decorator.h"

namespace loot {
/**
 * Represents a group to which plugin metadata objects can belong.
 */
class Group {
public:
  /**
   * Construct a Group with the name "default" and an empty set of groups to
   * load after.
   * @return A Group object.
   */
  LOOT_API explicit Group();

  /**
   * Construct a Group with the given name, description and set of groups to
   * load after.
   * @param  name
   *         The group name.
   * @param  afterGroups
   *         The names of groups this group loads after.
   * @param  description
   *         A description of the group.
   * @return A Group object.
   */
  LOOT_API explicit Group(const std::string& name,
                 const std::unordered_set<std::string>& afterGroups = {},
                 const std::string& description = "");

  /**
   * Check if two Group objects are equal by comparing their names.
   * @returns True if the names are case-sensitively equal, false otherwise.
   */
  LOOT_API bool operator==(const Group& rhs) const;

  /**
   * Get the name of the group.
   * @return The group's name.
   */
  LOOT_API std::string GetName() const;

  /**
   * Get the description of the group.
   * @return The group's description.
   */
  LOOT_API std::string GetDescription() const;

  /**
   * Get the set of groups this group loads after.
   * @return A set of group names.
   */
  LOOT_API std::unordered_set<std::string> GetAfterGroups() const;

private:
  std::string name_;
  std::string description_;
  std::unordered_set<std::string> afterGroups_;
};
}

namespace std {
/**
 * A specialisation of std::hash for loot::Group.
 */
template<>
struct hash<loot::Group> {
  /**
   * Calculate a hash value for a loot::Group object.
   * @return The hash generated from the group's name.
   */
  size_t operator()(const loot::Group& group) const {
    return hash<string>()(group.GetName());
  }
};
}

#endif
