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
      CommonGameTestFixture(GameType::tes4),
      metadataPath(metadataFilesPath / "masterlist.yaml"),
      savedMetadataPath(metadataFilesPath / "saved.masterlist.yaml"),
      missingMetadataPath(metadataFilesPath / "missing-metadata.yaml") {}

  void SetUp() override {
    CommonGameTestFixture::SetUp();

    using std::filesystem::copy;
    using std::filesystem::exists;

    writeMasterlist(metadataPath);
    ASSERT_TRUE(exists(metadataPath));

    ASSERT_FALSE(exists(savedMetadataPath));
    ASSERT_FALSE(exists(missingMetadataPath));
  }

  static void writeMasterlist(const std::filesystem::path& path) {
    std::ofstream out(path);
    out << R"(bash_tags:
  - 'C.Climate'
  - 'Relev'

groups:
  - name: group1
    after:
      - group2
  - name: group2
    after:
      - default

globals:
  - type: say
    content: 'A global message.'

plugins:
  - name: 'Blank.esm'
    priority: -100
    msg:
      - type: warn
        content: 'This is a warning.'
      - type: say
        content: 'This message should be removed when evaluating conditions.'
        condition: 'active("Blank - Different.esm")'

  - name: 'Blank.+\.esp'
    after:
      - 'Blank.esm'

  - name: 'Blank.+(Different)?.*\.esp'
    inc:
      - 'Blank.esp'

  - name: 'Blank.esp'
    group: group2
    dirty:
      - crc: 0xDEADBEEF
        util: utility)";
    out.close();
  }

  static std::string PluginMetadataToString(const PluginMetadata& metadata) {
    return metadata.GetName();
  }

  const std::filesystem::path metadataPath;
  const std::filesystem::path savedMetadataPath;
  const std::filesystem::path groupMetadataPath;
  const std::filesystem::path missingMetadataPath;
};

TEST_F(MetadataListTest, loadShouldLoadGlobalMessages) {
  MetadataList metadataList;

  EXPECT_NO_THROW(metadataList.Load(metadataPath));
  EXPECT_EQ(std::vector<Message>({
                Message(MessageType::say, "A global message."),
            }),
            metadataList.Messages());
}

TEST_F(MetadataListTest, loadShouldLoadPluginMetadata) {
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

TEST_F(MetadataListTest, loadShouldLoadBashTags) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  EXPECT_EQ(std::vector<std::string>({"C.Climate", "Relev"}),
            metadataList.BashTags());
}

TEST_F(MetadataListTest, loadShouldLoadGroups) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  auto groups = metadataList.Groups();

  ASSERT_EQ(3, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());

  EXPECT_EQ("group1", groups[1].GetName());
  EXPECT_EQ(std::vector<std::string>({"group2"}), groups[1].GetAfterGroups());

  EXPECT_EQ("group2", groups[2].GetName());
  EXPECT_EQ(std::vector<std::string>({"default"}), groups[2].GetAfterGroups());
}

TEST_F(MetadataListTest, loadYamlParsingShouldSupportMergeKeys) {
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

  ASSERT_EQ(1, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_EQ(std::vector<std::string>({"earliest"}), groups[0].GetAfterGroups());
}

TEST_F(MetadataListTest, loadShouldThrowIfAnInvalidMetadataFileIsGiven) {
  MetadataList metadataList;

  std::ofstream out(metadataPath);
  out << R"(  - 'C.Climate'
  - 'Relev'

globals:
  - type: say
    content: 'A global message.'

plugins:
  - name: 'Blank.+\.esp'
    after:
      - 'Blank.esm')";
  out.close();

  EXPECT_THROW(metadataList.Load(metadataPath), std::runtime_error);

  out.open(metadataPath);
  out << R"(globals:
  - type: say
    content: 'A global message.'

plugins:
  - name: 'Blank.esm'
    priority: -100
    msg:
      - type: warn
        content: 'This is a warning.'
      - type: say
        content: 'This message should be removed when evaluating conditions.'
        condition: 'active("Blank - Different.esm")'

  - name: 'Blank.esm'
    msg:
      - type: error
        content: 'This plugin entry will cause a failure, as it is not the first exact entry.')";
  out.close();

  EXPECT_THROW(metadataList.Load(metadataPath), std::runtime_error);
}

TEST_F(MetadataListTest,
       loadShouldClearExistingDataIfAnInvalidMetadataFileIsGiven) {
  MetadataList metadataList;

  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  EXPECT_THROW(metadataList.Load(blankEsm), std::runtime_error);
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_F(MetadataListTest,
       loadShouldClearExistingDataIfAMissingMetadataFileIsGiven) {
  MetadataList metadataList;

  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  EXPECT_THROW(metadataList.Load(missingMetadataPath), std::runtime_error);
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_F(
    MetadataListTest,
    loadWithPreludeShouldReplaceThePreludeInTheFirstFileWithTheContentOfTheSecond) {
  using std::endl;

  std::ofstream out(metadataPath);
  out << "prelude:" << endl
      << "  - &ref" << endl
      << "    type: say" << endl
      << "    content: Loaded from same file" << endl
      << "globals:" << endl
      << "  - *ref" << endl;

  out.close();

  auto preludePath = metadataFilesPath / "prelude.yaml";
  out.open(preludePath);
  out << "common:" << endl
      << "  - &ref" << endl
      << "    type: say" << endl
      << "    content: Loaded from prelude" << endl;

  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.LoadWithPrelude(metadataPath, preludePath));

  auto messages = metadataList.Messages();
  ASSERT_EQ(1, messages.size());
  EXPECT_EQ(MessageType::say, messages[0].GetType());
  ASSERT_EQ(1, messages[0].GetContent().size());
  EXPECT_EQ("Loaded from prelude", messages[0].GetContent()[0].GetText());
}

TEST_F(MetadataListTest, saveShouldWriteTheLoadedMetadataToTheGivenFilePath) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  EXPECT_NO_THROW(metadataList.Save(savedMetadataPath));

  EXPECT_TRUE(std::filesystem::exists(savedMetadataPath));

  // Check the new file contains the same metadata.
  EXPECT_NO_THROW(metadataList.Load(savedMetadataPath));

  EXPECT_EQ(std::vector<std::string>({"C.Climate", "Relev"}),
            metadataList.BashTags());

  auto expectedGroups = std::vector<Group>({Group("default"),
                                            Group("group1", {"group2"}),
                                            Group("group2", {"default"})});
  EXPECT_EQ(expectedGroups, metadataList.Groups());

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

TEST_F(MetadataListTest, clearShouldClearLoadedData) {
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

TEST_F(MetadataListTest, setGroupsShouldReplaceExistingGroups) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  metadataList.SetGroups({Group("group4")});

  auto groups = metadataList.Groups();

  ASSERT_EQ(2, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());

  EXPECT_EQ("group4", groups[1].GetName());
  EXPECT_TRUE(groups[1].GetAfterGroups().empty());
}

TEST_F(
    MetadataListTest,
    findPluginShouldReturnAnEmptyOptionalIfTheGivenPluginIsNotInTheMetadataList) {
  MetadataList metadataList;
  EXPECT_FALSE(metadataList.FindPlugin(blankDifferentEsm));
}

TEST_F(
    MetadataListTest,
    findPluginShouldReturnTheMetadataObjectInTheMetadataListIfOneExistsForTheGivenPlugin) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(blankDifferentEsp).value();

  EXPECT_EQ(blankDifferentEsp, plugin.GetName());
  EXPECT_EQ(std::vector<File>({
                File(blankEsm),
            }),
            plugin.GetLoadAfterFiles());
  EXPECT_EQ(std::vector<File>({
                File(blankEsp),
            }),
            plugin.GetIncompatibilities());
}

TEST_F(MetadataListTest, addPluginShouldStoreGivenSpecificPluginMetadata) {
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

TEST_F(MetadataListTest, addPluginShouldStoreGivenRegexPluginMetadata) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin(".+Dependent\\.esp");
  plugin.SetGroup("group1");
  metadataList.AddPlugin(plugin);

  plugin = metadataList.FindPlugin(blankPluginDependentEsp).value();

  EXPECT_EQ("group1", plugin.GetGroup());
}

TEST_F(MetadataListTest, addPluginShouldThrowIfAMatchingPluginAlreadyExists) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(blankEsm).value();
  ASSERT_EQ(blankEsm, plugin.GetName());

  EXPECT_THROW(metadataList.AddPlugin(PluginMetadata(blankEsm)),
               std::invalid_argument);
}

TEST_F(MetadataListTest,
       erasePluginShouldRemoveStoredMetadataForTheGivenPlugin) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(blankEsp).value();
  ASSERT_EQ(blankEsp, plugin.GetName());
  ASSERT_FALSE(plugin.HasNameOnly());

  metadataList.ErasePlugin(plugin.GetName());

  EXPECT_FALSE(metadataList.FindPlugin(plugin.GetName()));
}

TEST(ReplaceMetadataListPrelude, shouldReturnAnEmptyStringIfGivenEmptyStrings) {
  std::string prelude = "";
  std::string masterlist = "";

  auto result = ReplaceMetadataListPrelude(prelude, std::string(masterlist));

  EXPECT_EQ(masterlist, result);
}

TEST(ReplaceMetadataListPrelude, shouldNotChangeAMasterlistWithNoPrelude) {
  std::string prelude = R"(globals:
  - type: note
    content: A message.
)";
  std::string masterlist = R"(plugins:
  - name: a.esp
)";

  auto result = ReplaceMetadataListPrelude(prelude, std::string(masterlist));

  EXPECT_EQ(masterlist, result);
}

TEST(ReplaceMetadataListPrelude,
     shouldReplaceAPreludeAtTheStartOfTheMasterlist) {
  std::string prelude = R"(globals:
  - type: note
    content: A message.
)";
  std::string masterlist = R"(prelude:
  a: b

plugins:
  - name: a.esp
)";

  auto result = ReplaceMetadataListPrelude(prelude, std::move(masterlist));

  auto expectedResult = R"(prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
)";

  EXPECT_EQ(expectedResult, result);
}

TEST(ReplaceMetadataListPrelude, shouldChangeAMasterlistThatEndsWithAPrelude) {
  std::string prelude = R"(globals:
  - type: note
    content: A message.
)";
  std::string masterlist = R"(plugins:
  - name: a.esp
prelude:
  a: b

)";

  auto result = ReplaceMetadataListPrelude(prelude, std::move(masterlist));

  auto expectedResult = R"(plugins:
  - name: a.esp
prelude:
  globals:
    - type: note
      content: A message.
)";

  EXPECT_EQ(expectedResult, result);
}

TEST(ReplaceMetadataListPrelude, shouldReplaceOnlyThePreludeInTheMasterlist) {
  std::string prelude = R"(

globals:
  - type: note
    content: A message.

)";
  std::string masterlist = R"(
common:
  key: value
prelude:
  a: b
plugins:
  - name: a.esp
)";

  auto result = ReplaceMetadataListPrelude(prelude, std::move(masterlist));

  auto expectedResult = R"(
common:
  key: value
prelude:


  globals:
    - type: note
      content: A message.


plugins:
  - name: a.esp
)";

  EXPECT_EQ(expectedResult, result);
}

TEST(ReplaceMetadataListPrelude,
     shouldSucceedIfGivenABlockStylePreludeAndABlockStyleMasterlist) {
  std::string prelude = R"(globals:
  - type: note
    content: A message.
)";
  std::string masterlist = R"(prelude:
  a: b

plugins:
  - name: a.esp
)";

  auto result = ReplaceMetadataListPrelude(prelude, std::move(masterlist));

  auto expectedResult = R"(prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
)";

  EXPECT_EQ(expectedResult, result);
}

TEST(ReplaceMetadataListPrelude,
     shouldSucceedIfGivenAFlowStylePreludeAndABlockStyleMasterlist) {
  std::string prelude = "globals: [{type: note, content: A message.}]";
  std::string masterlist = R"(prelude:
  a: b

plugins:
  - name: a.esp
)";

  auto result = ReplaceMetadataListPrelude(prelude, std::move(masterlist));

  auto expectedResult = R"(prelude:
  globals: [{type: note, content: A message.}]
plugins:
  - name: a.esp
)";

  EXPECT_EQ(expectedResult, result);
}

TEST(ReplaceMetadataListPrelude, doesNotChangeAFlowStyleMasterlist) {
  std::string prelude = "globals: [{type: note, content: A message.}]";
  std::string masterlist = "{prelude: {}, plugins: [{name: a.esp}]}";

  auto result = ReplaceMetadataListPrelude(prelude, std::string(masterlist));

  EXPECT_EQ(masterlist, result);
}

TEST(ReplaceMetadataListPrelude, shouldNotStopAtComments) {
  std::string prelude = R"(globals:
  - type: note
    content: A message.
)";
  std::string masterlist = R"(prelude:
  a: b
# Comment line
  c: d

plugins:
  - name: a.esp
)";

  auto result = ReplaceMetadataListPrelude(prelude, std::move(masterlist));

  auto expectedResult = R"(prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
)";

  EXPECT_EQ(expectedResult, result);
}

TEST(ReplaceMetadataListPrelude, shouldNotStopAtABlankLine) {
  std::string prelude = R"(globals:
  - type: note
    content: A message.
)";
  std::string masterlist = R"(prelude:
  a: b


plugins:
  - name: a.esp
)";

  auto result = ReplaceMetadataListPrelude(prelude, std::move(masterlist));

  auto expectedResult = R"(prelude:
  globals:
    - type: note
      content: A message.

plugins:
  - name: a.esp
)";

  EXPECT_EQ(expectedResult, result);
}
}
}

#endif
