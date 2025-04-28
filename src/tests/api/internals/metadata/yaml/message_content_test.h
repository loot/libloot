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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_YAML_MESSAGE_CONTENT_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_YAML_MESSAGE_CONTENT_TEST

#include <gtest/gtest.h>

#include "api/metadata/yaml/message_content.h"

namespace loot::test {
const std::string french = "fr";

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

#endif
