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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_MESSAGE_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_MESSAGE_TEST

#include "api/game/game.h"
#include "api/metadata/yaml/message.h"
#include "loot/metadata/message.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
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

TEST_F(MessageTest, equalityShouldRequireEqualMessageTypes) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_TRUE(message1 == message2);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_FALSE(message1 == message2);
}

TEST_F(MessageTest, equalityShouldRequireCaseSensitiveEqualityOnCondition) {
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

TEST_F(MessageTest, equalityShouldRequireEqualContent) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_TRUE(message1 == message2);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_FALSE(message1 == message2);
}

TEST_F(MessageTest, inequalityShouldBeTheInverseOfEquality) {
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

TEST_F(MessageTest, lessThanOperatorShouldCompareMessageTypes) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_FALSE(message1 < message2);
  EXPECT_FALSE(message2 < message1);

  message1 = Message(MessageType::say, "content");
  message2 = Message(MessageType::warn, "content");

  EXPECT_TRUE(message1 < message2);
  EXPECT_FALSE(message2 < message1);
}

TEST_F(MessageTest, lessThanOperatorShouldCompareContent) {
  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content");

  EXPECT_FALSE(message1 < message2);
  EXPECT_FALSE(message2 < message1);

  message1 = Message(MessageType::say, "content1");
  message2 = Message(MessageType::say, "content2");

  EXPECT_TRUE(message1 < message2);
  EXPECT_FALSE(message2 < message1);
}

TEST_F(
    MessageTest,
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

TEST_F(
    MessageTest,
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

TEST_F(
    MessageTest,
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

TEST_F(
    MessageTest,
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

TEST_F(MessageTest, emittingAsYamlShouldOutputNoteMessageTypeCorrectly) {
  Message message(MessageType::say, "content1");
  YAML::Emitter emitter;
  emitter << message;

  EXPECT_STREQ(
      "type: say\n"
      "content: 'content1'",
      emitter.c_str());
}

TEST_F(MessageTest, emittingAsYamlShouldOutputWarnMessageTypeCorrectly) {
  Message message(MessageType::warn, "content1");
  YAML::Emitter emitter;
  emitter << message;

  EXPECT_STREQ(
      "type: warn\n"
      "content: 'content1'",
      emitter.c_str());
}

TEST_F(MessageTest, emittingAsYamlShouldOutputErrorMessageTypeCorrectly) {
  Message message(MessageType::error, "content1");
  YAML::Emitter emitter;
  emitter << message;

  EXPECT_STREQ(
      "type: error\n"
      "content: 'content1'",
      emitter.c_str());
}

TEST_F(MessageTest, emittingAsYamlShouldOutputConditionIfItIsNotEmpty) {
  Message message(MessageType::say, "content1", "condition1");
  YAML::Emitter emitter;
  emitter << message;

  EXPECT_STREQ(
      "type: say\n"
      "content: 'content1'\n"
      "condition: 'condition1'",
      emitter.c_str());
}

TEST_F(MessageTest, emittingAsYamlShouldOutputMultipleContentStringsAsAList) {
  Message message(MessageType::say,
                  MessageContents({MessageContent("content1"),
                                   MessageContent("content2", french)}));
  YAML::Emitter emitter;
  emitter << message;

  EXPECT_STREQ(
      "type: say\n"
      "content:\n"
      "  - lang: en\n"
      "    text: 'content1'\n"
      "  - lang: fr\n"
      "    text: 'content2'",
      emitter.c_str());
}

TEST_F(MessageTest, encodingAsYamlShouldStoreNoteMessageTypeCorrectly) {
  Message message(MessageType::say, "content1");
  YAML::Node node;
  node = message;

  EXPECT_EQ("say", node["type"].as<std::string>());
}

TEST_F(MessageTest, encodingAsYamlShouldStoreWarningMessageTypeCorrectly) {
  Message message(MessageType::warn, "content1");
  YAML::Node node;
  node = message;

  EXPECT_EQ("warn", node["type"].as<std::string>());
}

TEST_F(MessageTest, encodingAsYamlShouldStoreErrorMessageTypeCorrectly) {
  Message message(MessageType::error, "content1");
  YAML::Node node;
  node = message;

  EXPECT_EQ("error", node["type"].as<std::string>());
}

TEST_F(MessageTest, encodingAsYamlShouldOmitConditionFieldIfItIsEmpty) {
  Message message(MessageType::say, "content1");
  YAML::Node node;
  node = message;

  EXPECT_FALSE(node["condition"]);
}

TEST_F(MessageTest, encodingAsYamlShouldStoreConditionFieldIfItIsNotEmpty) {
  Message message(MessageType::say, "content1", "condition1");
  YAML::Node node;
  node = message;

  EXPECT_EQ("condition1", node["condition"].as<std::string>());
}

TEST_F(MessageTest, encodingAsYamlShouldStoreASingleContentStringInAVector) {
  Message message(MessageType::say, "content1");
  YAML::Node node;
  node = message;

  EXPECT_EQ(message.GetContent(), node["content"].as<MessageContents>());
}

TEST_F(MessageTest, encodingAsYamlShouldMultipleContentStringsInAVector) {
  MessageContents contents({
      MessageContent("content1"),
      MessageContent("content2", french),
  });
  Message message(MessageType::say, contents);
  YAML::Node node;
  node = message;

  EXPECT_EQ(contents, node["content"].as<MessageContents>());
}

TEST_F(MessageTest, decodingFromYamlShouldSetNoteTypeCorrectly) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: content1");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageType::say, message.GetType());
}

TEST_F(MessageTest, decodingFromYamlShouldSetWarningTypeCorrectly) {
  YAML::Node node = YAML::Load(
      "type: warn\n"
      "content: content1");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageType::warn, message.GetType());
}

TEST_F(MessageTest, decodingFromYamlShouldSetErrorTypeCorrectly) {
  YAML::Node node = YAML::Load(
      "type: error\n"
      "content: content1");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageType::error, message.GetType());
}

TEST_F(MessageTest, decodingFromYamlShouldHandleAnUnrecognisedTypeAsANote) {
  YAML::Node node = YAML::Load(
      "type: invalid\n"
      "content: content1");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageType::say, message.GetType());
}

TEST_F(MessageTest,
       decodingFromYamlShouldLeaveTheConditionEmptyIfNoneIsPresent) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: content1");
  Message message = node.as<Message>();

  EXPECT_TRUE(message.GetCondition().empty());
}

TEST_F(MessageTest, decodingFromYamlShouldStoreANonEmptyConditionField) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: content1\n"
      "condition: 'file(\"Foo.esp\")'");
  Message message = node.as<Message>();

  EXPECT_EQ("file(\"Foo.esp\")", message.GetCondition());
}

TEST_F(MessageTest, decodingFromYamlShouldStoreAScalarContentValueCorrectly) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: content1\n");
  Message message = node.as<Message>();
  MessageContents expectedContent({MessageContent("content1")});

  EXPECT_EQ(expectedContent, message.GetContent());
}

TEST_F(MessageTest, decodingFromYamlShouldStoreAListOfContentStringsCorrectly) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content:\n"
      "  - lang: en\n"
      "    text: content1\n"
      "  - lang: fr\n"
      "    text: content2");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageContents({
                MessageContent("content1"),
                MessageContent("content2", french),
            }),
            message.GetContent());
}

TEST_F(MessageTest,
       decodingFromYamlShouldNotThrowIfTheOnlyContentStringIsNotEnglish) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content:\n"
      "  - lang: fr\n"
      "    text: content1");

  EXPECT_NO_THROW(Message message = node.as<Message>());
}

TEST_F(
    MessageTest,
    decodingFromYamlShouldThrowIfMultipleContentStringsAreGivenAndNoneAreEnglish) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content:\n"
      "  - lang: de\n"
      "    text: content1\n"
      "  - lang: fr\n"
      "    text: content2");

  EXPECT_THROW(node.as<Message>(), YAML::RepresentationException);
}

TEST_F(
    MessageTest,
    decodingFromYamlShouldApplySubstitutionsWhenThereIsOnlyOneContentString) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: con{0}tent1\n"
      "subs:\n"
      "  - sub1");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageContents({MessageContent("consub1tent1")}),
            message.GetContent());
}

TEST_F(MessageTest,
       decodingFromYamlShouldApplySubstitutionsToAllContentStrings) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content:\n"
      "  - lang: en\n"
      "    text: content1 {0}\n"
      "  - lang: fr\n"
      "    text: content2 {0}\n"
      "subs:\n"
      "  - sub");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageContents({
                MessageContent("content1 sub"),
                MessageContent("content2 sub", french),
            }),
            message.GetContent());
}

TEST_F(
    MessageTest,
    decodingFromYamlShouldThrowIfTheContentStringExpectsMoreSubstitutionsThanExist) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: '{0} {1}'\n"
      "subs:\n"
      "  - sub1");

  EXPECT_THROW(node.as<Message>(), YAML::RepresentationException);
}

// Don't throw because no subs are given, so none are expected in the content
// string.
TEST_F(MessageTest,
       decodingFromYamlShouldIgnoreSubstitutionSyntaxIfNoSubstitutionsExist) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: con{0}tent1\n");
  Message message = node.as<Message>();

  EXPECT_EQ(MessageContents({MessageContent("con{0}tent1")}),
            message.GetContent());
}

TEST_F(MessageTest, decodingFromYamlShouldAcceptPercentagePlaceholderSyntax) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: content %1% %2% %3% %4% %5% %6% %7% %8% %9% %10% %11%\n"
      "subs:\n"
      "  - a\n"
      "  - b\n"
      "  - c\n"
      "  - d\n"
      "  - e\n"
      "  - f\n"
      "  - g\n"
      "  - h\n"
      "  - i\n"
      "  - j\n"
      "  - k");
  Message message = node.as<Message>();

  ASSERT_EQ(1, message.GetContent().size());
  EXPECT_EQ("content a b c d e f g h i j k", message.GetContent()[0].GetText());
}

TEST_F(MessageTest, decodingFromYamlShouldThrowIfAnInvalidConditionIsGiven) {
  YAML::Node node = YAML::Load(
      "type: say\n"
      "content: content1\n"
      "condition: invalid");

  EXPECT_THROW(node.as<Message>(), YAML::RepresentationException);
}

TEST_F(MessageTest, decodingFromYamlShouldThrowIfAScalarIsGiven) {
  YAML::Node node = YAML::Load("scalar");

  EXPECT_THROW(node.as<Message>(), YAML::RepresentationException);
}

TEST_F(MessageTest, decodingFromYamlShouldThrowIfAListIsGiven) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<Message>(), YAML::RepresentationException);
}
}
}

#endif
