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

#include <string>
#include <vector>

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
                 const std::vector<std::string>& afterGroups = {},
                 const std::string& description = "");

  /**
   * Check if two Group objects are equal by comparing their names.
   * @returns True if the names are case-sensitively equal, false otherwise.
   */
  LOOT_API bool operator==(const Group& rhs) const;

  /**
   * A less-than operator implemented with no semantics so that Group objects
   * can be stored in sets.
   * @returns True if this Group is less than the given Group, false
   *          otherwise.
   */
  LOOT_API bool operator<(const Group& rhs) const;

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
  LOOT_API std::vector<std::string> GetAfterGroups() const;

private:
  std::string name_;
  std::string description_;
  std::vector<std::string> afterGroups_;
};

/**
 * Check if two Group objects are not equal.
 * @returns True if the Group objects are not equal, false otherwise.
 */
LOOT_API bool operator!=(const Group& lhs, const Group& rhs);

/**
 * Check if the first Group object is greater than the second Group
 * object.
 * @returns True if the second Group object is less than the first Group
 *          object, false otherwise.
 */
LOOT_API bool operator>(const Group& lhs, const Group& rhs);

/**
 * Check if the first Group object is less than or equal to the second
 * Group object.
 * @returns True if the first Group object is not greater than the second
 *          Group object, false otherwise.
 */
LOOT_API bool operator<=(const Group& lhs, const Group& rhs);

/**
 * Check if the first Group object is greater than or equal to the second
 * Group object.
 * @returns True if the first Group object is not less than the second
 *          Group object, false otherwise.
 */
LOOT_API bool operator>=(const Group& lhs, const Group& rhs);
}

#endif
