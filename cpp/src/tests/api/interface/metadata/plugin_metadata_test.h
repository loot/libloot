/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2026 Oliver Hamlet

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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_PLUGIN_METADATA_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_PLUGIN_METADATA_TEST

#include "loot/metadata/plugin_metadata.h"
#include "tests/common_game_test_fixture.h"

namespace loot::test {
TEST(
    PluginMetadata,
    defaultConstructorShouldLeaveNameEmptyAndEnableMetadataAndLeaveGroupUnset) {
  PluginMetadata plugin;

  EXPECT_TRUE(plugin.GetName().empty());
  EXPECT_FALSE(plugin.GetGroup());
}

TEST(
    PluginMetadata,
    stringConstructorShouldSetNameToGivenStringAndEnableMetadataAndLeaveGroupUnset) {
  PluginMetadata plugin(BLANK_ESM);

  EXPECT_EQ(BLANK_ESM, plugin.GetName());
  EXPECT_FALSE(plugin.GetGroup());
}

TEST(PluginMetadata,
     nameMatchesShouldUseCaseInsensitiveNameComparisonForNonRegexNames) {
  PluginMetadata plugin(BLANK_ESM);

  EXPECT_TRUE(plugin.NameMatches("blank.esm"));
  EXPECT_FALSE(plugin.NameMatches(BLANK_DIFFERENT_ESM));
}

TEST(PluginMetadata, nameMatchesShouldTreatGivenPluginNameStringsAsLiterals) {
  PluginMetadata plugin(BLANK_ESM);
  std::string regex = "blan.\\.esm";

  EXPECT_FALSE(plugin.NameMatches(regex));
}

TEST(PluginMetadata,
     nameMatchesShouldUseCaseInsensitiveRegexMatchingForARegexName) {
  PluginMetadata plugin("Blan.\\.esm");

  EXPECT_TRUE(plugin.NameMatches("blank.esm"));
  EXPECT_FALSE(plugin.NameMatches(BLANK_DIFFERENT_ESM));
}

TEST(PluginMetadata, mergeMetadataShouldNotChangeName) {
  PluginMetadata plugin1(BLANK_ESM);
  PluginMetadata plugin2(BLANK_DIFFERENT_ESM);

  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(BLANK_ESM, plugin1.GetName());
}

TEST(PluginMetadata,
     mergeMetadataShouldNotUseMergedGroupIfItAndCurrentGroupAreBothExplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin1.SetGroup("group1");
  plugin2.SetGroup("group2");
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ("group1", plugin1.GetGroup());
}

TEST(PluginMetadata,
     mergeMetadataShouldNotUseMergedGroupIfItAndCurrentGroupAreBothImplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin1.MergeMetadata(plugin2);

  EXPECT_FALSE(plugin1.GetGroup().has_value());
}

TEST(
    PluginMetadata,
    mergeMetadataShouldNotUseMergedGroupIfItIsImplicitAndCurrentGroupIsExplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin1.SetGroup("group1");
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ("group1", plugin1.GetGroup());
}

TEST(PluginMetadata,
     mergeMetadataShouldUseMergedGroupIfItIsExplicitAndCurrentGroupIsImplicit) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;

  plugin2.SetGroup("group2");
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ("group2", plugin1.GetGroup());
}

TEST(PluginMetadata, mergeMetadataShouldMergeLoadAfterData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  File file1(BLANK_ESM);
  File file2(BLANK_DIFFERENT_ESM);

  plugin1.SetLoadAfterFiles({file1});
  plugin2.SetLoadAfterFiles({file1, file2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<File>({file1, file2}), plugin1.GetLoadAfterFiles());
}

TEST(PluginMetadata, mergeMetadataShouldMergeRequirementData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  File file1(BLANK_ESM);
  File file2(BLANK_DIFFERENT_ESM);

  plugin1.SetRequirements({file1});
  plugin2.SetRequirements({file1, file2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<File>({file1, file2}), plugin1.GetRequirements());
}

TEST(PluginMetadata, mergeMetadataShouldMergeIncompatibilityData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  File file1(BLANK_ESM);
  File file2(BLANK_DIFFERENT_ESM);

  plugin1.SetIncompatibilities({file1});
  plugin2.SetIncompatibilities({file1, file2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<File>({file1, file2}), plugin1.GetIncompatibilities());
}

TEST(PluginMetadata, mergeMetadataShouldMergeMessages) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  Message message(MessageType::say, "content");

  plugin1.SetMessages({message});
  plugin2.SetMessages({message});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<Message>({message, message}), plugin1.GetMessages());
}

TEST(PluginMetadata, mergeMetadataShouldMergeTags) {
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

TEST(PluginMetadata, mergeMetadataShouldMergeDirtyInfoData) {
  PluginMetadata plugin1;
  PluginMetadata plugin2;
  PluginCleaningData info1(
      0x5, "utility", {MessageContent("info")}, 1, 2, 3, "condition");
  PluginCleaningData info2(
      0xA, "utility", {MessageContent("info")}, 1, 2, 3, "condition");

  plugin1.SetDirtyInfo({info1});
  plugin2.SetDirtyInfo({info1, info2});
  plugin1.MergeMetadata(plugin2);

  EXPECT_EQ(std::vector<PluginCleaningData>({info1, info2}),
            plugin1.GetDirtyInfo());
}
TEST(PluginMetadata, mergeMetadataShouldMergeCleanInfoData) {
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

TEST(PluginMetadata, mergeMetadataShouldMergeLocationData) {
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

TEST(PluginMetadata, unsetGroupShouldLeaveNoGroupValueSet) {
  PluginMetadata plugin;
  EXPECT_FALSE(plugin.GetGroup().has_value());

  plugin.SetGroup("test");
  EXPECT_EQ("test", plugin.GetGroup().value());

  plugin.UnsetGroup();
  EXPECT_FALSE(plugin.GetGroup().has_value());
}

TEST(PluginMetadata,
     hasNameOnlyShouldBeTrueForADefaultConstructedPluginMetadataObject) {
  PluginMetadata plugin;

  EXPECT_TRUE(plugin.HasNameOnly());
}

TEST(PluginMetadata,
     hasNameOnlyShouldBeTrueForAPluginMetadataObjectConstructedWithAName) {
  PluginMetadata plugin(BLANK_ESP);

  EXPECT_TRUE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfTheGroupIsExplicit) {
  PluginMetadata plugin;
  plugin.SetGroup("group");

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfLoadAfterMetadataExists) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetLoadAfterFiles({File(BLANK_ESM)});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfRequirementMetadataExists) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetRequirements({File(BLANK_ESM)});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfIncompatibilityMetadataExists) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetIncompatibilities({File(BLANK_ESM)});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfMessagesExist) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetMessages({Message(MessageType::say, "content")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfTagsExist) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetTags({Tag("Relev")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfDirtyInfoExists) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetDirtyInfo({PluginCleaningData(
      5, "utility", {MessageContent("info")}, 0, 1, 2, "condition")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfCleanInfoExists) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetCleanInfo({PluginCleaningData(5, "utility")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, hasNameOnlyShouldBeFalseIfLocationsExist) {
  PluginMetadata plugin(BLANK_ESP);
  plugin.SetLocations({Location("http://www.example.com")});

  EXPECT_FALSE(plugin.HasNameOnly());
}

TEST(PluginMetadata, isRegexPluginShouldBeFalseForAnEmptyPluginName) {
  PluginMetadata plugin;

  EXPECT_FALSE(plugin.IsRegexPlugin());
}

TEST(PluginMetadata, isRegexPluginShouldBeFalseForAnExactPluginFilename) {
  PluginMetadata plugin(BLANK_ESM);

  EXPECT_FALSE(plugin.IsRegexPlugin());
}

TEST(PluginMetadata, isRegexPluginShouldBeTrueIfThePluginNameContainsAColon) {
  PluginMetadata plugin("Blank:.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST(PluginMetadata,
     isRegexPluginShouldBeTrueIfThePluginNameContainsABackslash) {
  PluginMetadata plugin("Blank\\.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST(PluginMetadata,
     isRegexPluginShouldBeTrueIfThePluginNameContainsAnAsterisk) {
  PluginMetadata plugin("Blank*.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST(PluginMetadata,
     isRegexPluginShouldBeTrueIfThePluginNameContainsAQuestionMark) {
  PluginMetadata plugin("Blank?.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST(PluginMetadata,
     isRegexPluginShouldBeTrueIfThePluginNameContainsAVerticalBar) {
  PluginMetadata plugin("Blank|.esm");

  EXPECT_TRUE(plugin.IsRegexPlugin());
}

TEST(PluginMetadata,
     asYamlShouldReturnAStringContainingTheMetadataEmittedAsYaml) {
  PluginMetadata plugin(BLANK_ESM);
  plugin.SetLoadAfterFiles({File(BLANK_ESM)});

  EXPECT_EQ(
      "name: 'Blank.esm'\n"
      "after: [ 'Blank.esm' ]",
      plugin.AsYaml());
}
}

#endif
