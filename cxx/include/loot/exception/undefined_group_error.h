/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

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

#ifndef LOOT_EXCEPTION_UNDEFINED_GROUP_ERROR
#define LOOT_EXCEPTION_UNDEFINED_GROUP_ERROR

#include <stdexcept>
#include <string_view>

#include "loot/api_decorator.h"

namespace loot {
/**
 * @brief An exception class thrown if group is referenced but is undefined.
 */
class UndefinedGroupError : public std::runtime_error {
public:
  /**
   * @brief Construct an exception for an undefined group.
   * @param groupName The name of the group that is undefined.
   */
  LOOT_API UndefinedGroupError(std::string_view groupName);

  /**
   * Get the name of the undefined group.
   * @return A group name.
   */
  LOOT_API std::string GetGroupName() const;

private:
  std::string groupName_;
};
}

#endif
