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
#ifndef LOOT_METADATA_MESSAGE
#define LOOT_METADATA_MESSAGE

#include <string>
#include <string_view>
#include <vector>

#include "loot/api_decorator.h"
#include "loot/enum/message_type.h"
#include "loot/metadata/message_content.h"

namespace loot {
/**
 * Represents a message with localisable text content.
 */
class Message {
public:
  /**
   * Construct a Message object of type 'say' with blank content and condition
   * strings.
   */
  LOOT_API Message() = default;

  /**
   * Construct a Message object with the given type, English content and
   * condition string.
   * @param  type
   *         The message type.
   * @param  content
   *         The English message content text.
   * @param  condition
   *         A condition string.
   */
  LOOT_API explicit Message(const MessageType type,
                            std::string_view content,
                            std::string_view condition = "");

  /**
   * Construct a Message object with the given type, content and condition
   * string.
   * @param  type
   *         The message type.
   * @param  content
   *         The message content. If multilingual, one language must be English.
   * @param  condition
   *         A condition string.
   */
  LOOT_API explicit Message(const MessageType type,
                            const std::vector<MessageContent>& content,
                            std::string_view condition = "");

  /**
   * Get the message type.
   * @return The message type.
   */
  LOOT_API MessageType GetType() const;

  /**
   * Get the message content.
   * @return The message's MessageContent objects.
   */
  LOOT_API std::vector<MessageContent> GetContent() const;

  /**
   * Get the condition string.
   * @return The message's condition string.
   */
  LOOT_API std::string GetCondition() const;

  /**
   * Compares two Message objects.
   */
  LOOT_API auto operator<=>(const Message& rhs) const = default;

private:
  MessageType type_{MessageType::say};
  std::vector<MessageContent> content_;
  std::string condition_;
};
}

#endif
