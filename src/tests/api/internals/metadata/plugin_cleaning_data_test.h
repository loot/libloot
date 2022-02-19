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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_PLUGIN_CLEANING_DATA
#define LOOT_TESTS_API_INTERNALS_METADATA_PLUGIN_CLEANING_DATA

#include "api/game/game.h"
#include "api/metadata/yaml/plugin_cleaning_data.h"
#include "loot/metadata/plugin_cleaning_data.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginCleaningDataTest : public CommonGameTestFixture {
protected:
  PluginCleaningDataTest() :
      info_(std::vector<MessageContent>({
          MessageContent("info"),
      })) {}

  const std::vector<MessageContent> info_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         PluginCleaningDataTest,
                         ::testing::Values(GameType::tes4));

TEST_P(PluginCleaningDataTest,
       defaultConstructorShouldLeaveAllCountsAtZeroAndTheUtilityStringEmpty) {
  PluginCleaningData info;
  EXPECT_EQ(0u, info.GetCRC());
  EXPECT_EQ(0u, info.GetITMCount());
  EXPECT_EQ(0u, info.GetDeletedReferenceCount());
  EXPECT_EQ(0u, info.GetDeletedNavmeshCount());
  EXPECT_TRUE(info.GetCleaningUtility().empty());
  EXPECT_TRUE(info.GetDetail().empty());
}

TEST_P(PluginCleaningDataTest, contentConstructorShouldStoreAllGivenData) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_EQ(0x12345678u, info.GetCRC());
  EXPECT_EQ(2u, info.GetITMCount());
  EXPECT_EQ(10u, info.GetDeletedReferenceCount());
  EXPECT_EQ(30u, info.GetDeletedNavmeshCount());
  EXPECT_EQ("cleaner", info.GetCleaningUtility());
  EXPECT_EQ(info_, info.GetDetail());
}

TEST_P(PluginCleaningDataTest, equalityShouldCheckEqualityOfAllFields) {
  PluginCleaningData info1(0x12345678, "cleaner", info_, 2, 10, 30);
  PluginCleaningData info2(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info1 == info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x87654321, "cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info1 == info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "Cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info1 == info2);

  info1 = PluginCleaningData(0x12345678, "cleaner1", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner2", info_, 2, 10, 30);
  EXPECT_FALSE(info1 == info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(
      0x12345678, "cleaner", std::vector<MessageContent>(), 2, 10, 30);
  EXPECT_FALSE(info1 == info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 4, 10, 30);
  EXPECT_FALSE(info1 == info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 20, 30);
  EXPECT_FALSE(info1 == info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 60);
  EXPECT_FALSE(info1 == info2);
}

TEST_P(PluginCleaningDataTest, inequalityShouldBeTheInverseOfEquality) {
  PluginCleaningData info1(0x12345678, "cleaner", info_, 2, 10, 30);
  PluginCleaningData info2(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info1 != info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x87654321, "cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info1 != info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "Cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info1 != info2);

  info1 = PluginCleaningData(0x12345678, "cleaner1", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner2", info_, 2, 10, 30);
  EXPECT_TRUE(info1 != info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(
      0x12345678, "cleaner", std::vector<MessageContent>(), 2, 10, 30);
  EXPECT_TRUE(info1 != info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 4, 10, 30);
  EXPECT_TRUE(info1 != info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 20, 30);
  EXPECT_TRUE(info1 != info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 60);
  EXPECT_TRUE(info1 != info2);
}

TEST_P(PluginCleaningDataTest, lessThanOperatorShouldCompareAllFields) {
  PluginCleaningData info1(0x12345678, "cleaner", info_, 2, 10, 30);
  PluginCleaningData info2(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x87654321, "cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "Cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info2 < info1);
  EXPECT_FALSE(info1 < info2);

  info1 = PluginCleaningData(0x12345678, "cleaner1", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner2", info_, 2, 10, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(
      0x12345678, "cleaner", std::vector<MessageContent>(), 2, 10, 30);
  EXPECT_TRUE(info2 < info1);
  EXPECT_FALSE(info1 < info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 4, 10, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 20, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 60);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);
}

TEST_P(
    PluginCleaningDataTest,
    greaterThanOperatorShouldReturnTrueIfTheSecondPluginCleaningDataIsLessThanTheFirst) {
  PluginCleaningData info1(0x12345678, "cleaner", info_, 2, 10, 30);
  PluginCleaningData info2(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_FALSE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x87654321, "cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "Cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info2 > info1);
  EXPECT_TRUE(info1 > info2);

  info1 = PluginCleaningData(0x12345678, "cleaner1", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner2", info_, 2, 10, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(
      0x12345678, "cleaner", std::vector<MessageContent>(), 2, 10, 30);
  EXPECT_FALSE(info2 > info1);
  EXPECT_TRUE(info1 > info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 4, 10, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 20, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 60);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);
}

TEST_P(
    PluginCleaningDataTest,
    lessThanOrEqualOperatorShouldReturnTrueIfTheFirstPluginCleaningDataIsNotGreaterThanTheSecond) {
  PluginCleaningData info1(0x12345678, "cleaner", info_, 2, 10, 30);
  PluginCleaningData info2(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info1 <= info2);
  EXPECT_TRUE(info2 <= info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x87654321, "cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "Cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info2 < info1);
  EXPECT_FALSE(info1 < info2);

  info1 = PluginCleaningData(0x12345678, "cleaner1", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner2", info_, 2, 10, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(
      0x12345678, "cleaner", std::vector<MessageContent>(), 2, 10, 30);
  EXPECT_TRUE(info2 < info1);
  EXPECT_FALSE(info1 < info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 4, 10, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 20, 30);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 60);
  EXPECT_TRUE(info1 < info2);
  EXPECT_FALSE(info2 < info1);
}

TEST_P(
    PluginCleaningDataTest,
    greaterThanOrEqualToOperatorShouldReturnTrueIfTheFirstPluginCleaningDataIsNotLessThanTheSecond) {
  PluginCleaningData info1(0x12345678, "cleaner", info_, 2, 10, 30);
  PluginCleaningData info2(0x12345678, "cleaner", info_, 2, 10, 30);
  EXPECT_TRUE(info1 >= info2);
  EXPECT_TRUE(info2 >= info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x87654321, "cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "Cleaner", info_, 2, 10, 30);
  EXPECT_FALSE(info2 > info1);
  EXPECT_TRUE(info1 > info2);

  info1 = PluginCleaningData(0x12345678, "cleaner1", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner2", info_, 2, 10, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(
      0x12345678, "cleaner", std::vector<MessageContent>(), 2, 10, 30);
  EXPECT_FALSE(info2 > info1);
  EXPECT_TRUE(info1 > info2);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 4, 10, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 20, 30);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);

  info1 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 30);
  info2 = PluginCleaningData(0x12345678, "cleaner", info_, 2, 10, 60);
  EXPECT_FALSE(info1 > info2);
  EXPECT_TRUE(info2 > info1);
}

TEST_P(PluginCleaningDataTest,
       chooseDetailShouldCreateADefaultContentObjectIfNoneExists) {
  PluginCleaningData dirtyInfo(
      0xDEADBEEF, "cleaner", std::vector<MessageContent>(), 2, 10, 30);
  EXPECT_FALSE(
      dirtyInfo.ChooseDetail(MessageContent::DEFAULT_LANGUAGE).has_value());
}

TEST_P(PluginCleaningDataTest,
       chooseDetailShouldLeaveTheContentUnchangedIfOnlyOneStringExists) {
  PluginCleaningData dirtyInfo(0xDEADBEEF, "cleaner", info_, 2, 10, 30);

  EXPECT_EQ(info_[0], dirtyInfo.ChooseDetail(french).value());
  EXPECT_EQ(info_[0],
            dirtyInfo.ChooseDetail(MessageContent::DEFAULT_LANGUAGE).value());
}

TEST_P(
    PluginCleaningDataTest,
    chooseDetailShouldSelectTheEnglishStringIfNoStringExistsForTheGivenLanguage) {
  MessageContent content("content1", MessageContent::DEFAULT_LANGUAGE);
  std::vector<MessageContent> info({
      content,
      MessageContent("content1", german),
  });
  PluginCleaningData dirtyInfo(0xDEADBEEF, "cleaner", info, 2, 10, 30);

  EXPECT_EQ(content, dirtyInfo.ChooseDetail(french).value());
}

TEST_P(PluginCleaningDataTest,
       chooseDetailShouldSelectTheStringForTheGivenLanguageIfOneExists) {
  MessageContent frenchContent("content3", french);
  std::vector<MessageContent> info({
      MessageContent("content1", german),
      MessageContent("content2", MessageContent::DEFAULT_LANGUAGE),
      frenchContent,
  });
  PluginCleaningData dirtyInfo(0xDEADBEEF, "cleaner", info, 2, 10, 30);

  EXPECT_EQ(frenchContent, dirtyInfo.ChooseDetail(french).value());
}

TEST_P(PluginCleaningDataTest, emittingAsYamlShouldOutputAllNonZeroCounts) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 2, 10, 30);
  YAML::Emitter emitter;
  emitter << info;

  EXPECT_STREQ(
      "crc: 0x12345678\nutil: 'cleaner'\ndetail: 'info'\nitm: 2\nudr: 10\nnav: "
      "30",
      emitter.c_str());
}

TEST_P(PluginCleaningDataTest, emittingAsYamlShouldOmitAllZeroCounts) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 0, 0, 0);
  YAML::Emitter emitter;
  emitter << info;

  EXPECT_STREQ("crc: 0x12345678\nutil: 'cleaner'\ndetail: 'info'",
               emitter.c_str());
}

TEST_P(PluginCleaningDataTest, encodingAsYamlShouldOmitAllZeroCountFields) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 0, 0, 0);
  YAML::Node node;
  node = info;

  EXPECT_EQ(0x12345678u, node["crc"].as<uint32_t>());
  EXPECT_EQ("cleaner", node["util"].as<std::string>());
  EXPECT_EQ(info_, node["detail"].as<std::vector<MessageContent>>());
  EXPECT_FALSE(node["itm"]);
  EXPECT_FALSE(node["udr"]);
  EXPECT_FALSE(node["nav"]);
}

TEST_P(PluginCleaningDataTest,
       encodingAsYamlShouldOutputAllNonZeroCountFields) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 2, 10, 30);
  YAML::Node node;
  node = info;

  EXPECT_EQ(0x12345678u, node["crc"].as<uint32_t>());
  EXPECT_EQ("cleaner", node["util"].as<std::string>());
  EXPECT_EQ(info_, node["detail"].as<std::vector<MessageContent>>());
  EXPECT_EQ(2u, node["itm"].as<unsigned int>());
  EXPECT_EQ(10u, node["udr"].as<unsigned int>());
  EXPECT_EQ(30u, node["nav"].as<unsigned int>());
}

TEST_P(PluginCleaningDataTest,
       decodingFromYamlShouldLeaveMissingFieldsWithZeroValues) {
  YAML::Node node = YAML::Load("{crc: 0x12345678, util: cleaner}");
  PluginCleaningData info = node.as<PluginCleaningData>();

  EXPECT_EQ(0x12345678u, info.GetCRC());
  EXPECT_TRUE(info.GetDetail().empty());
  EXPECT_EQ(0u, info.GetITMCount());
  EXPECT_EQ(0u, info.GetDeletedReferenceCount());
  EXPECT_EQ(0u, info.GetDeletedNavmeshCount());
  EXPECT_EQ("cleaner", info.GetCleaningUtility());
}

TEST_P(PluginCleaningDataTest, decodingFromYamlShouldStoreAllNonZeroCounts) {
  YAML::Node node = YAML::Load(
      "{crc: 0x12345678, util: cleaner, detail: info, itm: 2, udr: 10, nav: "
      "30}");
  PluginCleaningData info = node.as<PluginCleaningData>();

  EXPECT_EQ(0x12345678u, info.GetCRC());
  EXPECT_EQ(info_, info.GetDetail());
  EXPECT_EQ(2u, info.GetITMCount());
  EXPECT_EQ(10u, info.GetDeletedReferenceCount());
  EXPECT_EQ(30u, info.GetDeletedNavmeshCount());
  EXPECT_EQ("cleaner", info.GetCleaningUtility());
}

TEST_P(PluginCleaningDataTest,
       decodingFromYamlShouldNotThrowIfTheOnlyDetailStringIsNotEnglish) {
  YAML::Node node = YAML::Load(
      "crc: 0x12345678\n"
      "util: cleaner\n"
      "detail:\n"
      "  - lang: fr\n"
      "    text: content1");

  EXPECT_NO_THROW(node.as<PluginCleaningData>());
}

TEST_P(
    PluginCleaningDataTest,
    decodingFromYamlShouldThrowIfMultipleDetailStringsAreGivenAndNoneAreEnglish) {
  YAML::Node node = YAML::Load(
      "crc: 0x12345678\n"
      "util: cleaner\n"
      "detail:\n"
      "  - lang: de\n"
      "    text: content1\n"
      "  - lang: fr\n"
      "    text: content2");

  EXPECT_THROW(node.as<PluginCleaningData>(), YAML::RepresentationException);
}

TEST_P(PluginCleaningDataTest, decodingFromYamlScalarShouldThrow) {
  YAML::Node node = YAML::Load("scalar");

  EXPECT_THROW(node.as<PluginCleaningData>(), YAML::RepresentationException);
}

TEST_P(PluginCleaningDataTest, decodingFromYamlListShouldThrow) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<PluginCleaningData>(), YAML::RepresentationException);
}
}
}

#endif
