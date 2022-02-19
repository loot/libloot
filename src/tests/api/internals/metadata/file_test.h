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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_FILE_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_FILE_TEST

#include <gtest/gtest.h>

#include "api/metadata/yaml/file.h"
#include "loot/metadata/file.h"

namespace loot {
namespace test {
TEST(File, defaultConstructorShouldInitialiseEmptyStrings) {
  File file;

  EXPECT_EQ("", std::string(file.GetName()));
  EXPECT_EQ("", file.GetDisplayName());
  EXPECT_EQ("", file.GetCondition());
}

TEST(File, stringsConstructorShouldStoreGivenStrings) {
  std::vector<MessageContent> detail = {MessageContent("text", "en")};
  File file("name", "display", "condition", detail);

  EXPECT_EQ("name", std::string(file.GetName()));
  EXPECT_EQ("display", file.GetDisplayName());
  EXPECT_EQ("condition", file.GetCondition());
  EXPECT_EQ(detail, file.GetDetail());
}

TEST(File, equalityShouldBeCaseInsensitiveOnNameAndDisplay) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_TRUE(file1 == file2);

  file1 = File("name", "display", "condition");
  file2 = File("Name", "display", "condition");

  EXPECT_TRUE(file1 == file2);

  file1 = File("name1", "display", "condition");
  file2 = File("name2", "display", "condition");

  EXPECT_FALSE(file1 == file2);
}

TEST(File, equalityShouldBeCaseSensitiveOnDisplayAndCondition) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_TRUE(file1 == file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display1", "condition");
  file2 = File("name", "display2", "condition");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_FALSE(file1 == file2);
}

TEST(File, equalityShouldCompareTheDetailVectors) {
  File file1("", "", "", {MessageContent("text", "en")});
  File file2("", "", "", {MessageContent("text", "en")});

  EXPECT_TRUE(file1 == file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_FALSE(file1 == file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "En")});

  EXPECT_FALSE(file1 == file2);

  file1 = File(
      "", "", "", {MessageContent("text", "en"), MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 == file2);
}

TEST(File, inequalityShouldBeTheInverseOfEquality) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 != file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display1", "condition");
  file2 = File("name", "display2", "condition");

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_TRUE(file1 != file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_TRUE(file1 != file2);
}

TEST(File,
     lessThanOperatorShouldUseCaseInsensitiveLexicographicalComparisonForName) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_FALSE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name", "display", "condition");
  file2 = File("Name", "display", "condition");

  EXPECT_FALSE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name1");
  file2 = File("name2");

  EXPECT_TRUE(file1 < file2);
  EXPECT_FALSE(file2 < file1);
}

TEST(
    File,
    lessThanOperatorShouldUseCaseSensitiveLexicographicalComparisonForDisplayAndCondition) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_FALSE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_TRUE(file2 < file1);
  EXPECT_FALSE(file1 < file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_TRUE(file2 < file1);
  EXPECT_FALSE(file1 < file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_TRUE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_TRUE(file1 < file2);
  EXPECT_FALSE(file2 < file1);
}

TEST(File, lessThanOperatorShouldCompareTheDetailVectors) {
  File file1("", "", "", {MessageContent("text", "en")});
  File file2("", "", "", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 < file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_FALSE(file1 < file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "En")});

  EXPECT_FALSE(file1 < file2);

  file1 = File(
      "", "", "", {MessageContent("text", "en"), MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 < file2);
}

TEST(File, shouldAllowComparisonUsingGreaterThanOperator) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 > file2);
  EXPECT_FALSE(file2 > file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_FALSE(file2 > file1);
  EXPECT_TRUE(file1 > file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_FALSE(file2 > file1);
  EXPECT_TRUE(file1 > file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_FALSE(file1 > file2);
  EXPECT_TRUE(file2 > file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_FALSE(file1 > file2);
  EXPECT_TRUE(file2 > file1);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_TRUE(file1 > file2);
}

TEST(
    File,
    lessThanOrEqualToOperatorShouldReturnTrueIfFirstFileIsNotGreaterThanSecondFile) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_TRUE(file1 <= file2);
  EXPECT_TRUE(file2 <= file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_TRUE(file2 <= file1);
  EXPECT_FALSE(file1 <= file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_TRUE(file2 <= file1);
  EXPECT_FALSE(file1 <= file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_TRUE(file1 <= file2);
  EXPECT_FALSE(file2 <= file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_TRUE(file1 <= file2);
  EXPECT_FALSE(file2 <= file1);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_FALSE(file1 <= file2);
}

TEST(
    File,
    greaterThanOrEqualToOperatorShouldReturnTrueIfFirstFileIsNotLessThanSecondFile) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_TRUE(file1 >= file2);
  EXPECT_TRUE(file2 >= file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_FALSE(file2 >= file1);
  EXPECT_TRUE(file1 >= file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_FALSE(file2 >= file1);
  EXPECT_TRUE(file1 >= file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_FALSE(file1 >= file2);
  EXPECT_TRUE(file2 >= file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_FALSE(file1 >= file2);
  EXPECT_TRUE(file2 >= file1);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_TRUE(file1 >= file2);
}

TEST(File, getDisplayNameShouldReturnDisplayStringIfItIsNotEmpty) {
  File file("name", "display");

  EXPECT_EQ("display", file.GetDisplayName());
}

TEST(File, getDisplayNameShouldReturnNameStringIfDisplayStringIsEmpty) {
  File file("name", "");

  EXPECT_EQ("name", file.GetDisplayName());
}
TEST(File, getDisplayNameShouldNotEscapeASCIIPunctuationInDisplayString) {
  auto display = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
  File file("name", display);

  EXPECT_EQ(display, file.GetDisplayName());
}

TEST(File, getDisplayNameShouldEscapeASCIIPunctuationInNameString) {
  File file("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");

  EXPECT_EQ(
      R"raw(\!\"\#\$\%\&\'\(\)\*\+\,\-\.\/\:\;\<\=\>\?\@\[\\\]\^\_\`\{\|\}\~)raw",
      file.GetDisplayName());
}

TEST(File, emittingAsYamlShouldSingleQuoteValues) {
  File file(
      "name1", "display1", "condition1", {MessageContent("english", "en")});
  YAML::Emitter emitter;
  emitter << file;
  std::string expected = "name: '" + std::string(file.GetName()) +
                         "'\ncondition: '" + file.GetCondition() +
                         "'\ndisplay: '" + file.GetDisplayName() +
                         "'\ndetail: '" + file.GetDetail()[0].GetText() + "'";

  EXPECT_EQ(expected, emitter.c_str());
}

TEST(File, emittingAsYamlShouldOutputAsAScalarIfOnlyTheNameStringIsNotEmpty) {
  File file("file.esp");
  YAML::Emitter emitter;
  emitter << file;

  EXPECT_EQ("'" + std::string(file.GetName()) + "'", emitter.c_str());
}

TEST(
    File,
    emittingAsYamlShouldOmitDisplayFieldIfItMatchesTheNameFieldAfterEscapingASCIIPunctuation) {
  File file("file.esp", "file\\.esp");
  YAML::Emitter emitter;
  emitter << file;

  EXPECT_STREQ("'file.esp'", emitter.c_str());
}

TEST(File, emittingAsYamlShouldOmitAnEmptyConditionString) {
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
  File file("name1", "display1", "condition1", detail);
  YAML::Node node;
  node = file;

  EXPECT_EQ(std::string(file.GetName()), node["name"].as<std::string>());
  EXPECT_EQ(file.GetDisplayName(), node["display"].as<std::string>());
  EXPECT_EQ(file.GetCondition(), node["condition"].as<std::string>());
  EXPECT_EQ(file.GetDetail(), node["detail"].as<std::vector<MessageContent>>());
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

TEST(
    File,
    encodingAsYamlShouldOmitDisplayFieldIfItMatchesTheNameFieldAfterEscapingASCIIPunctuation) {
  File file("file.esp", "file\\.esp");
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
      "detail: 'details'}");
  File file = node.as<File>();

  std::vector<MessageContent> expectedDetail = {
      MessageContent("details", "en")};

  EXPECT_EQ(node["name"].as<std::string>(), std::string(file.GetName()));
  EXPECT_EQ(node["display"].as<std::string>(), file.GetDisplayName());
  EXPECT_EQ(node["condition"].as<std::string>(), file.GetCondition());
  EXPECT_EQ(expectedDetail, file.GetDetail());
}

TEST(File,
     decodingFromYamlWithMissingConditionFieldShouldLeaveConditionStringEmpty) {
  YAML::Node node = YAML::Load("{name: name1, display: display1}");
  File file = node.as<File>();

  EXPECT_EQ(node["name"].as<std::string>(), std::string(file.GetName()));
  EXPECT_EQ(node["display"].as<std::string>(), file.GetDisplayName());
  EXPECT_TRUE(file.GetCondition().empty());
  EXPECT_TRUE(file.GetDetail().empty());
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

TEST(
    File,
    decodingFromYamlScalarShouldUseNameValueForDisplayNameAndLeaveConditionEmpty) {
  YAML::Node node = YAML::Load("name1");
  File file = node.as<File>();

  EXPECT_EQ(node.as<std::string>(), std::string(file.GetName()));
  EXPECT_EQ(node.as<std::string>(), file.GetDisplayName());
  EXPECT_TRUE(file.GetCondition().empty());
  EXPECT_TRUE(file.GetDetail().empty());
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
}

#endif
