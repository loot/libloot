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

#ifndef LOOT_TESTS_API_INTERFACE_DATABASE_INTERFACE_TEST
#define LOOT_TESTS_API_INTERFACE_DATABASE_INTERFACE_TEST

#include "loot/api.h"
#include "tests/api/interface/api_game_operations_test.h"

namespace loot {
namespace test {
class DatabaseInterfaceTest : public ApiGameOperationsTest {
protected:
  DatabaseInterfaceTest() :
      userlistPath_(localPath / "userlist.yaml"),
      minimalOutputPath_(localPath / "minimal.yml"),
      generalUserlistMessage("A general userlist message.") {}

  void SetUp() override {
    ApiGameOperationsTest::SetUp();

    ASSERT_FALSE(std::filesystem::exists(minimalOutputPath_));
  }

  std::string GetExpectedMinimalContent() const {
    using std::endl;

    std::stringstream expectedContent;
    expectedContent << "plugins:" << endl
                    << "  - name: '" << blankDifferentEsm << "'" << endl
                    << "    dirty:" << endl
                    << "      - crc: 0x7D22F9DF" << endl
                    << "        util: 'TES4Edit'" << endl
                    << "        udr: 4" << endl
                    << "  - name: '" << blankEsm << "'" << endl
                    << "    tag:" << endl
                    << "      - Actors.ACBS" << endl
                    << "      - Actors.AIData" << endl
                    << "      - -C.Water";

    return expectedContent.str();
  }

  std::string GetFileContent(const std::filesystem::path& file) {
    std::ifstream stream(file);
    std::stringstream content;
    content << stream.rdbuf();

    return content.str();
  }

  void GenerateUserlist() {
    using std::endl;

    std::ofstream userlist(userlistPath_);
    userlist << "bash_tags:" << endl
             << "  - RaceRelations" << endl
             << "  - C.Lighting" << endl
             << "groups:" << endl
             << "  - name: group2" << endl
             << "    after:" << endl
             << "      - default" << endl
             << "  - name: group3" << endl
             << "    after:" << endl
             << "      - group1" << endl
             << "globals:" << endl
             << "  - type: say" << endl
             << "    content: '" << generalUserlistMessage << "'" << endl
             << "plugins:" << endl
             << "  - name: " << blankEsm << endl
             << "    after:" << endl
             << "      - " << blankDifferentEsm << endl
             << "    tag:" << endl
             << "      - name: Actors.ACBS" << endl
             << "        condition: 'file(\"" << missingEsp << "\")'" << endl
             << "  - name: " << blankDifferentEsp << endl
             << "    inc:" << endl
             << "      - " << blankEsp << endl
             << "    tag:" << endl
             << "      - name: C.Climate" << endl
             << "        condition: 'file(\"" << missingEsp << "\")'" << endl;

    userlist.close();
  }

  const std::filesystem::path userlistPath_;
  const std::filesystem::path minimalOutputPath_;
  const std::string generalUserlistMessage;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         DatabaseInterfaceTest,
                         ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(DatabaseInterfaceTest,
       loadMasterlistShouldSucceedEvenIfGameHandleIsDiscarded) {
  handle_ = CreateGameHandle(GetParam(), gamePath, localPath);

  ASSERT_NO_THROW(GenerateMasterlist());

  EXPECT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
}

TEST_P(DatabaseInterfaceTest,
       loadMasterlistShouldThrowIfNoMasterlistIsPresent) {
  EXPECT_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath),
               FileAccessError);
}

TEST_P(DatabaseInterfaceTest,
       loadMasterlistShouldSucceedIfTheMasterlistIsPresent) {
  ASSERT_NO_THROW(GenerateMasterlist());

  EXPECT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
}

TEST_P(
    DatabaseInterfaceTest,
    loadMasterlistWithPreludeShouldThrowIfAMasterlistIsPresentButAPreludeDoesNotExistAtTheGivenPath) {
  ASSERT_NO_THROW(GenerateMasterlist());

  auto preludePath = localPath / "prelude.yaml";

  EXPECT_THROW(handle_->GetDatabase().LoadMasterlistWithPrelude(masterlistPath,
                                                                preludePath),
               FileAccessError);
}

TEST_P(
    DatabaseInterfaceTest,
    loadMasterlistWithPreludeShouldSucceedIfTheMasterlistAndPreludeAreBothPresent) {
  using std::endl;

  std::ofstream out(masterlistPath);
  out << "prelude:" << endl
      << "  - &ref" << endl
      << "    type: say" << endl
      << "    content: Loaded from same file" << endl
      << "globals:" << endl
      << "  - *ref" << endl;

  out.close();

  auto preludePath = localPath / "prelude.yaml";
  out.open(preludePath);
  out << "common:" << endl
      << "  - &ref" << endl
      << "    type: say" << endl
      << "    content: Loaded from prelude" << endl;

  EXPECT_NO_THROW(handle_->GetDatabase().LoadMasterlistWithPrelude(
      masterlistPath, preludePath));

  auto messages = handle_->GetDatabase().GetGeneralMessages();
  ASSERT_EQ(1, messages.size());
  EXPECT_EQ(MessageType::say, messages[0].GetType());
  ASSERT_EQ(1, messages[0].GetContent().size());
  EXPECT_EQ("Loaded from prelude", messages[0].GetContent()[0].GetText());
}

TEST_P(DatabaseInterfaceTest,
       loadUserlistShouldThrowIfAUserlistDoesNotExistAtTheGivenPath) {
  ASSERT_NO_THROW(GenerateMasterlist());
  EXPECT_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_),
               FileAccessError);
}

TEST_P(DatabaseInterfaceTest, loadUserlistShouldSucceedIfTheUserlistIsPresent) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(std::filesystem::copy(masterlistPath, userlistPath_));

  EXPECT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeUserMetadataShouldThrowIfTheFileAlreadyExistsAndTheOverwriteArgumentIsFalse) {
  ASSERT_NO_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, false),
      FileAccessError);
}

TEST_P(
    DatabaseInterfaceTest,
    writeUserMetadataShouldReturnOkAndWriteToFileIfTheArgumentsAreValidAndTheOverwriteArgumentIsTrue) {
  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, true));
  EXPECT_TRUE(std::filesystem::exists(minimalOutputPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeUserMetadataShouldReturnOkIfTheFileAlreadyExistsAndTheOverwriteArgumentIsTrue) {
  ASSERT_NO_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, true));
}

TEST_P(DatabaseInterfaceTest,
       writeUserMetadataShouldThrowIfPathGivenExistsAndIsReadOnly) {
  ASSERT_NO_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  std::filesystem::permissions(minimalOutputPath_,
                               std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::replace);

  EXPECT_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, true),
      FileAccessError);
}

TEST_P(DatabaseInterfaceTest,
       writeUserMetadataShouldShouldNotWriteMasterlistMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));

  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, true));

  EXPECT_EQ("{}", GetFileContent(minimalOutputPath_));
}

TEST_P(DatabaseInterfaceTest, writeUserMetadataShouldShouldWriteUserMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(std::filesystem::copy(masterlistPath, userlistPath_));

  std::ofstream masterlist(masterlistPath);
  masterlist << "bash_tags:\n  []\nglobals:\n  []\nplugins:\n  []";
  masterlist.close();

  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteUserMetadata(minimalOutputPath_, true));

  EXPECT_FALSE(GetFileContent(minimalOutputPath_).empty());
}

TEST_P(DatabaseInterfaceTest, evaluateShouldReturnTrueIfTheConditionIsTrue) {
  EXPECT_TRUE(handle_->GetDatabase().Evaluate("file(\"Blank.esp\")"));
}

TEST_P(DatabaseInterfaceTest, evaluateShouldReturnFalseIfTheConditionIsFalse) {
  EXPECT_FALSE(handle_->GetDatabase().Evaluate("file(\"missing.esp\")"));
}

TEST_P(DatabaseInterfaceTest,
       getGroupsShouldReturnAllGroupsListedInTheLoadedMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto groups = handle_->GetDatabase().GetGroups();

  ASSERT_EQ(4, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());

  EXPECT_EQ("group1", groups[1].GetName());
  EXPECT_TRUE(groups[1].GetAfterGroups().empty());

  EXPECT_EQ("group2", groups[2].GetName());
  EXPECT_EQ(std::vector<std::string>({"group1", "default"}),
            groups[2].GetAfterGroups());

  EXPECT_EQ("group3", groups[3].GetName());
  EXPECT_EQ(std::vector<std::string>({"group1"}), groups[3].GetAfterGroups());
}

TEST_P(DatabaseInterfaceTest,
       getGroupsShouldReturnDefaultGroupEvenIfNoMetadataIsLoaded) {
  auto groups = handle_->GetDatabase().GetGroups();

  ASSERT_EQ(1, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());
}

TEST_P(DatabaseInterfaceTest,
       getGroupsShouldNotIncludeUserlistMetadataIfParameterIsFalse) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto groups = handle_->GetDatabase().GetGroups(false);

  ASSERT_EQ(3, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());

  EXPECT_EQ("group1", groups[1].GetName());
  EXPECT_TRUE(groups[1].GetAfterGroups().empty());

  EXPECT_EQ("group2", groups[2].GetName());
  EXPECT_EQ(std::vector<std::string>({"group1"}), groups[2].GetAfterGroups());
}

TEST_P(
    DatabaseInterfaceTest,
    getGroupsShouldReturnDefaultGroupIfNoMasterlistIsLoadedAndUserlistMetadataIsNotIncluded) {
  auto groups = handle_->GetDatabase().GetGroups(false);

  EXPECT_EQ(1, groups.size());

  EXPECT_EQ("default", groups.begin()->GetName());
  EXPECT_TRUE(groups.begin()->GetAfterGroups().empty());
}

TEST_P(DatabaseInterfaceTest,
       getUserGroupsShouldReturnOnlyGroupMetadataFromTheUserlist) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto groups = handle_->GetDatabase().GetUserGroups();

  ASSERT_EQ(3, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());

  EXPECT_EQ("group2", groups[1].GetName());
  EXPECT_EQ(std::vector<std::string>({"default"}), groups[1].GetAfterGroups());

  EXPECT_EQ("group3", groups[2].GetName());
  EXPECT_EQ(std::vector<std::string>({"group1"}), groups[2].GetAfterGroups());
}

TEST_P(
    DatabaseInterfaceTest,
    setUserGroupsShouldReplaceExistingUserGroupMetadataWithTheGivenMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  handle_->GetDatabase().SetUserGroups(std::vector<Group>({
      Group("group4"),
  }));

  auto groups = handle_->GetDatabase().GetUserGroups();

  ASSERT_EQ(2, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());

  EXPECT_EQ("group4", groups[1].GetName());
  EXPECT_TRUE(groups[1].GetAfterGroups().empty());
}

TEST_P(DatabaseInterfaceTest,
       getGroupsPathShouldReturnTheShortestPathBetweenTheGivenGroups) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto path = handle_->GetDatabase().GetGroupsPath("group1", "group3");

  ASSERT_EQ(2, path.size());
  EXPECT_EQ("group1", path[0].GetName());
  EXPECT_EQ(EdgeType::userLoadAfter, path[0].GetTypeOfEdgeToNextVertex());
  EXPECT_EQ("group3", path[1].GetName());
  EXPECT_FALSE(path[1].GetTypeOfEdgeToNextVertex().has_value());
}

TEST_P(DatabaseInterfaceTest,
       getKnownBashTagsShouldReturnAllBashTagsListedInLoadedMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto tags = handle_->GetDatabase().GetKnownBashTags();

  std::vector<std::string> expectedTags({
      "Actors.ACBS",
      "C.Climate",
      "RaceRelations",
      "C.Lighting",
  });
  EXPECT_EQ(expectedTags, tags);
}

TEST_P(DatabaseInterfaceTest,
       getGeneralMessagesShouldGetGeneralMessagesFromTheMasterlistAndUserlist) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto messages = handle_->GetDatabase().GetGeneralMessages();

  std::vector<Message> expectedMessages({
      Message(MessageType::say,
              generalMasterlistMessage,
              "file(\"" + missingEsp + "\")"),
      Message(MessageType::say, generalUserlistMessage),
  });
  EXPECT_EQ(expectedMessages, messages);
}

TEST_P(
    DatabaseInterfaceTest,
    getGeneralMessagesShouldReturnOnlyValidMessagesIfConditionsAreEvaluated) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));

  auto messages = handle_->GetDatabase().GetGeneralMessages(true);

  EXPECT_TRUE(messages.empty());
}

TEST_P(DatabaseInterfaceTest,
       getPluginMetadataShouldReturnAnEmptyOptionalIfThePluginHasNoMetadata) {
  EXPECT_FALSE(handle_->GetDatabase().GetPluginMetadata(blankEsm));
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginMetadataShouldReturnMergedMasterAndUserMetadataForTheGivenPluginIfIncludeUserMetadataIsTrue) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto metadata =
      handle_->GetDatabase().GetPluginMetadata(blankEsm, true).value();

  std::vector<File> expectedLoadAfter({
      File(blankDifferentEsm),
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(DatabaseInterfaceTest,
       getPluginMetadataShouldPreferUserMetadataWhenMergingMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto metadata =
      handle_->GetDatabase().GetPluginMetadata(blankEsm, true).value();

  std::vector<Tag> expectedTags({
      Tag("Actors.ACBS", true, "file(\"" + missingEsp + "\")"),
      Tag("Actors.ACBS"),
      Tag("Actors.AIData"),
      Tag("C.Water", false),
  });
  EXPECT_EQ(expectedTags, metadata.GetTags());
  EXPECT_EQ("file(\"" + missingEsp + "\")",
            metadata.GetTags()[0].GetCondition());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginMetadataShouldReturnOnlyMasterlistMetadataForTheGivenPluginIfIncludeUserMetadataIsFalse) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto metadata =
      handle_->GetDatabase().GetPluginMetadata(blankEsm, false).value();

  std::vector<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginMetadataShouldReturnOnlyValidMetadataForTheGivenPluginIfConditionsAreEvaluated) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));

  auto metadata =
      handle_->GetDatabase().GetPluginMetadata(blankEsm, false, true).value();

  EXPECT_TRUE(metadata.GetMessages().empty());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginUserMetadataShouldReturnAnEmptyPluginMetadataObjectIfThePluginHasNoUserMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  EXPECT_FALSE(handle_->GetDatabase().GetPluginUserMetadata(blankDifferentEsm));
}

TEST_P(DatabaseInterfaceTest,
       getPluginUserMetadataShouldReturnOnlyUserMetadataForTheGivenPlugin) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto metadata =
      handle_->GetDatabase().GetPluginUserMetadata(blankEsm).value();

  std::vector<File> expectedLoadAfter({
      File(blankDifferentEsm),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginUserMetadataShouldReturnOnlyValidMetadataForTheGivenPluginIfConditionsAreEvaluated) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  auto metadata =
      handle_->GetDatabase().GetPluginMetadata(blankEsm, false, true).value();

  EXPECT_TRUE(metadata.GetMessages().empty());
}

TEST_P(
    DatabaseInterfaceTest,
    setPluginUserMetadataShouldReplaceExistingUserMetadataWithTheGivenMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  PluginMetadata newMetadata(blankDifferentEsp);
  newMetadata.SetRequirements(std::vector<File>({File(masterFile)}));

  handle_->GetDatabase().SetPluginUserMetadata(newMetadata);

  auto metadata =
      handle_->GetDatabase().GetPluginUserMetadata(blankDifferentEsp).value();

  std::set<File> expectedLoadAfter({
      File(blankDifferentEsm),
  });
  EXPECT_TRUE(metadata.GetIncompatibilities().empty());
  EXPECT_EQ(newMetadata.GetRequirements(), metadata.GetRequirements());
}

TEST_P(DatabaseInterfaceTest,
       setPluginUserMetadataShouldNotAffectExistingMasterlistMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  PluginMetadata newMetadata(blankEsm);
  newMetadata.SetRequirements(std::vector<File>({File(masterFile)}));

  handle_->GetDatabase().SetPluginUserMetadata(newMetadata);

  auto metadata = handle_->GetDatabase().GetPluginMetadata(blankEsm).value();

  std::vector<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldDiscardAllUserMetadataForTheGivenPlugin) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  handle_->GetDatabase().DiscardPluginUserMetadata(blankEsm);

  EXPECT_FALSE(handle_->GetDatabase().GetPluginUserMetadata(blankEsm));
}

TEST_P(
    DatabaseInterfaceTest,
    discardPluginUserMetadataShouldNotDiscardMasterlistMetadataForTheGivenPlugin) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  handle_->GetDatabase().DiscardPluginUserMetadata(blankEsm);

  auto metadata = handle_->GetDatabase().GetPluginMetadata(blankEsm).value();

  std::vector<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldNotDiscardUserMetadataForOtherPlugins) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  handle_->GetDatabase().DiscardPluginUserMetadata(blankEsm);

  auto metadata =
      handle_->GetDatabase().GetPluginUserMetadata(blankDifferentEsp);

  EXPECT_TRUE(metadata);
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldNotDiscardGeneralMessages) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  handle_->GetDatabase().DiscardPluginUserMetadata(blankEsm);

  auto messages = handle_->GetDatabase().GetGeneralMessages();

  std::vector<Message> expectedMessages({
      Message(MessageType::say,
              generalMasterlistMessage,
              "file(\"" + missingEsp + "\")"),
      Message(MessageType::say, generalUserlistMessage),
  });
  EXPECT_EQ(expectedMessages, messages);
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldNotDiscardKnownBashTags) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  handle_->GetDatabase().DiscardPluginUserMetadata(blankEsm);

  auto tags = handle_->GetDatabase().GetKnownBashTags();

  std::vector<std::string> expectedTags({
      "Actors.ACBS",
      "C.Climate",
      "RaceRelations",
      "C.Lighting",
  });
  EXPECT_EQ(expectedTags, tags);
}

TEST_P(
    DatabaseInterfaceTest,
    discardAllUserMetadataShouldDiscardAllUserMetadataAndNoMasterlistMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));
  ASSERT_NO_THROW(handle_->GetDatabase().LoadUserlist(userlistPath_));

  handle_->GetDatabase().DiscardAllUserMetadata();

  EXPECT_FALSE(handle_->GetDatabase().GetPluginUserMetadata(blankEsm));
  EXPECT_FALSE(handle_->GetDatabase().GetPluginUserMetadata(blankDifferentEsp));

  auto metadata = handle_->GetDatabase().GetPluginMetadata(blankEsm).value();

  std::vector<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());

  auto messages = handle_->GetDatabase().GetGeneralMessages();

  std::vector<Message> expectedMessages({
      Message(MessageType::say,
              generalMasterlistMessage,
              "file(\"" + missingEsp + "\")"),
  });
  EXPECT_EQ(expectedMessages, messages);

  auto tags = handle_->GetDatabase().GetKnownBashTags();

  std::vector<std::string> expectedTags({
      "Actors.ACBS",
      "C.Climate",
  });
  EXPECT_EQ(expectedTags, tags);
}

TEST_P(DatabaseInterfaceTest,
       writeMinimalListShouldReturnOkAndWriteToFileIfArgumentsGivenAreValid) {
  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, false));
  EXPECT_TRUE(std::filesystem::exists(minimalOutputPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeMinimalListShouldThrowIfTheFileAlreadyExistsAndTheOverwriteArgumentIsFalse) {
  ASSERT_NO_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, false),
      FileAccessError);
}

TEST_P(
    DatabaseInterfaceTest,
    writeMinimalListShouldReturnOkAndWriteToFileIfTheArgumentsAreValidAndTheOverwriteArgumentIsTrue) {
  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, true));
  EXPECT_TRUE(std::filesystem::exists(minimalOutputPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeMinimalListShouldReturnOkIfTheFileAlreadyExistsAndTheOverwriteArgumentIsTrue) {
  ASSERT_NO_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, true));
}

TEST_P(DatabaseInterfaceTest,
       writeMinimalListShouldThrowIfPathGivenExistsAndIsReadOnly) {
  ASSERT_NO_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  std::filesystem::permissions(minimalOutputPath_,
                               std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::replace);

  EXPECT_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, true),
      FileAccessError);
}

TEST_P(DatabaseInterfaceTest,
       writeMinimalListShouldWriteOnlyBashTagsAndDirtyInfo) {
    using std::endl;

  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));

  EXPECT_NO_THROW(
      handle_->GetDatabase().WriteMinimalList(minimalOutputPath_, true));

  const auto content = GetFileContent(minimalOutputPath_);

  // Plugin entries are unordered.
  std::stringstream expectedContent;
  if (content.find(blankDifferentEsm) < content.find(blankEsm)) {
    expectedContent << "plugins:" << endl
                    << "  - name: '" << blankDifferentEsm << "'" << endl
                    << "    dirty:" << endl
                    << "      - crc: 0x7D22F9DF" << endl
                    << "        util: 'TES4Edit'" << endl
                    << "        udr: 4" << endl
                    << "  - name: '" << blankEsm << "'" << endl
                    << "    tag:" << endl
                    << "      - Actors.ACBS" << endl
                    << "      - Actors.AIData" << endl
                    << "      - -C.Water";
  } else {
    expectedContent << "plugins:" << endl
                    << "  - name: '" << blankEsm << "'" << endl
                    << "    tag:" << endl
                    << "      - Actors.ACBS" << endl
                    << "      - Actors.AIData" << endl
                    << "      - -C.Water" << endl
                    << "  - name: '" << blankDifferentEsm << "'" << endl
                    << "    dirty:" << endl
                    << "      - crc: 0x7D22F9DF" << endl
                    << "        util: 'TES4Edit'" << endl
                    << "        udr: 4";
  }

  EXPECT_EQ(expectedContent.str(), content);
}
}
}

#endif
