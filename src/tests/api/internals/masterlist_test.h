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

#ifndef LOOT_TESTS_API_INTERNALS_MASTERLIST_TEST
#define LOOT_TESTS_API_INTERNALS_MASTERLIST_TEST

#include "api/masterlist.h"

#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class MasterlistTest : public CommonGameTestFixture {
protected:
  MasterlistTest() :
      repoBranch("master"),
      oldBranch("old-branch"),
      repoPath("./testing-metadata.git"),
      masterlistPath(localPath / "masterlist.yaml"),
      nonAsciiMasterlistPath(
          localPath / std::filesystem::u8path(u8"masterl\u00EDst.yaml")) {}

  void SetUp() {
    CommonGameTestFixture::SetUp();

    auto sourceDirectory = getSourceMetadataFilesPath();
    std::filesystem::copy(sourceDirectory / "masterlist.yaml",
                          metadataFilesPath / "masterlist.yaml");
    ASSERT_TRUE(std::filesystem::exists(metadataFilesPath / "masterlist.yaml"));

    ASSERT_FALSE(std::filesystem::exists(masterlistPath));
    ASSERT_FALSE(std::filesystem::exists(nonAsciiMasterlistPath));
    ASSERT_FALSE(std::filesystem::exists(localPath / ".git"));
  }

  void runRepoCommand(const std::string& command) {
    auto testPath = std::filesystem::current_path();
    std::filesystem::current_path(masterlistPath.parent_path());
    system(command.c_str());
    std::filesystem::current_path(testPath);
  }

  const std::string repoPath;
  const std::string repoBranch;
  const std::string oldBranch;

  const std::filesystem::path masterlistPath;
  const std::filesystem::path nonAsciiMasterlistPath;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        MasterlistTest,
                        ::testing::Values(GameType::tes4,
                                          GameType::tes5,
                                          GameType::fo3,
                                          GameType::fonv,
                                          GameType::fo4,
                                          GameType::tes5se));

TEST_P(MasterlistTest, updateShouldThrowIfAnInvalidPathIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update("//\?", repoPath, repoBranch),
               std::system_error);
}

TEST_P(MasterlistTest, updateShouldThrowIfABlankPathIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update("", repoPath, repoBranch),
               std::invalid_argument);
}

TEST_P(MasterlistTest, updateShouldThrowIfABranchThatDoesNotExistIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update(masterlistPath, repoPath, "missing-branch"),
               std::system_error);
}

TEST_P(MasterlistTest, updateShouldThrowIfABlankBranchIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update(masterlistPath, repoPath, ""),
               std::invalid_argument);
}

TEST_P(MasterlistTest, updateShouldThrowIfAUrlThatDoesNotExistIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update(masterlistPath,
                                 "https://github.com/loot/does-not-exist.git",
                                 repoBranch),
               std::system_error);
}

TEST_P(MasterlistTest, updateShouldThrowIfABlankUrlIsGiven) {
  Masterlist masterlist;
  EXPECT_THROW(masterlist.Update(masterlistPath, "", repoBranch),
               std::invalid_argument);
}

TEST_P(MasterlistTest, updateShouldBeAbleToCloneAGitHubRepository) {
  Masterlist masterlist;

  EXPECT_NO_THROW(
      masterlist.Update(masterlistPath,
                        "https://github.com/loot/testing-metadata.git",
                        repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(MasterlistTest, updateShouldBeAbleToCloneALocalRepository) {
  Masterlist masterlist;

  EXPECT_NO_THROW(masterlist.Update(masterlistPath, repoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(MasterlistTest, updateShouldReturnTrueIfNoMasterlistExists) {
  Masterlist masterlist;
  EXPECT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(MasterlistTest, updateShouldReturnFalseIfAnUpToDateMasterlistExists) {
  Masterlist masterlist;

  EXPECT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));

  EXPECT_FALSE(masterlist.Update(masterlistPath, repoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(
    MasterlistTest,
    updateShouldReturnFalseIfAnUpToDateMasterlistWithANonAsciiFilenameExists) {
  Masterlist masterlist;

  EXPECT_TRUE(masterlist.Update(nonAsciiMasterlistPath, repoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(nonAsciiMasterlistPath));

  EXPECT_FALSE(masterlist.Update(nonAsciiMasterlistPath, repoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(nonAsciiMasterlistPath));
}

TEST_P(MasterlistTest,
       updateShouldDiscardLocalHistoryIfRemoteHistoryIsDifferent) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));

  runRepoCommand("git config commit.gpgsign false");
  runRepoCommand("git commit --amend -m \"changing local history\"");

  EXPECT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(MasterlistTest, getInfoShouldThrowIfNoMasterlistExistsAtTheGivenPath) {
  Masterlist masterlist;
  EXPECT_THROW(masterlist.GetInfo(masterlistPath, false), FileAccessError);
}

TEST_P(MasterlistTest,
       getInfoShouldThrowIfTheGivenPathDoesNotBelongToAGitRepository) {
  ASSERT_NO_THROW(std::filesystem::copy(metadataFilesPath / "masterlist.yaml",
                                        masterlistPath));

  Masterlist masterlist;
  EXPECT_THROW(masterlist.GetInfo(masterlistPath, false), GitStateError);
}

TEST_P(
    MasterlistTest,
    getInfoShouldReturnRevisionAndDateStringsOfTheCorrectLengthsWhenRequestingALongId) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));

  MasterlistInfo info = masterlist.GetInfo(masterlistPath, false);
  EXPECT_EQ(40, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(
    MasterlistTest,
    getInfoShouldReturnRevisionAndDateStringsOfTheCorrectLengthsWhenRequestingAShortId) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));

  MasterlistInfo info = masterlist.GetInfo(masterlistPath, true);
  EXPECT_GE((unsigned)40, info.revision_id.length());
  EXPECT_LE((unsigned)7, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(
    MasterlistTest,
    getInfoShouldAppendSuffixesToReturnedStringsIfTheMasterlistHasBeenEdited) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));
  std::ofstream out(masterlistPath);
  out.close();

  MasterlistInfo info = masterlist.GetInfo(masterlistPath, false);
  EXPECT_EQ(40, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_TRUE(info.is_modified);
}

TEST_P(MasterlistTest,
       getInfoShouldDetectWhenAMasterlistWithANonAsciiFilenameHasBeenEdited) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));

  auto nonAsciiPath = masterlistPath.parent_path() /
                      std::filesystem::u8path(u8"non\u00C1scii.yaml");
  std::filesystem::copy_file(masterlistPath, nonAsciiPath);

  runRepoCommand("git add " + nonAsciiPath.string());
  std::ofstream out(nonAsciiPath);
  out.close();

  MasterlistInfo info = masterlist.GetInfo(nonAsciiPath, false);
  EXPECT_EQ(40, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_TRUE(info.is_modified);
}

TEST_P(MasterlistTest,
       isLatestShouldThrowIfTheGivenPathDoesNotBelongToAGitRepository) {
  ASSERT_NO_THROW(std::filesystem::copy(metadataFilesPath / "masterlist.yaml",
                                        masterlistPath));

  EXPECT_THROW(Masterlist::IsLatest(masterlistPath, repoBranch), GitStateError);
}

TEST_P(MasterlistTest, isLatestShouldThrowIfTheGivenBranchIsAnEmptyString) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));

  EXPECT_THROW(Masterlist::IsLatest(masterlistPath, ""), std::invalid_argument);
}

TEST_P(
    MasterlistTest,
    isLatestShouldReturnFalseIfTheCurrentRevisionIsNotTheLatestRevisionInTheGivenBranch) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, oldBranch));

  EXPECT_FALSE(Masterlist::IsLatest(masterlistPath, repoBranch));
}

TEST_P(
    MasterlistTest,
    isLatestShouldReturnTrueIfTheCurrentRevisionIsTheLatestRevisioninTheGivenBranch) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath, repoPath, repoBranch));

  EXPECT_TRUE(Masterlist::IsLatest(masterlistPath, repoBranch));
}
}
}

#endif
