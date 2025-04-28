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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_YAML_TAG_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_YAML_TAG_TEST

#include <gtest/gtest.h>

#include "api/metadata/yaml/tag.h"

namespace loot::test {
TEST(
    Tag,
    emittingAsYamlShouldOutputOnlyTheNameStringIfTheTagIsAnAdditionWithNoCondition) {
  Tag tag("name1");
  YAML::Emitter emitter;
  emitter << tag;

  EXPECT_EQ(tag.GetName(), emitter.c_str());
}

TEST(
    Tag,
    emittingAsYamlShouldOutputOnlyTheNameStringPrefixedWithAHyphenIfTheTagIsARemovalWithNoCondition) {
  Tag tag("name1", false);
  YAML::Emitter emitter;
  emitter << tag;

  EXPECT_EQ("-" + tag.GetName(), emitter.c_str());
}

TEST(Tag, emittingAsYamlShouldOutputAMapIfTheTagHasACondition) {
  Tag tag("name1", false, "condition1");
  YAML::Emitter emitter;
  emitter << tag;

  EXPECT_STREQ("name: -name1\ncondition: 'condition1'", emitter.c_str());
}

TEST(Tag,
     encodingAsYamlShouldOmitTheConditionFieldIfTheConditionStringIsEmpty) {
  Tag tag;
  YAML::Node node;
  node = tag;

  EXPECT_FALSE(node["condition"]);
}

TEST(Tag, encodingAsYamlShouldOutputTheNameFieldCorrectly) {
  Tag tag("name1");
  YAML::Node node;
  node = tag;

  EXPECT_EQ(tag.GetName(), node["name"].as<std::string>());
}

TEST(
    Tag,
    encodingAsYamlShouldOutputTheNameFieldWithAHyphenPrefixIfTheTagIsARemoval) {
  Tag tag("name1", false);
  YAML::Node node;
  node = tag;

  EXPECT_EQ("-" + tag.GetName(), node["name"].as<std::string>());
}

TEST(
    Tag,
    encodingAsYamlShouldOutputTheConditionFieldIfTheConditionStringIsNotEmpty) {
  Tag tag("name1", true, "condition1");
  YAML::Node node;
  node = tag;

  EXPECT_EQ(tag.GetName(), node["name"].as<std::string>());
  EXPECT_EQ(tag.GetCondition(), node["condition"].as<std::string>());
}

TEST(Tag, decodingFromYamlScalarShouldSetNameCorrectly) {
  YAML::Node node = YAML::Load("name1");
  Tag tag = node.as<Tag>();

  EXPECT_EQ("name1", tag.GetName());
  EXPECT_TRUE(tag.IsAddition());
  EXPECT_EQ("", tag.GetCondition());
}

TEST(Tag, decodingFromYamlScalarShouldSetAdditionStateCorrectly) {
  YAML::Node node = YAML::Load("-name1");
  Tag tag = node.as<Tag>();

  EXPECT_EQ("name1", tag.GetName());
  EXPECT_FALSE(tag.IsAddition());
  EXPECT_EQ("", tag.GetCondition());
}

TEST(Tag, decodingFromYamlMapShouldSetDataCorrectly) {
  YAML::Node node = YAML::Load("{name: name1, condition: 'file(\"Foo.esp\")'}");
  Tag tag = node.as<Tag>();

  EXPECT_EQ("name1", tag.GetName());
  EXPECT_TRUE(tag.IsAddition());
  EXPECT_EQ("file(\"Foo.esp\")", tag.GetCondition());
}

TEST(Tag, decodingFromYamlShouldThrowIfAnInvalidConditionIsGiven) {
  YAML::Node node = YAML::Load("{name: name1, condition: invalid}");

  EXPECT_THROW(node.as<Tag>(), YAML::RepresentationException);
}

TEST(Tag, decodingFromYamlListShouldThrow) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<Tag>(), YAML::RepresentationException);
}
}

#endif
