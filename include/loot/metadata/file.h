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
#ifndef LOOT_METADATA_FILE
#define LOOT_METADATA_FILE

#include <string>

#include "loot/api_decorator.h"
#include "loot/metadata/conditional_metadata.h"
#include "loot/metadata/filename.h"
#include "loot/metadata/message_content.h"

namespace loot {
/**
 * Represents a file in a game's Data folder, including files in subdirectories.
 */
class File : public ConditionalMetadata {
public:
  /**
   * Construct a File with blank name, display and condition strings.
   * @return A File object.
   */
  LOOT_API explicit File();

  /**
   * Construct a File with the given name, display name and condition strings.
   * @param  name
   *         The filename of the file.
   * @param  display
   *         The name to be displayed for the file in messages, formatted using
   *         GitHub Flavored Markdown.
   * @param  condition
   *         The File's condition string.
   * @return A File object.
   */
  LOOT_API explicit File(const std::string& name,
                         const std::string& display = "",
                         const std::string& condition = "",
                         const std::vector<MessageContent>& detail = {});

  /**
   * A less-than operator implemented with no semantics so that File objects can
   * be stored in sets.
   * @returns True if this File is less than the given File, false otherwise.
   */
  LOOT_API bool operator<(const File& rhs) const;

  /**
   * Check if two File objects are equal by comparing their fields.
   * @returns True if the objects' fields are equal, false otherwise.
   */
  LOOT_API bool operator==(const File& rhs) const;

  /**
   * Get the filename of the file.
   * @return The file's filename.
   */
  LOOT_API Filename GetName() const;

  /**
   * Get the display name of the file.
   *
   * If the File was constructed with an empty display string, the name field
   * will be returned instead, with any `ASCII punctuation characters
   * <https://github.github.com/gfm/#ascii-punctuation-character>`_ escaped.
   * Escaping is not performed if returning the value of the display string.
   * @return The file's display name or filename.
   */
  LOOT_API std::string GetDisplayName() const;

  /**
   * Get the detail message content of the file.
   *
   * If this file causes an error message to be displayed, the detail message
   * content should be appended to that message, as it provides more detail
   * about the error (e.g. suggestions for how to resolve it).
   */
  LOOT_API std::vector<MessageContent> GetDetail() const;

  /**
   * Choose a detail MessageContent object given a preferred language.
   * @param  language
   *         The preferred language's code.
   * @return The MessageContent object for the preferred language, or if one
   *         does not exist, the English-language MessageContent object.
   */
  LOOT_API std::optional<MessageContent> ChooseDetail(
      const std::string& language) const;

private:
  Filename name_;
  std::string display_;
  std::vector<MessageContent> detail_;
};

/**
 * Check if two File objects are not equal.
 * @returns True if the File objects are not equal, false otherwise.
 */
LOOT_API bool operator!=(const File& lhs, const File& rhs);

/**
 * Check if the first File object is greater than the second File object.
 * @returns True if the second File object is less than the first File object,
 *          false otherwise.
 */
LOOT_API bool operator>(const File& lhs, const File& rhs);

/**
 * Check if the first File object is less than or equal to the second File
 * object.
 * @returns True if the first File object is not greater than the second File
 *          object, false otherwise.
 */
LOOT_API bool operator<=(const File& lhs, const File& rhs);

/**
 * Check if the first File object is greater than or equal to the second File
 * object.
 * @returns True if the first File object is not less than the second File
 *          object, false otherwise.
 */
LOOT_API bool operator>=(const File& lhs, const File& rhs);
}

#endif
