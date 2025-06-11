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

#include <stdexcept>

namespace loot {
Message::Message(const MessageType type,
                 std::string_view content,
                 std::string_view condition) :
    type_(type), content_({MessageContent(content)}), condition_(condition) {}

Message::Message(const MessageType type,
                 const std::vector<MessageContent>& content,
                 std::string_view condition) :
    type_(type), content_(content), condition_(condition) {
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

MessageType Message::GetType() const { return type_; }

std::vector<MessageContent> Message::GetContent() const { return content_; }

std::string Message::GetCondition() const { return condition_; }
}
