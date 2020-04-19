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
      db_(nullptr),
      userlistPath_(localPath / "userlist.yaml"),
      url_("./testing-metadata.git"),
      branch_("master"),
      oldBranch_("old-branch"),
      minimalOutputPath_(localPath / "minimal.yml"),
      generalUserlistMessage("A general userlist message.") {}

  void SetUp() {
    ApiGameOperationsTest::SetUp();

    db_ = handle_->GetDatabase();

    ASSERT_FALSE(std::filesystem::exists(minimalOutputPath_));
  }

  std::string GetExpectedMinimalContent() const {
    using std::endl;

    std::stringstream expectedContent;
    expectedContent << "plugins:" << endl
                    << "  - name: '" << blankDifferentEsm << "'" << endl
                    << "    dirty:" << endl
                    << "      - crc: 0x7d22f9df" << endl
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
  const std::string url_;
  const std::string branch_;
  const std::string oldBranch_;
  const std::string generalUserlistMessage;

  std::shared_ptr<DatabaseInterface> db_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        DatabaseInterfaceTest,
                        ::testing::Values(GameType::tes4,
                                          GameType::tes5,
                                          GameType::fo3,
                                          GameType::fonv,
                                          GameType::fo4,
                                          GameType::tes5se));

TEST_P(DatabaseInterfaceTest,
       loadListsShouldSucceedEvenIfGameHandleIsDiscarded) {
  db_ = CreateGameHandle(GetParam(), dataPath.parent_path(), localPath)
            ->GetDatabase();

  ASSERT_NO_THROW(GenerateMasterlist());

  EXPECT_NO_THROW(db_->LoadLists(masterlistPath, ""));
}

TEST_P(DatabaseInterfaceTest, loadListsShouldThrowIfNoMasterlistIsPresent) {
  EXPECT_THROW(db_->LoadLists(masterlistPath, ""), FileAccessError);
}

TEST_P(
    DatabaseInterfaceTest,
    loadListsShouldThrowIfAMasterlistIsPresentButAUserlistDoesNotExistAtTheGivenPath) {
  ASSERT_NO_THROW(GenerateMasterlist());
  EXPECT_THROW(db_->LoadLists(masterlistPath, userlistPath_), FileAccessError);
}

TEST_P(
    DatabaseInterfaceTest,
    loadListsShouldSucceedIfTheMasterlistIsPresentAndTheUserlistPathIsAnEmptyString) {
  ASSERT_NO_THROW(GenerateMasterlist());

  EXPECT_NO_THROW(db_->LoadLists(masterlistPath, ""));
}

TEST_P(DatabaseInterfaceTest,
       loadListsShouldSucceedIfTheMasterlistAndUserlistAreBothPresent) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(std::filesystem::copy(masterlistPath, userlistPath_));

  EXPECT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeUserMetadataShouldThrowIfTheFileAlreadyExistsAndTheOverwriteArgumentIsFalse) {
  ASSERT_NO_THROW(db_->WriteUserMetadata(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_THROW(db_->WriteUserMetadata(minimalOutputPath_, false),
               FileAccessError);
}

TEST_P(
    DatabaseInterfaceTest,
    writeUserMetadataShouldReturnOkAndWriteToFileIfTheArgumentsAreValidAndTheOverwriteArgumentIsTrue) {
  EXPECT_NO_THROW(db_->WriteUserMetadata(minimalOutputPath_, true));
  EXPECT_TRUE(std::filesystem::exists(minimalOutputPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeUserMetadataShouldReturnOkIfTheFileAlreadyExistsAndTheOverwriteArgumentIsTrue) {
  ASSERT_NO_THROW(db_->WriteUserMetadata(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_NO_THROW(db_->WriteUserMetadata(minimalOutputPath_, true));
}

TEST_P(DatabaseInterfaceTest,
       writeUserMetadataShouldThrowIfPathGivenExistsAndIsReadOnly) {
  ASSERT_NO_THROW(db_->WriteUserMetadata(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  std::filesystem::permissions(minimalOutputPath_,
                               std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::replace);

  EXPECT_THROW(db_->WriteUserMetadata(minimalOutputPath_, true),
               FileAccessError);
}

TEST_P(DatabaseInterfaceTest,
       writeUserMetadataShouldShouldNotWriteMasterlistMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, ""));

  EXPECT_NO_THROW(db_->WriteUserMetadata(minimalOutputPath_, true));

  EXPECT_EQ("{}", GetFileContent(minimalOutputPath_));
}

TEST_P(DatabaseInterfaceTest, writeUserMetadataShouldShouldWriteUserMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(std::filesystem::copy(masterlistPath, userlistPath_));

  std::ofstream masterlist(masterlistPath);
  masterlist << "bash_tags:\n  []\nglobals:\n  []\nplugins:\n  []";
  masterlist.close();

  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  EXPECT_NO_THROW(db_->WriteUserMetadata(minimalOutputPath_, true));

  EXPECT_FALSE(GetFileContent(minimalOutputPath_).empty());
}

TEST_P(DatabaseInterfaceTest,
       updateMasterlistShouldThrowIfTheMasterlistPathGivenIsInvalid) {
  EXPECT_THROW(db_->UpdateMasterlist("//\?", url_, branch_), std::exception);
}

TEST_P(DatabaseInterfaceTest,
       updateMasterlistShouldThrowIfTheMasterlistPathGivenIsEmpty) {
  EXPECT_THROW(db_->UpdateMasterlist("", url_, branch_), std::invalid_argument);
}

TEST_P(DatabaseInterfaceTest,
       updateMasterlistShouldThrowIfTheRepositoryUrlGivenCannotBeFound) {
  EXPECT_THROW(db_->UpdateMasterlist(
                   masterlistPath,
                   "https://github.com/loot/oblivion-does-not-exist.git",
                   branch_),
               std::system_error);
}

TEST_P(DatabaseInterfaceTest,
       updateMasterlistShouldThrowIfTheRepositoryUrlGivenIsEmpty) {
  EXPECT_THROW(db_->UpdateMasterlist(masterlistPath, "", branch_),
               std::invalid_argument);
}

TEST_P(DatabaseInterfaceTest,
       updateMasterlistShouldThrowIfTheRepositoryBranchGivenCannotBeFound) {
  EXPECT_THROW(db_->UpdateMasterlist(masterlistPath, url_, "missing-branch"),
               std::system_error);
}

TEST_P(DatabaseInterfaceTest,
       updateMasterlistShouldThrowIfTheRepositoryBranchGivenIsEmpty) {
  EXPECT_THROW(db_->UpdateMasterlist(masterlistPath, url_, ""),
               std::invalid_argument);
}

TEST_P(
    DatabaseInterfaceTest,
    updateMasterlistShouldSucceedIfPassedValidParametersAndOutputTrueIfTheMasterlistWasUpdated) {
  bool updated = false;
  EXPECT_NO_THROW(updated =
                      db_->UpdateMasterlist(masterlistPath, url_, branch_));
  EXPECT_TRUE(updated);
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(
    DatabaseInterfaceTest,
    updateMasterlistShouldSucceedIfCalledRepeatedlyButOnlyOutputTrueForTheFirstCall) {
  bool updated = false;
  EXPECT_NO_THROW(updated =
                      db_->UpdateMasterlist(masterlistPath, url_, branch_));
  EXPECT_TRUE(updated);

  EXPECT_NO_THROW(updated =
                      db_->UpdateMasterlist(masterlistPath, url_, branch_));
  EXPECT_FALSE(updated);
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(DatabaseInterfaceTest,
       getMasterlistRevisionShouldThrowIfNoMasterlistIsPresent) {
  MasterlistInfo info;
  EXPECT_THROW(info = db_->GetMasterlistRevision(masterlistPath, false),
               FileAccessError);
  EXPECT_TRUE(info.revision_id.empty());
  EXPECT_TRUE(info.revision_date.empty());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(
    DatabaseInterfaceTest,
    getMasterlistRevisionShouldThrowIfANonVersionControlledMasterlistIsPresent) {
  ASSERT_NO_THROW(GenerateMasterlist());

  MasterlistInfo info;
  EXPECT_THROW(info = db_->GetMasterlistRevision(masterlistPath, false),
               GitStateError);
  EXPECT_TRUE(info.revision_id.empty());
  EXPECT_TRUE(info.revision_date.empty());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(
    DatabaseInterfaceTest,
    getMasterlistRevisionShouldOutputLongStringsAndBooleanFalseIfAVersionControlledMasterlistIsPresentAndGetShortIdParameterIsFalse) {
  ASSERT_NO_THROW(db_->UpdateMasterlist(masterlistPath, url_, branch_));

  MasterlistInfo info;
  EXPECT_NO_THROW(info = db_->GetMasterlistRevision(masterlistPath, false));
  EXPECT_EQ(40, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(
    DatabaseInterfaceTest,
    getMasterlistRevisionShouldOutputShortStringsAndBooleanFalseIfAVersionControlledMasterlistIsPresentAndGetShortIdParameterIsTrue) {
  ASSERT_NO_THROW(db_->UpdateMasterlist(masterlistPath, url_, branch_));

  MasterlistInfo info;
  EXPECT_NO_THROW(info = db_->GetMasterlistRevision(masterlistPath, false));
  EXPECT_GE(size_t(40), info.revision_id.length());
  EXPECT_LE(size_t(7), info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(
    DatabaseInterfaceTest,
    getMasterlistRevisionShouldSucceedIfAnEditedVersionControlledMasterlistIsPresent) {
  ASSERT_NO_THROW(db_->UpdateMasterlist(masterlistPath, url_, branch_));
  ASSERT_NO_THROW(GenerateMasterlist());

  MasterlistInfo info;
  EXPECT_NO_THROW(info = db_->GetMasterlistRevision(masterlistPath, false));
  EXPECT_EQ(40, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_TRUE(info.is_modified);
}

TEST_P(
    DatabaseInterfaceTest,
    isLatestMasterlistShouldReturnFalseIfTheCurrentRevisionIsNotTheLatestRevisionInTheGivenBranch) {
  ASSERT_NO_THROW(db_->UpdateMasterlist(masterlistPath, url_, oldBranch_));

  EXPECT_FALSE(db_->IsLatestMasterlist(masterlistPath, branch_));
}

TEST_P(
    DatabaseInterfaceTest,
    isLatestMasterlistShouldReturnTrueIfTheCurrentRevisionIsTheLatestRevisioninTheGivenBranch) {
  ASSERT_NO_THROW(db_->UpdateMasterlist(masterlistPath, url_, branch_));

  EXPECT_TRUE(db_->IsLatestMasterlist(masterlistPath, branch_));
}

TEST_P(DatabaseInterfaceTest,
       getGroupsShouldReturnAllGroupsListedInTheLoadedMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto groups = db_->GetGroups();

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
  auto groups = db_->GetGroups();

  ASSERT_EQ(1, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());
}

TEST_P(DatabaseInterfaceTest,
       getGroupsShouldNotIncludeUserlistMetadataIfParameterIsFalse) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto groups = db_->GetGroups(false);

  ASSERT_EQ(3, groups.size());

  EXPECT_EQ("default", groups[0].GetName());
  EXPECT_TRUE(groups[0].GetAfterGroups().empty());

  EXPECT_EQ("group1", groups[1].GetName());
  EXPECT_TRUE(groups[1].GetAfterGroups().empty());

  EXPECT_EQ("group2", groups[2].GetName());
  EXPECT_EQ(std::vector<std::string>({"group1"}),
            groups[2].GetAfterGroups());
}

TEST_P(
    DatabaseInterfaceTest,
    getGroupsShouldReturnDefaultGroupIfNoMasterlistIsLoadedAndUserlistMetadataIsNotIncluded) {
  auto groups = db_->GetGroups(false);

  EXPECT_EQ(1, groups.size());

  EXPECT_EQ("default", groups.begin()->GetName());
  EXPECT_TRUE(groups.begin()->GetAfterGroups().empty());
}

TEST_P(DatabaseInterfaceTest,
       getUserGroupsShouldReturnOnlyGroupMetadataFromTheUserlist) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());

  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto groups = db_->GetUserGroups();

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

  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  db_->SetUserGroups(std::vector<Group>({
      Group("group4"),
  }));

  auto groups = db_->GetUserGroups();

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

  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto path = db_->GetGroupsPath("group1", "group3");

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

  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto tags = db_->GetKnownBashTags();

  std::set<std::string> expectedTags({
      "RaceRelations",
      "C.Lighting",
      "Actors.ACBS",
      "C.Climate",
  });
  EXPECT_EQ(expectedTags, tags);
}

TEST_P(DatabaseInterfaceTest,
       getGeneralMessagesShouldGetGeneralMessagesFromTheMasterlistAndUserlist) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto messages = db_->GetGeneralMessages();

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
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, ""));

  auto messages = db_->GetGeneralMessages(true);

  EXPECT_TRUE(messages.empty());
}

TEST_P(DatabaseInterfaceTest,
       getPluginMetadataShouldReturnAnEmptyOptionalIfThePluginHasNoMetadata) {
  EXPECT_FALSE(db_->GetPluginMetadata(blankEsm));
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginMetadataShouldReturnMergedMasterAndUserMetadataForTheGivenPluginIfIncludeUserMetadataIsTrue) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto metadata = db_->GetPluginMetadata(blankEsm, true).value();

  std::set<File> expectedLoadAfter({
      File(masterFile),
      File(blankDifferentEsm),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(DatabaseInterfaceTest,
       getPluginMetadataShouldPreferUserMetadataWhenMergingMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto metadata = db_->GetPluginMetadata(blankEsm, true).value();

  std::set<Tag> expectedTags({
      Tag("Actors.ACBS"),
      Tag("Actors.ACBS", true, "file(\"" + missingEsp + "\")"),
      Tag("Actors.AIData"),
      Tag("C.Water", false),
  });
  EXPECT_EQ(expectedTags, metadata.GetTags());
  EXPECT_EQ("file(\"" + missingEsp + "\")",
            metadata.GetTags()
                .find(Tag("Actors.ACBS", true, "file(\"" + missingEsp + "\")"))
                ->GetCondition());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginMetadataShouldReturnOnlyMasterlistMetadataForTheGivenPluginIfIncludeUserMetadataIsFalse) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto metadata = db_->GetPluginMetadata(blankEsm, false).value();

  std::set<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginMetadataShouldReturnOnlyValidMetadataForTheGivenPluginIfConditionsAreEvaluated) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, ""));

  auto metadata = db_->GetPluginMetadata(blankEsm, false, true).value();

  EXPECT_TRUE(metadata.GetMessages().empty());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginUserMetadataShouldReturnAnEmptyPluginMetadataObjectIfThePluginHasNoUserMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  EXPECT_FALSE(db_->GetPluginUserMetadata(blankDifferentEsm));
}

TEST_P(DatabaseInterfaceTest,
       getPluginUserMetadataShouldReturnOnlyUserMetadataForTheGivenPlugin) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto metadata = db_->GetPluginUserMetadata(blankEsm).value();

  std::set<File> expectedLoadAfter({
      File(blankDifferentEsm),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(
    DatabaseInterfaceTest,
    getPluginUserMetadataShouldReturnOnlyValidMetadataForTheGivenPluginIfConditionsAreEvaluated) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  auto metadata = db_->GetPluginMetadata(blankEsm, false, true).value();

  EXPECT_TRUE(metadata.GetMessages().empty());
}

TEST_P(
    DatabaseInterfaceTest,
    setPluginUserMetadataShouldReplaceExistingUserMetadataWithTheGivenMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  PluginMetadata newMetadata(blankDifferentEsp);
  newMetadata.SetRequirements(std::set<File>({File(masterFile)}));

  db_->SetPluginUserMetadata(newMetadata);

  auto metadata = db_->GetPluginUserMetadata(blankDifferentEsp).value();

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
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  PluginMetadata newMetadata(blankEsm);
  newMetadata.SetRequirements(std::set<File>({File(masterFile)}));

  db_->SetPluginUserMetadata(newMetadata);

  auto metadata = db_->GetPluginMetadata(blankEsm).value();

  std::set<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldDiscardAllUserMetadataForTheGivenPlugin) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  db_->DiscardPluginUserMetadata(blankEsm);

  EXPECT_FALSE(db_->GetPluginUserMetadata(blankEsm));
}

TEST_P(
    DatabaseInterfaceTest,
    discardPluginUserMetadataShouldNotDiscardMasterlistMetadataForTheGivenPlugin) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  db_->DiscardPluginUserMetadata(blankEsm);

  auto metadata = db_->GetPluginMetadata(blankEsm).value();

  std::set<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldNotDiscardUserMetadataForOtherPlugins) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  db_->DiscardPluginUserMetadata(blankEsm);

  auto metadata = db_->GetPluginUserMetadata(blankDifferentEsp);

  EXPECT_TRUE(metadata);
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldNotDiscardGeneralMessages) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  db_->DiscardPluginUserMetadata(blankEsm);

  auto messages = db_->GetGeneralMessages();

  std::vector<Message> expectedMessages({
      Message(MessageType::say,
              generalMasterlistMessage, "file(\"" + missingEsp + "\")"),
      Message(MessageType::say, generalUserlistMessage),
  });
  EXPECT_EQ(expectedMessages, messages);
}

TEST_P(DatabaseInterfaceTest,
       discardPluginUserMetadataShouldNotDiscardKnownBashTags) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  db_->DiscardPluginUserMetadata(blankEsm);

  auto tags = db_->GetKnownBashTags();

  std::set<std::string> expectedTags({
      "RaceRelations",
      "C.Lighting",
      "Actors.ACBS",
      "C.Climate",
  });
  EXPECT_EQ(expectedTags, tags);
}

TEST_P(
    DatabaseInterfaceTest,
    discardAllUserMetadataShouldDiscardAllUserMetadataAndNoMasterlistMetadata) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(GenerateUserlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, userlistPath_));

  db_->DiscardAllUserMetadata();

  EXPECT_FALSE(db_->GetPluginUserMetadata(blankEsm));
  EXPECT_FALSE(db_->GetPluginUserMetadata(blankDifferentEsp));

  auto metadata = db_->GetPluginMetadata(blankEsm).value();

  std::set<File> expectedLoadAfter({
      File(masterFile),
  });
  EXPECT_EQ(expectedLoadAfter, metadata.GetLoadAfterFiles());

  auto messages = db_->GetGeneralMessages();

  std::vector<Message> expectedMessages({
      Message(MessageType::say,
              generalMasterlistMessage,
              "file(\"" + missingEsp + "\")"),
  });
  EXPECT_EQ(expectedMessages, messages);

  auto tags = db_->GetKnownBashTags();

  std::set<std::string> expectedTags({
      "Actors.ACBS",
      "C.Climate",
  });
  EXPECT_EQ(expectedTags, tags);
}

TEST_P(DatabaseInterfaceTest,
       writeMinimalListShouldReturnOkAndWriteToFileIfArgumentsGivenAreValid) {
  EXPECT_NO_THROW(db_->WriteMinimalList(minimalOutputPath_, false));
  EXPECT_TRUE(std::filesystem::exists(minimalOutputPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeMinimalListShouldThrowIfTheFileAlreadyExistsAndTheOverwriteArgumentIsFalse) {
  ASSERT_NO_THROW(db_->WriteMinimalList(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_THROW(db_->WriteMinimalList(minimalOutputPath_, false),
               FileAccessError);
}

TEST_P(
    DatabaseInterfaceTest,
    writeMinimalListShouldReturnOkAndWriteToFileIfTheArgumentsAreValidAndTheOverwriteArgumentIsTrue) {
  EXPECT_NO_THROW(db_->WriteMinimalList(minimalOutputPath_, true));
  EXPECT_TRUE(std::filesystem::exists(minimalOutputPath_));
}

TEST_P(
    DatabaseInterfaceTest,
    writeMinimalListShouldReturnOkIfTheFileAlreadyExistsAndTheOverwriteArgumentIsTrue) {
  ASSERT_NO_THROW(db_->WriteMinimalList(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  EXPECT_NO_THROW(db_->WriteMinimalList(minimalOutputPath_, true));
}

TEST_P(DatabaseInterfaceTest,
       writeMinimalListShouldThrowIfPathGivenExistsAndIsReadOnly) {
  ASSERT_NO_THROW(db_->WriteMinimalList(minimalOutputPath_, false));
  ASSERT_TRUE(std::filesystem::exists(minimalOutputPath_));

  std::filesystem::permissions(minimalOutputPath_,
                               std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::replace);

  EXPECT_THROW(db_->WriteMinimalList(minimalOutputPath_, true),
               FileAccessError);
}

TEST_P(DatabaseInterfaceTest,
       writeMinimalListShouldWriteOnlyBashTagsAndDirtyInfo) {
  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(db_->LoadLists(masterlistPath, ""));

  EXPECT_NO_THROW(db_->WriteMinimalList(minimalOutputPath_, true));

  EXPECT_EQ(GetExpectedMinimalContent(), GetFileContent(minimalOutputPath_));
}
}
}

#endif
