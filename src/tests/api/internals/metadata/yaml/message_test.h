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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_YAML_MESSAGE_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_YAML_MESSAGE_TEST

#include "api/metadata/yaml/message.h"
#include "tests/common_game_test_fixture.h"

namespace loot::test {
class MessageTest : public CommonGameTestFixture {
protected:
  MessageTest() : CommonGameTestFixture(GameType::tes4) {}
  typedef std::vector<MessageContent> MessageContents;
};

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

#endif
