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

#include "loot/metadata/message.h"

#include <boost/algorithm/string.hpp>

#include "api/game/game.h"

namespace loot {
Message::Message(const MessageType type,
                 const std::string& content,
                 const std::string& condition) :
    ConditionalMetadata(condition),
    type_(type),
    content_({MessageContent(content)}) {}

Message::Message(const MessageType type,
                 const std::vector<MessageContent>& content,
                 const std::string& condition) :
    ConditionalMetadata(condition), type_(type), content_(content) {
  if (content.size() > 1) {
    bool englishStringExists = false;
    for (const auto& mc : content) {
      if (mc.GetLanguage() == MessageContent::DEFAULT_LANGUAGE)
        englishStringExists = true;
    }
    if (!englishStringExists)
      throw std::invalid_argument(
          "bad conversion: multilingual messages must contain an English "
          "content string");
  }
}

Message::Message(const SimpleMessage& message) :
    ConditionalMetadata(message.condition),
    type_(message.type),
    content_({MessageContent(message.text, message.language)}) {}

MessageType Message::GetType() const { return type_; }

std::vector<MessageContent> Message::GetContent() const { return content_; }

bool operator==(const Message& lhs, const Message& rhs) {
  return lhs.GetType() == rhs.GetType() &&
         lhs.GetCondition() == rhs.GetCondition() &&
         lhs.GetContent() == rhs.GetContent();
}

bool operator!=(const Message& lhs, const Message& rhs) {
  return !(lhs == rhs);
}

bool operator<(const Message& lhs, const Message& rhs) {
  if (lhs.GetType() < rhs.GetType()) {
    return true;
  }

  if (rhs.GetType() < lhs.GetType()) {
    return false;
  }

  if (lhs.GetCondition() < rhs.GetCondition()) {
    return true;
  }

  if (rhs.GetCondition() < lhs.GetCondition()) {
    return false;
  }

  return lhs.GetContent() < rhs.GetContent();
}

bool operator>(const Message& lhs, const Message& rhs) { return rhs < lhs; }

bool operator<=(const Message& lhs, const Message& rhs) { return !(lhs > rhs); }

bool operator>=(const Message& lhs, const Message& rhs) { return !(lhs < rhs); }

std::optional<SimpleMessage> ToSimpleMessage(const Message& message,
                                             const std::string& language) {
  auto content = SelectMessageContent(message.GetContent(), language);
  if (!content.has_value()) {
    return std::nullopt;
  }

  SimpleMessage simpleMessage;

  simpleMessage.type = message.GetType();
  simpleMessage.language = content.value().GetLanguage();
  simpleMessage.text = content.value().GetText();
  simpleMessage.condition = message.GetCondition();

  return simpleMessage;
}

std::vector<SimpleMessage> ToSimpleMessages(
    const std::vector<Message>& messages,
    const std::string& language) {
  std::vector<SimpleMessage> simpleMessages;

  for (const auto& message : messages) {
    auto simpleMessage = ToSimpleMessage(message, language);
    if (simpleMessage.has_value()) {
      simpleMessages.push_back(simpleMessage.value());
    }
  }

  return simpleMessages;
}
}
