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
#include <string_view>

#include "loot/api_decorator.h"
#include "loot/metadata/filename.h"
#include "loot/metadata/message_content.h"

namespace loot {
/**
 * Represents a file in a game's Data folder, including files in subdirectories.
 */
class File {
public:
  /**
   * Construct a File with blank name, display and condition strings.
   */
  LOOT_API File() = default;

  /**
   * Construct a File with the given name, display name and condition strings.
   * @param  name
   *         The filename of the file.
   * @param  display
   *         The name to be displayed for the file in messages, formatted using
   *         CommonMark.
   * @param  condition
   *         The File's condition string.
   * @param  detail
   *         The detail message content, which may be appended to any messages
   *         generated for this file. If multilingual, one language must be
   *         English.
   * @param  constraint
   *         A condition string that must evaluate to true for the file's existence
   *         to be recognised.
   */
  LOOT_API explicit File(std::string_view name,
                         std::string_view display = "",
                         std::string_view condition = "",
                         const std::vector<MessageContent>& detail = {},
                         std::string_view constraint = "");

  /**
   * Get the filename of the file.
   * @return The file's filename.
   */
  LOOT_API Filename GetName() const;

  /**
   * Get the display name of the file.
   * @return The file's display name.
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
   * Get the condition string.
   * @return The file's condition string.
   */
  LOOT_API std::string GetCondition() const;

  /**
   * Get the constraint that applies to the file.
   * @return The file's constraint.
   */
  LOOT_API std::string GetConstraint() const;

  /**
   * Compares two File objects.
   */
  LOOT_API auto operator<=>(const File& rhs) const = default;

private:
  Filename name_;
  std::string display_;
  std::vector<MessageContent> detail_;
  std::string condition_;
  std::string constraint_;
};
}

#endif
