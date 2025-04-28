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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_YAML_GROUP_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_YAML_GROUP_TEST

#include <gtest/gtest.h>

#include "api/metadata/yaml/group.h"

namespace loot::test {
TEST(Group, emittingAsYamlShouldOmitAfterKeyIfAfterGroupsIsEmpty) {
  Group group;

  YAML::Emitter emitter;
  emitter << group;

  EXPECT_STREQ("name: 'default'", emitter.c_str());
}

TEST(Group, emittingAsYamlShouldIncludeDescriptionKeyIfDescriptionIsNotEmpty) {
  Group group("group1", {}, "test");

  YAML::Emitter emitter;
  emitter << group;

  EXPECT_STREQ(
      "name: 'group1'\n"
      "description: 'test'",
      emitter.c_str());
}

TEST(Group, emittingAsYamlShouldIncludeAfterKeyIfAfterGroupsIsNotEmpty) {
  Group group("group1", {"other_group"});

  YAML::Emitter emitter;
  emitter << group;

  EXPECT_STREQ(
      "name: 'group1'\n"
      "after:\n"
      "  - other_group",
      emitter.c_str());
}

TEST(Group, encodingAsYamlShouldOmitDescriptionKeyIfDescriptionIsEmpty) {
  Group group;
  YAML::Node node;
  node = group;

  EXPECT_EQ("default", node["name"].as<std::string>());
  EXPECT_FALSE(node["description"]);
}

TEST(Group, encodingAsYamlShouldIncludeDescriptionKeyIfDescriptionIsNotEmpty) {
  Group group("group1", {}, "test");
  YAML::Node node;
  node = group;

  EXPECT_EQ("group1", node["name"].as<std::string>());
  EXPECT_EQ("test", node["description"].as<std::string>());
}

TEST(Group, encodingAsYamlShouldOmitAfterKeyIfAfterGroupsIsEmpty) {
  Group group;
  YAML::Node node;
  node = group;

  EXPECT_EQ("default", node["name"].as<std::string>());
  EXPECT_FALSE(node["after"]);
}

TEST(Group, encodingAsYamlShouldIncludeAfterKeyIfAfterGroupsIsNotEmpty) {
  Group group("group1", {"other_group"});
  YAML::Node node;
  node = group;

  std::vector<std::string> expectedAfterGroups = {"other_group"};
  EXPECT_EQ("group1", node["name"].as<std::string>());
  EXPECT_EQ(expectedAfterGroups, node["after"].as<std::vector<std::string>>());
}

TEST(Group, decodingFromYamlShouldSetGivenName) {
  YAML::Node node = YAML::Load("{name: group1}");
  Group group = node.as<Group>();

  EXPECT_EQ("group1", group.GetName());
  EXPECT_TRUE(group.GetAfterGroups().empty());
}

TEST(Group, decodingFromYamlShouldSetDescriptionIfOneIsGiven) {
  YAML::Node node = YAML::Load("{name: group1, description: test}");
  Group group = node.as<Group>();

  EXPECT_EQ("group1", group.GetName());
  EXPECT_EQ("test", group.GetDescription());
}

TEST(Group, decodingFromYamlShouldSetAfterGroupsIfAnyAreGiven) {
  YAML::Node node = YAML::Load("{name: group1, after: [ other_group ]}");
  Group group = node.as<Group>();

  std::vector<std::string> expectedAfterGroups = {"other_group"};
  EXPECT_EQ("group1", group.GetName());
  EXPECT_EQ(expectedAfterGroups, group.GetAfterGroups());
}

TEST(Group, decodingFromYamlShouldThrowIfTheNameKeyIsMissing) {
  YAML::Node node = YAML::Load("{after: []}");

  EXPECT_THROW(node.as<Group>(), YAML::RepresentationException);
}

TEST(Group, decodingFromYamlShouldThrowIfAListIsGiven) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<Group>(), YAML::RepresentationException);
}
}

#endif
