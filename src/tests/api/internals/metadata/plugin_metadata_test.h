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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_PLUGIN_METADATA_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_PLUGIN_METADATA_TEST

#include "api/metadata/yaml/plugin_metadata.h"
#include "loot/metadata/plugin_metadata.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginMetadataTest : public CommonGameTestFixture {
protected:
  PluginMetadataTest() :
      info_(std::vector<MessageContent>({
          MessageContent("info"),
      })) {}

  const std::vector<MessageContent> info_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         PluginMetadataTest,
                         ::testing::Values(GameType::tes5));

TEST_P(
    PluginMetadataTest,
    defaultConstructorShouldLeaveNameEmptyAndEnableMetadataAndLeaveGroupUnset) {
  PluginMetadata plugin;

  EXPECT_TRUE(plugin.GetName().empty());
  EXPECT_FALSE(plugin.GetGroup());
}

TEST_P(
    PluginMetadataTest,
    stringConstructorShouldSetNameToGivenStringAndEnableMetadataAndLeaveGroupUnset) {
  PluginMetadata plugin(blankEsm);

  EXPECT_EQ(blankEsm, plugin.GetName());
  EXPECT_FALSE(plugin.GetGroup());
}

TEST_P(PluginMetadataTest,
       nameMatchesShouldUseCaseInsensitiveNameComparisonForNonRegexNames) {
  PluginMetadata plugin(blankEsm);

  EXPECT_TRUE(plugin.NameMatches(boost::to_lower_copy(blankEsm)));
  EXPECT_FALSE(plugin.NameMatches(blankDifferentEsm));
}

TEST_P(PluginMetadataTest,
       nameMatchesShouldTreatGivenPluginNameStringsAsLiterals) {
  PluginMetadata plugin(blankEsm);
  std::string regex = "blan.\\.esm";

  EXPECT_FALSE(plugin.NameMatches(regex));
}

TEST_P(PluginMetadataTest,
       nameMatchesShouldUseCaseInsensitiveRegexMatchingForARegexName) {
  PluginMetadata plugin("Blan.\\.esm");

  EXPECT_TRUE(plugin.NameMatches(boost::to_lower_copy(blankEsm)));
  EXPECT_FALSE(plugin.NameMatches(blankDifferentEsm));
}

TEST_P(PluginMetadataTest, mergeMetadataShouldNotChangeName) {
  PluginMetadata plugin1(blankEsm);
  PluginMetadata plugin2(blankDifferentEsm);

  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(blankEsm, plugin1.GetName());
}

TEST_P(PluginMetadataTest,
       mergeMetadataShouldNotUseMergedGroupIfItAndCurrentGroupAreBothExplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin1.SetGroup("group1");
  plugin2.SetGroup("group2");
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ("group1", plugin1.GetGroup());
}

TEST_P(PluginMetadataTest,
       mergeMetadataShouldNotUseMergedGroupIfItAndCurrentGroupAreBothImplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin1.MergeMetadata(plugin2);

  EXPECT_FALSE(plugin1.GetGroup().has_value());
}

TEST_P(
    PluginMetadataTest,
    mergeMetadataShouldNotUseMergedGroupIfItIsImplicitAndCurrentGroupIsExplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin1.SetGroup("group1");
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ("group1", plugin1.GetGroup());
}

TEST_P(
    PluginMetadataTest,
    mergeMetadataShouldUseMergedGroupIfItIsExplicitAndCurrentGroupIsImplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin2.SetGroup("group2");
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ("group2", plugin1.GetGroup());
}

TEST_P(PluginMetadataTest, mergeMetadataShouldMergeLoadAfterData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  File file1(blankEsm);
  File file2(blankDifferentEsm);

  plugin1.SetLoadAfterFiles({file1});
  plugin2.SetLoadAfterFiles({file1, file2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<File>({file1, file2}), plugin1.GetLoadAfterFiles());
}

TEST_P(PluginMetadataTest, mergeMetadataShouldMergeRequirementData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  File file1(blankEsm);
  File file2(blankDifferentEsm);

  plugin1.SetRequirements({file1});
  plugin2.SetRequirements({file1, file2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<File>({file1, file2}), plugin1.GetRequirements());
}

TEST_P(PluginMetadataTest, mergeMetadataShouldMergeIncompatibilityData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  File file1(blankEsm);
  File file2(blankDifferentEsm);

  plugin1.SetIncompatibilities({file1});
  plugin2.SetIncompatibilities({file1, file2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<File>({file1, file2}), plugin1.GetIncompatibilities());
}

TEST_P(PluginMetadataTest, mergeMetadataShouldMergeMessages) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  Message message(MessageType::say, "content");

  plugin1.SetMessages({message});
  plugin2.SetMessages({message});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<Message>({message, message}), plugin1.GetMessages());
}

TEST_P(PluginMetadataTest, mergeMetadataShouldMergeTags) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  Tag tag1("Relev");
  Tag tag2("Relev", false);
  Tag tag3("Delev");

  plugin1.SetTags({tag1});
  plugin2.SetTags({tag1, tag2, tag3});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<Tag>({tag1, tag2, tag3}), plugin1.GetTags());
}

TEST_P(PluginMetadataTest, mergeMetadataShouldMergeDirtyInfoData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  PluginCleaningData info1(0x5, "utility", info_, 1, 2, 3);
  PluginCleaningData info2(0xA, "utility", info_, 1, 2, 3);

  plugin1.SetDirtyInfo({info1});
  plugin2.SetDirtyInfo({info1, info2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<PluginCleaningData>({info1, info2}),
            plugin1.GetDirtyInfo());
}
TEST_P(PluginMetadataTest, mergeMetadataShouldMergeCleanInfoData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  PluginCleaningData info1(0x5, "utility");
  PluginCleaningData info2(0xA, "utility");

  plugin1.SetCleanInfo({info1});
  plugin2.SetCleanInfo({info1, info2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<PluginCleaningData>({info1, info2}),
            plugin1.GetCleanInfo());
}

TEST_P(PluginMetadataTest, mergeMetadataShouldMergeLocationData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  Location location1("http://www.example.com/1");
  Location location2("http://www.example.com/2");

  plugin1.SetLocations({location1});
  plugin2.SetLocations({location1, location2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<Location>({location1, location2}),
            plugin1.GetLocations());
}

TEST_P(PluginMetadataTest, unsetGroupShouldLeaveNoGroupValueSet) {
  PluginMetadata plugin;
  EXPECT_FALSE(plugin.GetGroup().has_value());

  plugin.SetGroup("test");
  EXPECT_EQ("test", plugin.GetGroup().value());

  plugin.UnsetGroup();
  EXPECT_FALSE(plugin.GetGroup().has_value());
}

TEST_P(PluginMetadataTest,
       hasNameOnlyShouldBeTrueForADefaultConstructedPluginMetadataObject) {
  PluginMetadata plugin;

  EXPECT_TRUE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest,
       hasNameOnlyShouldBeTrueForAPluginMetadataObjectConstructedWithAName) {
  PluginMetadata plugin(blankEsp);

  EXPECT_TRUE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, hasNameOnlyShouldBeFalseIfTheGroupIsExplicit) {
  PluginMetadata plugin;
  plugin.SetGroup("group");

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, hasNameOnlyShouldBeFalseIfLoadAfterMetadataExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetLoadAfterFiles({File(blankEsm)});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest,
       hasNameOnlyShouldBeFalseIfRequirementMetadataExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetRequirements({File(blankEsm)});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest,
       hasNameOnlyShouldBeFalseIfIncompatibilityMetadataExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetIncompatibilities({File(blankEsm)});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, hasNameOnlyShouldBeFalseIfMessagesExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetMessages({Message(MessageType::say, "content")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, hasNameOnlyShouldBeFalseIfTagsExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetTags({Tag("Relev")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, hasNameOnlyShouldBeFalseIfDirtyInfoExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetDirtyInfo({PluginCleaningData(5, "utility", info_, 0, 1, 2)});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, hasNameOnlyShouldBeFalseIfCleanInfoExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetCleanInfo({PluginCleaningData(5, "utility")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, hasNameOnlyShouldBeFalseIfLocationsExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetLocations({Location("http://www.example.com")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST_P(PluginMetadataTest, isRegexPluginShouldBeFalseForAnEmptyPluginName) {
  PluginMetadata plugin;

  EXPECT_FALSE(plugin.IsRegexPlugin());
}

TEST_P(PluginMetadataTest, isRegexPluginShouldBeFalseForAnExactPluginFilename) {
  PluginMetadata plugin(blankEsm);

  EXPECT_FALSE(plugin.IsRegexPlugin());
}

TEST_P(PluginMetadataTest,
       isRegexPluginShouldBeTrueIfThePluginNameContainsAColon) {
  PluginMetadata plugin("Blank:.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST_P(PluginMetadataTest,
       isRegexPluginShouldBeTrueIfThePluginNameContainsABackslash) {
  PluginMetadata plugin("Blank\\.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST_P(PluginMetadataTest,
       isRegexPluginShouldBeTrueIfThePluginNameContainsAnAsterisk) {
  PluginMetadata plugin("Blank*.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST_P(PluginMetadataTest,
       isRegexPluginShouldBeTrueIfThePluginNameContainsAQuestionMark) {
  PluginMetadata plugin("Blank?.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST_P(PluginMetadataTest,
       isRegexPluginShouldBeTrueIfThePluginNameContainsAVerticalBar) {
  PluginMetadata plugin("Blank|.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST_P(PluginMetadataTest,
       asYamlShouldReturnAStringContainingTheMetadataEmittedAsYaml) {
  PluginMetadata plugin(blankEsm);
  plugin.SetLoadAfterFiles({File(blankEsm)});

  EXPECT_EQ(
      "name: 'Blank.esm'\n"
      "after: ['Blank.esm']",
      plugin.AsYaml());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithNoMetadataAsABlankString) {
  PluginMetadata plugin(blankEsm);
  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ("", emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginOmittingAnImplicitGroup) {
  PluginMetadata plugin(blankEsm);
  plugin.SetLoadAfterFiles({File(blankEsm)});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esm'\n"
      "after: ['Blank.esm']",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithAnExplicitGroup) {
  PluginMetadata plugin(blankEsm);
  plugin.SetGroup("group1");

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esm'\n"
      "group: 'group1'",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithLoadAfterMetadataCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetLoadAfterFiles({File(blankEsm)});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "after: ['Blank.esm']",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithRequirementsCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetRequirements({File(blankEsm)});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "req: ['Blank.esm']",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithIncompatibilitiesCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetIncompatibilities({File(blankEsm)});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "inc: ['Blank.esm']",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithMessagesCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetMessages({Message(MessageType::say, "content")});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "msg:\n"
      "  - type: say\n"
      "    content: 'content'",
      emitter.c_str());
}

TEST_P(PluginMetadataTest, emittingAsYamlShouldOutputAPluginWithTagsCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetTags({Tag("Relev")});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "tag: [Relev]",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithDirtyInfoCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetDirtyInfo({PluginCleaningData(5, "utility", info_, 0, 1, 2)});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "dirty:\n"
      "  - crc: 0x00000005\n"
      "    util: 'utility'\n"
      "    detail: 'info'\n"
      "    udr: 1\n"
      "    nav: 2",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithCleanInfoCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetCleanInfo({PluginCleaningData(5, "utility")});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "clean:\n"
      "  - crc: 0x00000005\n"
      "    util: 'utility'",
      emitter.c_str());
}

TEST_P(PluginMetadataTest,
       emittingAsYamlShouldOutputAPluginWithLocationsCorrectly) {
  PluginMetadata plugin(blankEsp);
  plugin.SetLocations({Location("http://www.example.com")});

  YAML::Emitter emitter;
  emitter << plugin;

  EXPECT_STREQ(
      "name: 'Blank.esp'\n"
      "url: ['http://www.example.com']",
      emitter.c_str());
}

TEST_P(PluginMetadataTest, encodingAsYamlShouldOmitAllUnsetFields) {
  PluginMetadata plugin(blankEsp);
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetName(), node["name"].as<std::string>());
  EXPECT_FALSE(node["after"]);
  EXPECT_FALSE(node["req"]);
  EXPECT_FALSE(node["inc"]);
  EXPECT_FALSE(node["msg"]);
  EXPECT_FALSE(node["tag"]);
  EXPECT_FALSE(node["dirty"]);
  EXPECT_FALSE(node["clean"]);
  EXPECT_FALSE(node["url"]);
}

TEST_P(PluginMetadataTest,
       encodingAsYamlShouldSetAfterFieldIfLoadAfterMetadataExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetLoadAfterFiles({File(blankEsm)});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetLoadAfterFiles(), node["after"].as<std::vector<File>>());
}

TEST_P(PluginMetadataTest, encodingAsYamlShouldSetReqFieldIfRequirementsExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetRequirements({File(blankEsm)});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetRequirements(), node["req"].as<std::vector<File>>());
}

TEST_P(PluginMetadataTest,
       encodingAsYamlShouldSetIncFieldIfIncompatibilitiesExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetIncompatibilities({File(blankEsm)});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetIncompatibilities(), node["inc"].as<std::vector<File>>());
}

TEST_P(PluginMetadataTest, encodingAsYamlShouldSetMsgFieldIfMessagesExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetMessages({Message(MessageType::say, "content")});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetMessages(), node["msg"].as<std::vector<Message>>());
}

TEST_P(PluginMetadataTest, encodingAsYamlShouldSetTagFieldIfTagsExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetTags({Tag("Relev")});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetTags(), node["tag"].as<std::vector<Tag>>());
}

TEST_P(PluginMetadataTest, encodingAsYamlShouldSetDirtyFieldIfDirtyInfoExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetDirtyInfo({PluginCleaningData(5, "utility", info_, 0, 1, 2)});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetDirtyInfo(),
            node["dirty"].as<std::vector<PluginCleaningData>>());
}

TEST_P(PluginMetadataTest, encodingAsYamlShouldSetCleanFieldIfCleanInfoExists) {
  PluginMetadata plugin(blankEsp);
  plugin.SetCleanInfo({PluginCleaningData(5, "utility")});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetCleanInfo(),
            node["clean"].as<std::vector<PluginCleaningData>>());
}

TEST_P(PluginMetadataTest, encodingAsYamlShouldSetUrlFieldIfLocationsExist) {
  PluginMetadata plugin(blankEsp);
  plugin.SetLocations({Location("http://www.example.com")});
  YAML::Node node;
  node = plugin;

  EXPECT_EQ(plugin.GetLocations(), node["url"].as<std::vector<Location>>());
}

TEST_P(PluginMetadataTest, decodingFromYamlShouldStoreAllGivenData) {
  YAML::Node node = YAML::Load(
      "name: 'Blank.esp'\n"
      "after:\n"
      "  - 'Blank.esm'\n"
      "req:\n"
      "  - 'Blank.esm'\n"
      "inc:\n"
      "  - 'Blank.esm'\n"
      "msg:\n"
      "  - type: say\n"
      "    content: 'content'\n"
      "tag:\n"
      "  - Relev\n"
      "dirty:\n"
      "  - crc: 0x5\n"
      "    util: 'utility'\n"
      "    udr: 1\n"
      "    nav: 2\n"
      "clean:\n"
      "  - crc: 0x6\n"
      "    util: 'utility'\n"
      "url:\n"
      "  - 'http://www.example.com'");
  PluginMetadata plugin = node.as<PluginMetadata>();

  EXPECT_EQ("Blank.esp", plugin.GetName());
  EXPECT_EQ(std::vector<File>({File("Blank.esm")}), plugin.GetLoadAfterFiles());
  EXPECT_EQ(std::vector<File>({File("Blank.esm")}), plugin.GetRequirements());
  EXPECT_EQ(std::vector<File>({File("Blank.esm")}),
            plugin.GetIncompatibilities());
  EXPECT_EQ(std::vector<Message>({Message(MessageType::say, "content")}),
            plugin.GetMessages());
  EXPECT_EQ(std::vector<Tag>({Tag("Relev")}), plugin.GetTags());
  EXPECT_EQ(std::vector<PluginCleaningData>(
                {PluginCleaningData(5, "utility", {}, 0, 1, 2)}),
            plugin.GetDirtyInfo());
  EXPECT_EQ(std::vector<PluginCleaningData>({PluginCleaningData(6, "utility")}),
            plugin.GetCleanInfo());
  EXPECT_EQ(std::vector<Location>({Location("http://www.example.com")}),
            plugin.GetLocations());
}

TEST_P(PluginMetadataTest,
       decodingFromYamlWithDirtyInfoInARegexPluginMetadataObjectShouldThrow) {
  YAML::Node node = YAML::Load(
      "name: 'Blank\\.esp'\n"
      "dirty:\n"
      "  - crc: 0x5\n"
      "    util: 'utility'\n"
      "    udr: 1\n"
      "    nav: 2");
  PluginMetadata plugin = node.as<PluginMetadata>();

  EXPECT_EQ("Blank\\.esp", plugin.GetName());
  EXPECT_EQ(std::vector<PluginCleaningData>(
                {PluginCleaningData(5, "utility", {}, 0, 1, 2)}),
            plugin.GetDirtyInfo());
}

TEST_P(PluginMetadataTest,
       decodingFromYamlWithCleanInfoInARegexPluginMetadataObjectShouldThrow) {
  YAML::Node node = YAML::Load(
      "name: 'Blank\\.esp'\n"
      "clean:\n"
      "  - crc: 0x5\n"
      "    util: 'utility'");
  PluginMetadata plugin = node.as<PluginMetadata>();

  EXPECT_EQ("Blank\\.esp", plugin.GetName());
  EXPECT_EQ(std::vector<PluginCleaningData>({PluginCleaningData(5, "utility")}),
            plugin.GetCleanInfo());
}

TEST_P(PluginMetadataTest, decodingFromYamlWithAnInvalidRegexNameShouldThrow) {
  YAML::Node node = YAML::Load(
      "name: 'RagnvaldBook(Farengar(+Ragnvald)?)?\\.esp'\n"
      "dirty:\n"
      "  - crc: 0x5\n"
      "    util: 'utility'\n"
      "    udr: 1\n"
      "    nav: 2");

  EXPECT_THROW(node.as<PluginMetadata>(), YAML::RepresentationException);
}

TEST_P(PluginMetadataTest, decodingFromAYamlScalarShouldThrow) {
  YAML::Node node = YAML::Load("scalar");

  EXPECT_THROW(node.as<PluginMetadata>(), YAML::RepresentationException);
}

TEST_P(PluginMetadataTest, decodingFromAYamlListShouldThrow) {
  YAML::Node node = YAML::Load("[0, 1, 2]");

  EXPECT_THROW(node.as<PluginMetadata>(), YAML::RepresentationException);
}
}
}

#endif
