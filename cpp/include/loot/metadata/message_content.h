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
#ifndef LOOT_METADATA_MESSAGE_CONTENT
#define LOOT_METADATA_MESSAGE_CONTENT

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "loot/api_decorator.h"

namespace loot {
/**
 * Represents a message's localised text content.
 */
class MessageContent {
public:
  /**
   * The code for the default language assumed for message content, which is
   * "en" (English).
   */
  static constexpr std::string_view DEFAULT_LANGUAGE = "en";

  /**
   * Construct a MessageContent object with an empty English message string.
   */
  LOOT_API MessageContent() = default;

  /**
   * Construct a Message object with the given text in the given language.
   * @param  text
   *         The message text.
   * @param  language
   *         The language that the message is written in.
   */
  LOOT_API explicit MessageContent(
      std::string_view text,
      std::string_view language = DEFAULT_LANGUAGE);

  /**
   * Get the message text.
   * @return A string containing the message text.
   */
  LOOT_API std::string GetText() const;

  /**
   * Get the message language.
   * @return A code representing the language that the message is written in.
   */
  LOOT_API std::string GetLanguage() const;

private:
  std::string text_;
  std::string language_{DEFAULT_LANGUAGE};
};

/**
 * Check if two MessageContent objects are equal by comparing their fields.
 * @returns True if the objects' fields are equal, false otherwise.
 */
LOOT_API bool operator==(const MessageContent& lhs, const MessageContent& rhs);

/**
 * Check if two MessageContent objects are not equal.
 * @returns True if the MessageContent objects are not equal, false otherwise.
 */
LOOT_API bool operator!=(const MessageContent& lhs, const MessageContent& rhs);

/**
 * A less-than operator implemented with no semantics so that MessageContent
 * objects can be stored in sets.
 * @returns True if the first MessageContent is less than the second
 *          MessageContent, false otherwise.
 */
LOOT_API bool operator<(const MessageContent& lhs, const MessageContent& rhs);

/**
 * Check if the first MessageContent object is greater than the second
 * MessageContent object.
 * @returns True if the second MessageContent object is less than the first
 *          MessageContent object, false otherwise.
 */
LOOT_API bool operator>(const MessageContent& lhs, const MessageContent& rhs);

/**
 * Check if the first MessageContent object is less than or equal to the second
 * MessageContent object.
 * @returns True if the first MessageContent object is not greater than the
 *          second MessageContent object, false otherwise.
 */
LOOT_API bool operator<=(const MessageContent& lhs, const MessageContent& rhs);

/**
 * Check if the first MessageContent object is greater than or equal to the
 * second MessageContent object.
 * @returns True if the first MessageContent object is not less than the second
 *          MessageContent object, false otherwise.
 */
LOOT_API bool operator>=(const MessageContent& lhs, const MessageContent& rhs);

/**
 * Choose a MessageContent object from a vector given a language.
 * @param  content
 *         The MessageContent objects to choose between.
 * @param  language
 *         The preferred language to select. Values are expected to have the
 *         form `[language code]` or `[language code]_[country code]`, where
 *         `[language code]` is an ISO 639-1 language code and `[country code]`
 *         is an ISO 3166 country code.
 * @return A MessageContent object.
 *         * If the vector only contains a single element, that element is
 *           returned.
 *         * If content with a language that exactly matches the given language
 *           is present, that content is returned.
 *         * If the given language includes a country code and there is no exact
 *           match but content for the same language code is present, that
 *           content is returned.
 *         * If the given language does not include a country code and there is
 *           no exact match but content for thet same language code is present,
 *           that content is returned.
 *         * If no matches are found and content in the default language is
 *           present, that content is returned.
 *         * Otherwise, an empty optional is returned.
 */
LOOT_API std::optional<MessageContent> SelectMessageContent(
    const std::vector<MessageContent> content,
    std::string_view language);
}

#endif
