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
#ifndef LOOT_METADATA_TAG
#define LOOT_METADATA_TAG

#include <compare>
#include <string>
#include <string_view>

#include "loot/api_decorator.h"

namespace loot {
/**
 * Represents a Bash Tag suggestion for a plugin.
 */
class Tag {
public:
  /**
   * Construct a Tag object with an empty tag name suggested for addition, with
   * an empty condition string.
   */
  LOOT_API explicit Tag() = default;

  /**
   * Construct a Tag object with the given name, for addition or removal, with
   * the given condition string.
   * @param  tag
   *         The name of the Bash Tag.
   * @param  isAddition
   *         True if the tag should be added, false if it should be removed.
   * @param  condition
   *         A condition string.
   */
  LOOT_API explicit Tag(std::string_view tag,
                        const bool isAddition = true,
                        std::string_view condition = "");

  /**
   * Check if the tag should be added.
   * @return True if the tag should be added, false if it should be removed.
   */
  LOOT_API bool IsAddition() const;

  /**
   * Get the tag's name.
   * @return The tag's name.
   */
  LOOT_API std::string GetName() const;

  /**
   * Get the condition string.
   * @return The tag's condition string.
   */
  LOOT_API std::string GetCondition() const;

  /**
   * Check if two Tag objects are equal.
   * @returns True if the objects' fields are equal, false otherwise.
   */
  LOOT_API bool operator==(const Tag& rhs) const = default;

private:
  std::string name_;
  bool addTag_{true};
  std::string condition_;

  LOOT_API friend std::strong_ordering operator<=>(const Tag& lhs,
                                                   const Tag& rhs);
};

/**
 * Compares two Tag objects.
 *
 * Tag objects that suggest additions are considered less than those that
 * suggest removals.
 */
LOOT_API std::strong_ordering operator<=>(const Tag& lhs, const Tag& rhs);
}

#endif
