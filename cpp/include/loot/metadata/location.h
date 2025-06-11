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
#include <string_view>
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
   */
  LOOT_API Location() = default;

  /**
   * Construct a Location with the given URL and name.
   * @param  url
   *         The URL at which the plugin can be found.
   * @param  name
   *         A name for the URL, eg. the page or site name.
   */
  LOOT_API explicit Location(std::string_view url,
                             std::string_view name = "");

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

  /**
   * Compares two Location objects.
   */
  LOOT_API auto operator<=>(const Location& rhs) const = default;

private:
  std::string url_;
  std::string name_;
};
}

#endif
