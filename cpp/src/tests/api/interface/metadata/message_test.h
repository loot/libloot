/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2026 Oliver Hamlet

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

#include <gtest/gtest.h>

#include "loot/metadata/message.h"

namespace loot::test {
TEST(Message, defaultConstructorShouldCreateNoteWithNoContent) {
  Message message;
  EXPECT_EQ(MessageType::say, message.GetType());
  EXPECT_TRUE(message.GetContent().empty());
}

TEST(Message,
     scalarContentConstructorShouldCreateAMessageWithASingleContentString) {
  MessageContent content = MessageContent("content1");
  Message message(MessageType::warn, content.GetText(), "condition1");

  EXPECT_EQ(MessageType::warn, message.GetType());
  EXPECT_EQ(std::vector<MessageContent>({content}), message.GetContent());
  EXPECT_EQ("condition1", message.GetCondition());
}

TEST(Message,
     vectorContentConstructorShouldCreateAMessageWithGivenContentStrings) {
  std::vector<MessageContent> contents({
      MessageContent("content1"),
      MessageContent("content2", "fr"),
  });
  Message message(MessageType::error, contents, "condition1");

  EXPECT_EQ(MessageType::error, message.GetType());
  EXPECT_EQ(contents, message.GetContent());
  EXPECT_EQ("condition1", message.GetCondition());
}

TEST(
    Message,
    vectorContentConstructorShouldThrowIfMultipleContentStringsAreGivenAndNoneAreEnglish) {
  std::vector<MessageContent> contents({
      MessageContent("content1", "de"),
      MessageContent("content2", "fr"),
  });
  EXPECT_THROW(Message(MessageType::error, contents, "condition1"),
               std::invalid_argument);
}

TEST(Message, equalityShouldRequireEqualMessageTypes) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_TRUE(message1 == message2);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_FALSE(message1 == message2);
}

TEST(Message, equalityShouldRequireCaseSensitiveEqualityOnCondition) {
  Message message1(MessageType::say, "content", "condition");
  Message message2(MessageType::say, "content", "condition");

  EXPECT_TRUE(message1 == message2);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "Condition");

  EXPECT_FALSE(message1 == message2);

  message1 = Message(MessageType::say, "content", "condition1");
  message2 = Message(MessageType::say, "content", "condition2");

  EXPECT_FALSE(message1 == message2);
}

TEST(Message, equalityShouldRequireEqualContent) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_TRUE(message1 == message2);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_FALSE(message1 == message2);
}

TEST(Message, inequalityShouldBeTheInverseOfEquality) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_FALSE(message1 != message2);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_TRUE(message1 != message2);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "condition");

  EXPECT_FALSE(message1 != message2);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "Condition");

  EXPECT_TRUE(message1 != message2);

  message1 = Message(MessageType::say, "content", "condition1");
  message2 = Message(MessageType::say, "content", "condition2");

  EXPECT_TRUE(message1 != message2);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::say, "content");

  EXPECT_FALSE(message1 != message2);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_TRUE(message1 != message2);
}

TEST(Message, lessThanOperatorShouldCompareMessageTypes) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_FALSE(message1 < message2);
  EXPECT_FALSE(message2 < message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_TRUE(message1 < message2);
  EXPECT_FALSE(message2 < message1);
}

TEST(Message, lessThanOperatorShouldCompareContent) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_FALSE(message1 < message2);
  EXPECT_FALSE(message2 < message1);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_TRUE(message1 < message2);
  EXPECT_FALSE(message2 < message1);
}

TEST(
    Message,
    lessThanOperatorShouldUseCaseSensitiveLexicographicalComparisonForConditions) {
  Message message1(MessageType::say, "content", "condition");
  Message message2(MessageType::say, "content", "condition");

  EXPECT_FALSE(message1 < message2);
  EXPECT_FALSE(message2 < message1);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "Condition");

  EXPECT_TRUE(message2 < message1);
  EXPECT_FALSE(message1 < message2);

  message1 = Message(MessageType::say, "content", "condition1");
  message2 = Message(MessageType::say, "content", "condition2");

  EXPECT_TRUE(message1 < message2);
  EXPECT_FALSE(message2 < message1);
}

TEST(Message,
     greaterThanOperatorShouldReturnTrueIfTheSecondMessageIsLessThanTheFirst) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_FALSE(message1 > message2);
  EXPECT_FALSE(message2 > message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_FALSE(message1 > message2);
  EXPECT_TRUE(message2 > message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::say, "content");

  EXPECT_FALSE(message1 > message2);
  EXPECT_FALSE(message2 > message1);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_FALSE(message1 > message2);
  EXPECT_TRUE(message2 > message1);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "condition");

  EXPECT_FALSE(message1 > message2);
  EXPECT_FALSE(message2 > message1);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "Condition");

  EXPECT_FALSE(message2 > message1);
  EXPECT_TRUE(message1 > message2);

  message1 = Message(MessageType::say, "content", "condition1");
  message2 = Message(MessageType::say, "content", "condition2");

  EXPECT_FALSE(message1 > message2);
  EXPECT_TRUE(message2 > message1);
}

TEST(
    Message,
    lessThanOrEqualOperatorShouldReturnTrueIfTheFirstMessageIsNotGreaterThanTheSecond) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_TRUE(message1 <= message2);
  EXPECT_TRUE(message2 <= message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_TRUE(message1 <= message2);
  EXPECT_FALSE(message2 <= message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::say, "content");

  EXPECT_TRUE(message1 <= message2);
  EXPECT_TRUE(message2 <= message1);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_TRUE(message1 <= message2);
  EXPECT_FALSE(message2 <= message1);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "condition");

  EXPECT_TRUE(message1 <= message2);
  EXPECT_TRUE(message2 <= message1);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "Condition");

  EXPECT_TRUE(message2 <= message1);
  EXPECT_FALSE(message1 <= message2);

  message1 = Message(MessageType::say, "content", "condition1");
  message2 = Message(MessageType::say, "content", "condition2");

  EXPECT_TRUE(message1 <= message2);
  EXPECT_FALSE(message2 <= message1);
}

TEST(
    Message,
    greaterThanOrEqualToOperatorShouldReturnTrueIfTheFirstMessageIsNotLessThanTheSecond) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_TRUE(message1 >= message2);
  EXPECT_TRUE(message2 >= message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_FALSE(message1 >= message2);
  EXPECT_TRUE(message2 >= message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::say, "content");

  EXPECT_TRUE(message1 >= message2);
  EXPECT_TRUE(message2 >= message1);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_FALSE(message1 >= message2);
  EXPECT_TRUE(message2 >= message1);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "condition");

  EXPECT_TRUE(message1 >= message2);
  EXPECT_TRUE(message2 >= message1);

  message1 = Message(MessageType::say, "content", "condition");
  message2 = Message(MessageType::say, "content", "Condition");

  EXPECT_FALSE(message2 >= message1);
  EXPECT_TRUE(message1 >= message2);

  message1 = Message(MessageType::say, "content", "condition1");
  message2 = Message(MessageType::say, "content", "condition2");

  EXPECT_FALSE(message1 >= message2);
  EXPECT_TRUE(message2 >= message1);
}
}

#endif
