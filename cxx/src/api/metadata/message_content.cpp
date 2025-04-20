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

#include "loot/metadata/message_content.h"

namespace loot {
MessageContent::MessageContent(std::string_view text,
                               std::string_view language) :
    text_(text), language_(language) {}

std::string MessageContent::GetText() const { return text_; }

std::string MessageContent::GetLanguage() const { return language_; }

bool operator==(const MessageContent& lhs, const MessageContent& rhs) {
  return lhs.GetText() == rhs.GetText() &&
         lhs.GetLanguage() == rhs.GetLanguage();
}

bool operator!=(const MessageContent& lhs, const MessageContent& rhs) {
  return !(lhs == rhs);
}

bool operator<(const MessageContent& lhs, const MessageContent& rhs) {
  if (lhs.GetText() < rhs.GetText()) {
    return true;
  }

  if (rhs.GetText() < lhs.GetText()) {
    return false;
  }

  return lhs.GetLanguage() < rhs.GetLanguage();
}

bool operator>(const MessageContent& lhs, const MessageContent& rhs) {
  return rhs < lhs;
}

bool operator<=(const MessageContent& lhs, const MessageContent& rhs) {
  return !(lhs > rhs);
}

bool operator>=(const MessageContent& lhs, const MessageContent& rhs) {
  return !(lhs < rhs);
}

std::optional<MessageContent> SelectMessageContent(
    const std::vector<MessageContent> content,
    std::string_view language) {
  if (content.empty())
    return std::nullopt;
  else if (content.size() == 1)
    return content.at(0);
  else {
    auto languageCode = language.substr(0, language.find("_"));
    const auto isCountryCodeGiven = languageCode.length() != language.length();

    std::optional<MessageContent> matchedLanguage;
    std::optional<MessageContent> english;
    for (const auto& mc : content) {
      auto contentLanguage = mc.GetLanguage();

      if (contentLanguage == language) {
        return mc;
      } else if (!matchedLanguage.has_value()) {
        if (isCountryCodeGiven && contentLanguage == languageCode) {
          matchedLanguage = mc;
        } else if (!isCountryCodeGiven) {
          const auto underscorePos = contentLanguage.find("_");
          if (underscorePos != std::string::npos) {
            auto contentLanguageCode = contentLanguage.substr(0, underscorePos);
            if (contentLanguageCode == language) {
              matchedLanguage = mc;
            }
          }
        }

        if (contentLanguage == MessageContent::DEFAULT_LANGUAGE) {
          english = mc;
        }
      }
    }

    if (matchedLanguage.has_value()) {
      return matchedLanguage;
    }

    if (english.has_value()) {
      return english;
    }

    return std::nullopt;
  }
}
}
