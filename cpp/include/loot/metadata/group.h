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
#include <string_view>
#include <vector>

#include "loot/api_decorator.h"

namespace loot {
/**
 * Represents a group to which plugin metadata objects can belong.
 */
class Group {
public:
  /**
   * The name of the group to which all plugins belong by default.
   */
  static constexpr std::string_view DEFAULT_NAME = "default";

  /**
   * Construct a Group with the name "default" and an empty set of groups to
   * load after.
   */
  LOOT_API Group() = default;

  /**
   * Construct a Group with the given name, description and set of groups to
   * load after.
   * @param  name
   *         The group name.
   * @param  afterGroups
   *         The names of groups this group loads after.
   * @param  description
   *         A description of the group.
   */
  LOOT_API explicit Group(std::string_view name,
                          const std::vector<std::string>& afterGroups = {},
                          std::string_view description = "");

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

  /**
   * Compares two Group objects.
   */
  LOOT_API auto operator<=>(const Group& rhs) const = default;

private:
  std::string name_{DEFAULT_NAME};
  std::string description_;
  std::vector<std::string> afterGroups_;
};
}

#endif
