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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_YAML_PLUGIN_CLEANING_DATA_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_YAML_PLUGIN_CLEANING_DATA_TEST

#include "api/metadata/yaml/plugin_cleaning_data.h"
#include "tests/common_game_test_fixture.h"

namespace loot::test {
class PluginCleaningDataTest : public CommonGameTestFixture {
protected:
  PluginCleaningDataTest() :
      CommonGameTestFixture(GameType::tes4),
      info_(std::vector<MessageContent>({
          MessageContent("info"),
      })) {}

  const std::vector<MessageContent> info_;
};

TEST_F(PluginCleaningDataTest, emittingAsYamlShouldOutputAllNonZeroCounts) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 2, 10, 30);
  YAML::Emitter emitter;
  emitter << info;

  EXPECT_STREQ(
      "crc: 0x12345678\nutil: 'cleaner'\ndetail: 'info'\nitm: 2\nudr: 10\nnav: "
      "30",
      emitter.c_str());
}

TEST_F(PluginCleaningDataTest, emittingAsYamlShouldOmitAllZeroCounts) {
  PluginCleaningData info(0x12345678, "cleaner", info_, 0, 0, 0);
  YAML::Emitter emitter;
  emitter << info;

  EXPECT_STREQ("crc: 0x12345678\nutil: 'cleaner'\ndetail: 'info'",
               emitter.c_str());
}

TEST_F(PluginCleaningDataTest, encodingAsYamlShouldOmitAllZeroCountFields) {
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

TEST_F(PluginCleaningDataTest,
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

TEST_F(PluginCleaningDataTest,
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

TEST_F(PluginCleaningDataTest, decodingFromYamlShouldStoreAllNonZeroCounts) {
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

TEST_F(PluginCleaningDataTest,
       decodingFromYamlShouldNotThrowIfTheOnlyDetailStringIsNotEnglish) {
  YAML::Node node = YAML::Load(
      "crc: 0x12345678\n"
      "util: cleaner\n"
      "detail:\n"
      "  - lang: fr\n"
      "    text: content1");

  EXPECT_NO_THROW(node.as<PluginCleaningData>());
}

TEST_F(
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

TEST_F(PluginCleaningDataTest, decodingFromYamlScalarShouldThrow) {
  YAML::Node node = YAML::Load("scalar");

  EXPECT_THROW(node.as<PluginCleaningData>(), YAML::RepresentationException);
}

TEST_F(PluginCleaningDataTest, decodingFromYamlListShouldThrow) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<PluginCleaningData>(), YAML::RepresentationException);
}
}

#endif
