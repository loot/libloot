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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_MESSAGE_CONTENT_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_MESSAGE_CONTENT_TEST

#include <gtest/gtest.h>

#include "api/metadata/yaml/message_content.h"
#include "loot/metadata/message_content.h"

namespace loot {
namespace test {
const std::string french = "fr";

TEST(MessageContent, defaultConstructorShouldSetEmptyEnglishLanguageString) {
  MessageContent content;

  EXPECT_TRUE(content.GetText().empty());
  EXPECT_EQ(MessageContent::defaultLanguage, content.GetLanguage());
}

TEST(MessageContent, contentConstructorShouldStoreGivenStringAndLanguage) {
  MessageContent content("content", french);

  EXPECT_EQ("content", content.GetText());
  EXPECT_EQ(french, content.GetLanguage());
}

TEST(MessageContent,
     equalityShouldRequireCaseSensitiveEqualityOnTextAndLanguage) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_TRUE(content1 == content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_FALSE(content1 == content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_FALSE(content1 == content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_FALSE(content1 == content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_FALSE(content1 == content2);
}

TEST(MessageContent, inequalityShouldBeTheInverseOfEquality) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_FALSE(content1 != content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_TRUE(content1 != content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_TRUE(content1 != content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_TRUE(content1 != content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_TRUE(content1 != content2);
}

TEST(
    MessageContent,
    lessThanOperatorShouldUseCaseSensitiveLexicographicalComparisonForTextAndLanguage) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_FALSE(content1 < content2);
  EXPECT_FALSE(content2 < content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_FALSE(content1 < content2);
  EXPECT_TRUE(content2 < content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_TRUE(content2 < content1);
  EXPECT_FALSE(content1 < content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_TRUE(content1 < content2);
  EXPECT_FALSE(content2 < content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_TRUE(content2 < content1);
  EXPECT_FALSE(content1 < content2);
}

TEST(
    MessageContent,
    greaterThanOperatorShouldReturnTrueIfTheSecondMessageContentIsLessThanTheFirst) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_FALSE(content1 > content2);
  EXPECT_FALSE(content2 > content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_TRUE(content1 > content2);
  EXPECT_FALSE(content2 > content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_FALSE(content2 > content1);
  EXPECT_TRUE(content1 > content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_FALSE(content1 > content2);
  EXPECT_TRUE(content2 > content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_FALSE(content2 > content1);
  EXPECT_TRUE(content1 > content2);
}

TEST(
    MessageContent,
    lessThanOrEqualOperatorShouldReturnTrueIfTheFirstMessageContentIsNotGreaterThanTheSecond) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_TRUE(content1 <= content2);
  EXPECT_TRUE(content2 <= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_FALSE(content1 <= content2);
  EXPECT_TRUE(content2 <= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_TRUE(content2 <= content1);
  EXPECT_FALSE(content1 <= content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_TRUE(content1 <= content2);
  EXPECT_FALSE(content2 <= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_TRUE(content2 <= content1);
  EXPECT_FALSE(content1 <= content2);
}

TEST(
    MessageContent,
    greaterThanOrEqualToOperatorShouldReturnTrueIfTheFirstMessageContentIsNotLessThanTheSecond) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_TRUE(content1 >= content2);
  EXPECT_TRUE(content2 >= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_TRUE(content1 >= content2);
  EXPECT_FALSE(content2 >= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_FALSE(content2 >= content1);
  EXPECT_TRUE(content1 >= content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_FALSE(content1 >= content2);
  EXPECT_TRUE(content2 >= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_FALSE(content2 >= content1);
  EXPECT_TRUE(content1 >= content2);
}

TEST(MessageContent,
     chooseShouldReturnAnEmptyEnglishMessageIfTheVectorIsEmpty) {
  auto content = MessageContent::Choose(std::vector<MessageContent>(), "fr");

  EXPECT_EQ("en", content.GetLanguage());
  EXPECT_EQ("", content.GetText());
}

TEST(MessageContent, chooseShouldReturnTheOnlyElementOfASingleElementVector) {
  MessageContent content("test", "de");
  auto chosen = MessageContent::Choose({MessageContent("test", "de")}, "fr");

  EXPECT_EQ(content, chosen);
}

TEST(
    MessageContent,
    chooseShouldReturnAnEmptyEnglishMessageIfTheVectorHasNoEnglishOrMatchingLanguageContentWithTwoOrMoreElements) {
  auto contents = {MessageContent("test1", "de"),
                   MessageContent("test2", "fr")};
  auto content = MessageContent::Choose(contents, "pt");

  EXPECT_EQ("en", content.GetLanguage());
  EXPECT_EQ("", content.GetText());
}

TEST(MessageContent,
     chooseShouldReturnElementWithExactlyMatchingLocaleCodeIfPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt"),
                   MessageContent("test4", "pt_PT"),
                   MessageContent("test5", "pt_BR")};
  auto content = MessageContent::Choose(contents, "pt_BR");

  EXPECT_EQ("pt_BR", content.GetLanguage());
  EXPECT_EQ("test5", content.GetText());
}

TEST(
    MessageContent,
    chooseShouldReturnElementWithMatchingLanguageCodeIfExactlyMatchingLocaleCodeIsNotPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt_PT"),
                   MessageContent("test4", "pt")};
  auto content = MessageContent::Choose(contents, "pt_BR");

  EXPECT_EQ("pt", content.GetLanguage());
  EXPECT_EQ("test4", content.GetText());
}

TEST(
    MessageContent,
    chooseShouldReturnElementWithEnLanguageCodeIfNoMatchingLanguageCodeIsPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt_PT")};
  auto content = MessageContent::Choose(contents, "pt_BR");

  EXPECT_EQ("en", content.GetLanguage());
  EXPECT_EQ("test1", content.GetText());
}

TEST(
    MessageContent,
    chooseShouldReturnElementWithExactlyMatchingLanguageCodeIfLanguageCodeIsGiven) {
  auto contents = {
      MessageContent("test1", "en"),
      MessageContent("test2", "de"),
      MessageContent("test3", "pt_BR"),
      MessageContent("test4", "pt"),
  };
  auto content = MessageContent::Choose(contents, "pt");

  EXPECT_EQ("pt", content.GetLanguage());
  EXPECT_EQ("test4", content.GetText());
}

TEST(
    MessageContent,
    chooseShouldReturnFirstElementWithMatchingLanguageCodeIfLanguageCodeIsGivenAndNoExactMatchIsPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt_PT"),
                   MessageContent("test4", "pt_BR")};
  auto content = MessageContent::Choose(contents, "pt");

  EXPECT_EQ("pt_PT", content.GetLanguage());
  EXPECT_EQ("test3", content.GetText());
}

TEST(MessageContent, emittingAsYamlShouldOutputDataCorrectly) {
  MessageContent content("content", french);
  YAML::Emitter emitter;
  emitter << content;

  EXPECT_EQ("lang: " + french + "\ntext: '" + content.GetText() + "'",
            emitter.c_str());
}

TEST(MessageContent, encodingAsYamlShouldOutputDataCorrectly) {
  MessageContent content("content", french);
  YAML::Node node;
  node = content;

  EXPECT_EQ(content.GetText(), node["text"].as<std::string>());
  EXPECT_EQ(french, node["lang"].as<std::string>());
}

TEST(MessageContent, decodingFromYamlShouldSetDataCorrectly) {
  YAML::Node node = YAML::Load("{text: content, lang: fr}");
  MessageContent content = node.as<MessageContent>();

  EXPECT_EQ("content", content.GetText());
  EXPECT_EQ(french, content.GetLanguage());
}

TEST(MessageContent, decodingFromYamlScalarShouldThrow) {
  YAML::Node node = YAML::Load("scalar");

  EXPECT_THROW(node.as<MessageContent>(), YAML::RepresentationException);
}

TEST(MessageContent, decodingFromYamlListShouldThrow) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<MessageContent>(), YAML::RepresentationException);
}
}
}

#endif
