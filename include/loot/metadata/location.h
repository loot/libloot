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
#ifndef LOOT_METADATA_LOCATION
#define LOOT_METADATA_LOCATION

#include <string>
#include <vector>

#include "loot/api_decorator.h"

namespace loot {
/**
 * Represents a URL at which the parent plugin can be found.
 */
class Location {
public:
  /**
   * Construct a Location with empty URL and name strings.
   * @return A Location object.
   */
  LOOT_API explicit Location();

  /**
   * Construct a Location with the given URL and name.
   * @param  url
   *         The URL at which the plugin can be found.
   * @param  name
   *         A name for the URL, eg. the page or site name.
   * @return A Location object.
   */
  LOOT_API explicit Location(const std::string& url, const std::string& name = "");

  /**
   * A less-than operator implemented with no semantics so that Location objects
   * can be stored in sets.
   * @returns True if this Location is less than the given Location, false 
   *          otherwise.
   */
  LOOT_API bool operator<(const Location& rhs) const;

  /**
   * Check if two Location objects are equal by comparing their fields.
   * @returns True if the objects' fields are equal, false otherwise.
   */
  LOOT_API bool operator==(const Location& rhs) const;

  /**
   * Get the object's URL.
   * @return A URL string.
   */
  LOOT_API std::string GetURL() const;

  /**
   * Get the object's name.
   * @return The name of the location.
   */
  LOOT_API std::string GetName() const;

private:
  std::string url_;
  std::string name_;
};

/**
 * Check if two Location objects are not equal.
 * @returns True if the Location objects are not equal, false otherwise.
 */
LOOT_API bool operator!=(const Location& lhs, const Location& rhs);

/**
 * Check if the first Location object is greater than the second Location 
 * object.
 * @returns True if the second Location object is less than the first Location
 *          object, false otherwise.
 */
LOOT_API bool operator>(const Location& lhs, const Location& rhs);

/**
 * Check if the first Location object is less than or equal to the second 
 * Location object.
 * @returns True if the first Location object is not greater than the second 
 *          Location object, false otherwise.
 */
LOOT_API bool operator<=(const Location& lhs, const Location& rhs);

/**
 * Check if the first Location object is greater than or equal to the second 
 * Location object.
 * @returns True if the first Location object is not less than the second 
 *          Location object, false otherwise.
 */
LOOT_API bool operator>=(const Location& lhs, const Location& rhs);
}

#endif
