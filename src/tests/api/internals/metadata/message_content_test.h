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

#include "loot/metadata/message_content.h"

#include <gtest/gtest.h>

#include "api/metadata/yaml/message_content.h"

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

TEST(MessageContent, equalityShouldRequireCaseSensitiveEqualityOnTextAndLanguage) {
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

TEST(MessageContent,
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
