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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_YAML_FILE_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_YAML_FILE_TEST

#include <gtest/gtest.h>

#include "api/metadata/yaml/file.h"

namespace loot::test {
TEST(File, emittingAsYamlShouldSingleQuoteValues) {
  File file("name1",
            "display1",
            "condition1",
            {MessageContent("english", "en")},
            "constraint1");
  YAML::Emitter emitter;
  emitter << file;
  std::string expected = "name: '" + std::string(file.GetName()) +
                         "'\ncondition: '" + file.GetCondition() +
                         "'\ndisplay: '" + file.GetDisplayName() +
                         "'\nconstraint: '" + file.GetConstraint() +
                         "'\ndetail: '" + file.GetDetail()[0].GetText() + "'";

  EXPECT_EQ(expected, emitter.c_str());
}

TEST(File, emittingAsYamlShouldOutputAsAScalarIfOnlyTheNameStringIsNotEmpty) {
  File file("file.esp");
  YAML::Emitter emitter;
  emitter << file;

  EXPECT_EQ("'" + std::string(file.GetName()) + "'", emitter.c_str());
}

TEST(File, emittingAsYamlShouldOmitEmptyConditionAndConstraintStrings) {
  File file("name1", "display1");
  YAML::Emitter emitter;
  emitter << file;
  std::string expected = "name: '" + std::string(file.GetName()) +
                         "'\ndisplay: '" + file.GetDisplayName() + "'";

  EXPECT_EQ(expected, emitter.c_str());
}

TEST(
    File,
    emittingAsYamlShouldWriteDetailAsAListIfTheVectorContainsMoreThanOneElement) {
  File file("",
            "",
            "",
            {MessageContent("english", "en"), MessageContent("french", "fr")});
  YAML::Emitter emitter;
  emitter << file;
  std::string expected =
      "name: ''\n"
      "detail:\n"
      "  - lang: en\n"
      "    text: 'english'\n"
      "  - lang: fr\n"
      "    text: 'french'";

  EXPECT_EQ(expected, emitter.c_str());
}

TEST(File, encodingAsYamlShouldStoreDataCorrectly) {
  auto detail = {MessageContent("english", "en"),
                 MessageContent("french", "fr")};
  File file("name1", "display1", "condition1", detail, "constraint1");
  YAML::Node node;
  node = file;

  EXPECT_EQ(std::string(file.GetName()), node["name"].as<std::string>());
  EXPECT_EQ(file.GetDisplayName(), node["display"].as<std::string>());
  EXPECT_EQ(file.GetCondition(), node["condition"].as<std::string>());
  EXPECT_EQ(file.GetDetail(), node["detail"].as<std::vector<MessageContent>>());
  EXPECT_EQ(file.GetConstraint(), node["constraint"].as<std::string>());
}

TEST(File, encodingAsYamlShouldOmitEmptyFields) {
  File file("file.esp");
  YAML::Node node;
  node = file;

  EXPECT_EQ(std::string(file.GetName()), node["name"].as<std::string>());
  EXPECT_FALSE(node["display"]);
  EXPECT_FALSE(node["condition"]);
  EXPECT_FALSE(node["detail"]);
}

TEST(File, decodingFromYamlShouldSetDataCorrectly) {
  YAML::Node node = YAML::Load(
      "{name: name1, display: display1, condition: 'file(\"Foo.esp\")', "
      "detail: 'details', constraint: 'file(\"Bar.esp\")'}");
  File file = node.as<File>();

  std::vector<MessageContent> expectedDetail = {
      MessageContent("details", "en")};

  EXPECT_EQ(node["name"].as<std::string>(), std::string(file.GetName()));
  EXPECT_EQ(node["display"].as<std::string>(), file.GetDisplayName());
  EXPECT_EQ(node["condition"].as<std::string>(), file.GetCondition());
  EXPECT_EQ(expectedDetail, file.GetDetail());
  EXPECT_EQ(node["constraint"].as<std::string>(), file.GetConstraint());
}

TEST(File,
     decodingFromYamlWithMissingConditionFieldShouldLeaveConditionStringEmpty) {
  YAML::Node node = YAML::Load("{name: name1, display: display1}");
  File file = node.as<File>();

  EXPECT_EQ(node["name"].as<std::string>(), std::string(file.GetName()));
  EXPECT_EQ(node["display"].as<std::string>(), file.GetDisplayName());
  EXPECT_TRUE(file.GetCondition().empty());
  EXPECT_TRUE(file.GetDetail().empty());
  EXPECT_TRUE(file.GetConstraint().empty());
}

TEST(File, decodingFromYamlWithAListOfMessageContentDetailsShouldReadThemAll) {
  YAML::Node node = YAML::Load(
      "{name: name1, display: display1, condition: 'file(\"Foo.esp\")', "
      "detail: [{text: english, lang: en}, {text: french, lang: fr}]}");
  File file = node.as<File>();

  std::vector<MessageContent> expectedDetail = {MessageContent("english", "en"),
                                                MessageContent("french", "fr")};

  EXPECT_EQ(expectedDetail, file.GetDetail());
}

TEST(File, decodingFromYamlShouldNotThrowIfTheOnlyDetailStringIsNotEnglish) {
  YAML::Node node = YAML::Load(
      "name: name1\n"
      "detail:\n"
      "  - lang: fr\n"
      "    text: content1");

  EXPECT_NO_THROW(node.as<File>());
}

TEST(
    File,
    decodingFromYamlShouldThrowIfMultipleContentStringsAreGivenAndNoneAreEnglish) {
  YAML::Node node = YAML::Load(
      "name: name1\n"
      "detail:\n"
      "  - lang: de\n"
      "    text: content1\n"
      "  - lang: fr\n"
      "    text: content2");

  EXPECT_THROW(node.as<File>(), YAML::RepresentationException);
}

TEST(File, decodingFromYamlScalarShouldLeaveDisplayNameAndConditionEmpty) {
  YAML::Node node = YAML::Load("name1");
  File file = node.as<File>();

  EXPECT_EQ(node.as<std::string>(), std::string(file.GetName()));
  EXPECT_TRUE(file.GetDisplayName().empty());
  EXPECT_TRUE(file.GetCondition().empty());
  EXPECT_TRUE(file.GetDetail().empty());
  EXPECT_TRUE(file.GetConstraint().empty());
}

TEST(File, decodingFromYamlShouldThrowIfAnInvalidMapIsGiven) {
  YAML::Node node = YAML::Load("{name: name1, condition: invalid}");

  EXPECT_THROW(node.as<File>(), YAML::RepresentationException);
}

TEST(File, decodingFromYamlShouldThrowIfAListIsGiven) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<File>(), YAML::RepresentationException);
}
}

#endif
