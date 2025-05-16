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
#include "loot/metadata/conditional_metadata.h"
#include "loot/metadata/message_content.h"

namespace loot {
/**
 * Represents a message with localisable text content.
 */
class Message : public ConditionalMetadata {
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

private:
  MessageType type_{MessageType::say};
  std::vector<MessageContent> content_;
};

/**
 * Check if two Message objects are equal by comparing their fields.
 * @returns True if the objects' fields are equal, false otherwise.
 */
LOOT_API bool operator==(const Message& lhs, const Message& rhs);

/**
 * Check if two Message objects are not equal.
 * @returns True if the Message objects are not equal, false otherwise.
 */
LOOT_API bool operator!=(const Message& lhs, const Message& rhs);

/**
 * A less-than operator implemented with no semantics so that Message objects
 * can be stored in sets.
 * @returns Returns true if the first Message is less than the second Message,
 *          and false otherwise.
 */
LOOT_API bool operator<(const Message& lhs, const Message& rhs);

/**
 * Check if the first Message object is greater than the second Message object.
 * @returns True if the second Message object is less than the first Message
 *          object, false otherwise.
 */
LOOT_API bool operator>(const Message& lhs, const Message& rhs);

/**
 * Check if the first Message object is less than or equal to the second
 * Message object.
 * @returns True if the first Message object is not greater than the second
 *          Message object, false otherwise.
 */
LOOT_API bool operator<=(const Message& lhs, const Message& rhs);

/**
 * Check if the first Message object is greater than or equal to the second
 * Message object.
 * @returns True if the first Message object is not less than the second
 *          Message object, false otherwise.
 */
LOOT_API bool operator>=(const Message& lhs, const Message& rhs);
}

#endif
