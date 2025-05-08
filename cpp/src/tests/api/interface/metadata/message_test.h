/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2016    WrinklyNinja

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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_MESSAGE_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_MESSAGE_TEST

#include "loot/metadata/message.h"
#include "tests/common_game_test_fixture.h"

namespace loot::test {
class MessageTest : public CommonGameTestFixture {
protected:
  MessageTest() : CommonGameTestFixture(GameType::tes4) {}
  typedef std::vector<MessageContent> MessageContents;
};

TEST_F(MessageTest, defaultConstructorShouldCreateNoteWithNoContent) {
  Message message;
  EXPECT_EQ(MessageType::say, message.GetType());
  EXPECT_EQ(MessageContents(), message.GetContent());
}

TEST_F(MessageTest,
       scalarContentConstructorShouldCreateAMessageWithASingleContentString) {
  MessageContent content = MessageContent("content1");
  Message message(MessageType::warn, content.GetText(), "condition1");

  EXPECT_EQ(MessageType::warn, message.GetType());
  EXPECT_EQ(MessageContents({content}), message.GetContent());
  EXPECT_EQ("condition1", message.GetCondition());
}

TEST_F(MessageTest,
       vectorContentConstructorShouldCreateAMessageWithGivenContentStrings) {
  MessageContents contents({
      MessageContent("content1"),
      MessageContent("content2", french),
  });
  Message message(MessageType::error, contents, "condition1");

  EXPECT_EQ(MessageType::error, message.GetType());
  EXPECT_EQ(contents, message.GetContent());
  EXPECT_EQ("condition1", message.GetCondition());
}

TEST_F(
    MessageTest,
    vectorContentConstructorShouldThrowIfMultipleContentStringsAreGivenAndNoneAreEnglish) {
  MessageContents contents({
      MessageContent("content1", german),
      MessageContent("content2", french),
  });
  EXPECT_THROW(Message(MessageType::error, contents, "condition1"),
               std::invalid_argument);
}
}

#endif
