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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_LIST_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_LIST_TEST

#include "api/metadata_list.h"

#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class MetadataListTest : public CommonGameTestFixture {
protected:
  MetadataListTest() :
      metadataPath(metadataFilesPath / "masterlist.yaml"),
      savedMetadataPath(metadataFilesPath / "saved.masterlist.yaml"),
      missingMetadataPath(metadataFilesPath / "missing-metadata.yaml"),
      invalidMetadataPaths(
          {metadataFilesPath / "invalid" / "non_map_root.yaml",
           metadataFilesPath / "invalid" / "non_unique.yaml"}) {}

  inline virtual void SetUp() {
    CommonGameTestFixture::SetUp();

    using std::filesystem::copy;
    using std::filesystem::exists;

    auto sourceDirectory = getSourceMetadataFilesPath();

    copy(sourceDirectory / "masterlist.yaml", metadataPath);
    ASSERT_TRUE(exists(metadataPath));

    copyInvalidMetadataFile(sourceDirectory, "non_map_root.yaml");
    copyInvalidMetadataFile(sourceDirectory, "non_unique.yaml");

    ASSERT_FALSE(exists(savedMetadataPath));
    ASSERT_FALSE(exists(missingMetadataPath));
  }

  void copyInvalidMetadataFile(const std::filesystem::path& sourceDirectory,
                               const std::string& file) {
    std::filesystem::create_directories(metadataFilesPath / "invalid");
    std::filesystem::copy(sourceDirectory / "invalid" / file,
                          metadataFilesPath / "invalid" / file);
    ASSERT_TRUE(std::filesystem::exists(metadataFilesPath / "invalid" / file));
  }

  static std::string PluginMetadataToString(const PluginMetadata& metadata) {
    return metadata.GetName();
  }

  const std::filesystem::path metadataPath;
  const std::filesystem::path savedMetadataPath;
  const std::filesystem::path groupMetadataPath;
  const std::filesystem::path missingMetadataPath;
  const std::vector<std::filesystem::path> invalidMetadataPaths;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(, MetadataListTest, ::testing::Values(GameType::tes4));

TEST_P(MetadataListTest, loadShouldLoadGlobalMessages) {
  MetadataList metadataList;

  EXPECT_NO_THROW(metadataList.Load(metadataPath));
  EXPECT_EQ(std::vector<Message>({
                Message(MessageType::say, "A global message."),
            }),
            metadataList.Messages());
}

TEST_P(MetadataListTest, loadShouldLoadPluginMetadata) {
  MetadataList metadataList;

  EXPECT_NO_THROW(metadataList.Load(metadataPath));
  // Non-regex plugins can be outputted in any order, and regex entries can
  // match each other, so convert the list to a set of strings for
  // comparison.
  std::vector<PluginMetadata> result(metadataList.Plugins());
  std::set<std::string> names;
  std::transform(
      begin(result),
      end(result),
      std::insert_iterator<std::set<std::string>>(names, begin(names)),
      &MetadataListTest::PluginMetadataToString);

  EXPECT_EQ(std::set<std::string>({
                blankEsm,
                blankEsp,
                "Blank.+\\.esp",
                "Blank.+(Different)?.*\\.esp",
            }),
            names);
}

TEST_P(MetadataListTest, loadShouldLoadBashTags) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  EXPECT_EQ(std::set<std::string>({"C.Climate", "Relev"}),
            metadataList.BashTags());
}

TEST_P(MetadataListTest, loadShouldLoadGroups) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  auto groups = metadataList.Groups();

  EXPECT_EQ(3, groups.size());

  EXPECT_EQ(1, groups.count(Group("default")));
  EXPECT_TRUE(groups.find(Group("default"))->GetAfterGroups().empty());

  EXPECT_EQ(1, groups.count(Group("group1")));
  EXPECT_EQ(std::unordered_set<std::string>({"group2"}),
            groups.find(Group("group1"))->GetAfterGroups());

  EXPECT_EQ(1, groups.count(Group("group2")));
  EXPECT_EQ(std::unordered_set<std::string>({"default"}),
            groups.find(Group("group2"))->GetAfterGroups());
}

TEST_P(MetadataListTest, loadYamlParsingShouldSupportMergeKeys) {
  using std::endl;

  std::ofstream out(metadataPath);
  out << "common:" << endl
      << "  - &earlier" << endl
      << "    name: earlier" << endl
      << "    after:" << endl
      << "      - earliest" << endl
      << "groups:" << endl
      << "  - name: default" << endl
      << "    <<: *earlier" << endl;

  out.close();

  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  auto groups = metadataList.Groups();

  EXPECT_EQ(1, groups.size());

  EXPECT_EQ(1, groups.count(Group("default")));
  EXPECT_EQ(std::unordered_set<std::string>({"earliest"}),
            groups.find(Group("default"))->GetAfterGroups());
}

TEST_P(MetadataListTest, loadShouldThrowIfAnInvalidMetadataFileIsGiven) {
  MetadataList ml;
  for (const auto& path : invalidMetadataPaths) {
    EXPECT_THROW(ml.Load(path), FileAccessError);
  }
}

TEST_P(MetadataListTest,
       loadShouldClearExistingDataIfAnInvalidMetadataFileIsGiven) {
  MetadataList metadataList;

  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  EXPECT_THROW(metadataList.Load(blankEsm), FileAccessError);
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_P(MetadataListTest,
       loadShouldClearExistingDataIfAMissingMetadataFileIsGiven) {
  MetadataList metadataList;

  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  EXPECT_THROW(metadataList.Load(missingMetadataPath), FileAccessError);
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_P(MetadataListTest, saveShouldWriteTheLoadedMetadataToTheGivenFilePath) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  EXPECT_NO_THROW(metadataList.Save(savedMetadataPath));

  EXPECT_TRUE(std::filesystem::exists(savedMetadataPath));

  // Check the new file contains the same metadata.
  EXPECT_NO_THROW(metadataList.Load(savedMetadataPath));

  EXPECT_EQ(std::set<std::string>({"C.Climate", "Relev"}),
            metadataList.BashTags());

  EXPECT_EQ(std::unordered_set<Group>(
                {Group("default"), Group("group1"), Group("group2")}),
            metadataList.Groups());

  EXPECT_EQ(std::vector<Message>({
                Message(MessageType::say, "A global message."),
            }),
            metadataList.Messages());

  // Non-regex plugins can be outputted in any order, and regex entries can
  // match each other, so convert the list to a set of strings for
  // comparison.
  std::vector<PluginMetadata> result(metadataList.Plugins());
  std::set<std::string> names;
  std::transform(
      begin(result),
      end(result),
      std::insert_iterator<std::set<std::string>>(names, begin(names)),
      &MetadataListTest::PluginMetadataToString);
  EXPECT_EQ(std::set<std::string>({
                blankEsm,
                blankEsp,
                "Blank.+\\.esp",
                "Blank.+(Different)?.*\\.esp",
            }),
            names);
}

TEST_P(MetadataListTest, clearShouldClearLoadedData) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  metadataList.Clear();
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_P(MetadataListTest, setGroupsShouldReplaceExistingGroups) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  metadataList.SetGroups({Group("group4")});

  auto groups = metadataList.Groups();

  EXPECT_EQ(2, groups.size());

  EXPECT_EQ(1, groups.count(Group("default")));
  EXPECT_TRUE(groups.find(Group("default"))->GetAfterGroups().empty());

  EXPECT_EQ(1, groups.count(Group("group4")));
  EXPECT_TRUE(groups.find(Group("group4"))->GetAfterGroups().empty());
}

TEST_P(
    MetadataListTest,
    findPluginShouldReturnAnEmptyOptionalIfTheGivenPluginIsNotInTheMetadataList) {
  MetadataList metadataList;
  EXPECT_FALSE(metadataList.FindPlugin(blankDifferentEsm));
}

TEST_P(
    MetadataListTest,
    findPluginShouldReturnTheMetadataObjectInTheMetadataListIfOneExistsForTheGivenPlugin) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(blankDifferentEsp).value();

  EXPECT_EQ(blankDifferentEsp, plugin.GetName());
  EXPECT_EQ(std::set<File>({
                File(blankEsm),
            }),
            plugin.GetLoadAfterFiles());
  EXPECT_EQ(std::set<File>({
                File(blankEsp),
            }),
            plugin.GetIncompatibilities());
}

TEST_P(MetadataListTest, addPluginShouldStoreGivenSpecificPluginMetadata) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.FindPlugin(blankDifferentEsm));

  PluginMetadata plugin(blankDifferentEsm);
  plugin.SetGroup("group1");
  metadataList.AddPlugin(plugin);

  plugin = metadataList.FindPlugin(plugin.GetName()).value();

  EXPECT_EQ(blankDifferentEsm, plugin.GetName());
  EXPECT_EQ("group1", plugin.GetGroup());
}

TEST_P(MetadataListTest, addPluginShouldStoreGivenRegexPluginMetadata) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin(".+Dependent\\.esp");
  plugin.SetGroup("group1");
  metadataList.AddPlugin(plugin);

  plugin = metadataList.FindPlugin(blankPluginDependentEsp).value();

  EXPECT_EQ("group1", plugin.GetGroup());
}

TEST_P(MetadataListTest, addPluginShouldThrowIfAMatchingPluginAlreadyExists) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(blankEsm).value();
  ASSERT_EQ(blankEsm, plugin.GetName());

  EXPECT_THROW(metadataList.AddPlugin(PluginMetadata(blankEsm)),
               std::invalid_argument);
}

TEST_P(MetadataListTest,
       erasePluginShouldRemoveStoredMetadataForTheGivenPlugin) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(blankEsp).value();
  ASSERT_EQ(blankEsp, plugin.GetName());
  ASSERT_FALSE(plugin.HasNameOnly());

  metadataList.ErasePlugin(plugin.GetName());

  EXPECT_FALSE(metadataList.FindPlugin(plugin.GetName()));
}

TEST_P(
    MetadataListTest,
    evalAllConditionsShouldEvaluateTheConditionsForThePluginsStoredInTheMetadataList) {
  Game game(GetParam(), dataPath.parent_path(), localPath);
  ConditionEvaluator evaluator(game.Type(), game.DataPath());

  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(blankEsm).value();
  ASSERT_EQ(
      std::vector<Message>({
          Message(MessageType::warn, "This is a warning."),
          Message(MessageType::say,
                  "This message should be removed when evaluating conditions.",
                  "active(\"Blank - Different.esm\")"),
      }),
      plugin.GetMessages());

  plugin = metadataList.FindPlugin(blankEsp).value();
  ASSERT_EQ(blankEsp, plugin.GetName());
  ASSERT_FALSE(plugin.HasNameOnly());

  EXPECT_NO_THROW(metadataList.EvalAllConditions(evaluator));

  plugin = metadataList.FindPlugin(blankEsm).value();
  EXPECT_EQ(std::vector<Message>({
                Message(MessageType::warn, "This is a warning."),
            }),
            plugin.GetMessages());

  plugin = metadataList.FindPlugin(blankEsp).value();
  EXPECT_EQ(blankEsp, plugin.GetName());
  EXPECT_TRUE(plugin.GetDirtyInfo().empty());
}
}
}

#endif
