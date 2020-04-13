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

#include <string>

#include "loot/api_decorator.h"
#include "loot/metadata/conditional_metadata.h"

namespace loot {
/**
 * Represents a Bash Tag suggestion for a plugin.
 */
class Tag : public ConditionalMetadata {
public:
  /**
   * Construct a Tag object with an empty tag name suggested for addition, with
   * an empty condition string.
   * @return A Tag object.
   */
  LOOT_API explicit Tag();

  /**
   * Construct a Tag object with the given name, for addition or removal, with
   * the given condition string.
   * @param  tag
   *         The name of the Bash Tag.
   * @param  isAddition
   *         True if the tag should be added, false if it should be removed.
   * @param  condition
   *         A condition string.
   * @return A Tag object.
   */
  LOOT_API explicit Tag(const std::string& tag,
                        const bool isAddition = true,
                        const std::string& condition = "");

  /**
   * A less-than operator implemented with no semantics so that Tag objects
   * can be stored in sets.
   * @returns True if this Tag is less than the given Tag, false otherwise.
   */
  LOOT_API bool operator<(const Tag& rhs) const;

  /**
   * Check if two Tag objects are equal.
   * @returns True if the objects' fields are equal, false otherwise.
   */
  LOOT_API bool operator==(const Tag& rhs) const;

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

private:
  std::string name_;
  bool addTag_;
};

/**
 * Check if two Tag objects are not equal.
 * @returns True if the Tag objects are not equal, false otherwise.
 */
LOOT_API bool operator!=(const Tag& lhs, const Tag& rhs);

/**
 * Check if the first Tag object is greater than the second Tag object.
 * @returns True if the second Tag object is less than the first Tag object, 
 *          false otherwise.
 */
LOOT_API bool operator>(const Tag& lhs, const Tag& rhs);

/**
 * Check if the first Tag object is less than or equal to the second Tag 
 * object.
 * @returns True if the first Tag object is not greater than the second Tag 
 *          object, false otherwise.
 */
LOOT_API bool operator<=(const Tag& lhs, const Tag& rhs);

/**
 * Check if the first Tag object is greater than or equal to the second Tag
 * object.
 * @returns True if the first Tag object is not less than the second Tag 
 *          object, false otherwise.
 */
LOOT_API bool operator>=(const Tag& lhs, const Tag& rhs);
}

#endif
