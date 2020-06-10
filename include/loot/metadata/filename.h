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

#include <string>

#include "loot/api_decorator.h"

namespace loot {
/**
 * Represents a case-insensitive filename.
 */
class Filename {
public:
  /**
   * Construct a Filename using an empty string.
   * @return A Filename object.
   */
  LOOT_API explicit Filename();

  /**
   * Construct a Filename using the given string.
   * @return A Filename object.
   */
  LOOT_API explicit Filename(const std::string& filename);

  LOOT_API explicit operator std::string() const;
private:
  std::string filename_;
};

/**
 * Check if two Filename objects are equal by comparing their fields.
 * @returns True if the filenames are case-insensitively equal and all other
 *          fields are case-sensitively equal, false otherwise.
 */
LOOT_API bool operator==(const Filename& lhs,const Filename& rhs);

/**
 * Check if two Filename objects are not equal.
 * @returns True if the Filename objects are not equal, false otherwise.
 */
LOOT_API bool operator!=(const Filename& lhs, const Filename& rhs);

/**
 * A less-than operator implemented with no semantics so that Filename objects can
 * be stored in sets.
 * @returns True if this Filename is less than the given Filename, false otherwise.
 */
LOOT_API bool operator<(const Filename& lhs,const Filename& rhs);

/**
 * Check if the first Filename object is greater than the second Filename object.
 * @returns True if the second Filename object is less than the first Filename object,
 *          false otherwise.
 */
LOOT_API bool operator>(const Filename& lhs, const Filename& rhs);

/**
 * Check if the first Filename object is less than or equal to the second Filename
 * object.
 * @returns True if the first Filename object is not greater than the second Filename
 *          object, false otherwise.
 */
LOOT_API bool operator<=(const Filename& lhs, const Filename& rhs);

/**
 * Check if the first Filename object is greater than or equal to the second Filename
 * object.
 * @returns True if the first Filename object is not less than the second Filename
 *          object, false otherwise.
 */
LOOT_API bool operator>=(const Filename& lhs, const Filename& rhs);
}

#endif
