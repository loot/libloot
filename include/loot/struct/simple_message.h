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
#ifndef LOOT_SIMPLE_MESSAGE
#define LOOT_SIMPLE_MESSAGE

#include <string>

#include "loot/enum/message_type.h"

namespace loot {
/**
 * A structure that holds the type of a message and the message string itself.
 */
struct SimpleMessage {
  /** The type of the message. */
  MessageType type{MessageType::say};

  /** The language the message string is written in. */
  std::string language;

  /** The message string, which may be formatted using CommonMark. */
  std::string text;

  /** The message's condition string. */
  std::string condition;
};
}

#endif
