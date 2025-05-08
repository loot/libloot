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
#ifndef LOOT_METADATA_FILENAME
#define LOOT_METADATA_FILENAME

#include <compare>
#include <string>
#include <string_view>

#include "loot/api_decorator.h"

namespace loot {
/**
 * Represents a case-insensitive filename.
 */
class Filename {
public:
  /**
   * Construct a Filename using an empty string.
   */
  LOOT_API Filename() = default;

  /**
   * Construct a Filename using the given string.
   */
  LOOT_API explicit Filename(std::string_view filename);

  /**
   * Get this Filename as a string.
   */
  LOOT_API explicit operator std::string() const;

private:
  std::string filename_;

  LOOT_API friend std::weak_ordering operator<=>(const Filename& lhs,
                                                 const Filename& rhs);

  LOOT_API friend bool operator==(const Filename& lhs, const Filename& rhs);
};

/**
 * Compare two Filename objects.
 *
 * Filenames are compared case-insensitively.
 */
LOOT_API std::weak_ordering operator<=>(const Filename& lhs,
                                        const Filename& rhs);

/**
 * Check if two Filename objects are equal by comparing their fields.
 * @returns True if the filenames are case-insensitively equal and all other
 *          fields are case-sensitively equal, false otherwise.
 */
LOOT_API bool operator==(const Filename& lhs, const Filename& rhs);
}

#endif
