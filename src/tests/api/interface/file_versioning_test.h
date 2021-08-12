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

#ifndef LOOT_TESTS_API_INTERFACE_FILE_VERSIONING_TEST
#define LOOT_TESTS_API_INTERFACE_FILE_VERSIONING_TEST

#include "loot/api.h"
#include "tests/api/interface/api_game_operations_test.h"

namespace loot {
namespace test {
class FileVersioningTest : public ApiGameOperationsTest {
protected:
  FileVersioningTest() :
      url_("./testing-metadata.git"),
      branch_("master"),
      oldBranch_("old-branch") {}

  const std::string url_;
  const std::string branch_;
  const std::string oldBranch_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         FileVersioningTest,
                         ::testing::Values(GameType::tes4,
                                           GameType::tes5,
                                           GameType::fo3,
                                           GameType::fonv,
                                           GameType::fo4,
                                           GameType::tes5se));

TEST_P(FileVersioningTest,
       updateFileShouldThrowIfTheMasterlistPathGivenIsInvalid) {
  EXPECT_THROW(UpdateFile("//\?", url_, branch_), std::exception);
}

TEST_P(FileVersioningTest,
       updateFileShouldThrowIfTheMasterlistPathGivenIsEmpty) {
  EXPECT_THROW(UpdateFile("", url_, branch_), std::invalid_argument);
}

TEST_P(FileVersioningTest,
       updateFileShouldThrowIfTheRepositoryUrlGivenCannotBeFound) {
  EXPECT_THROW(UpdateFile(
                   masterlistPath,
                   "https://github.com/loot/oblivion-does-not-exist.git",
                   branch_),
               std::system_error);
}

TEST_P(FileVersioningTest,
       updateFileShouldThrowIfTheRepositoryUrlGivenIsEmpty) {
  EXPECT_THROW(UpdateFile(masterlistPath, "", branch_),
               std::invalid_argument);
}

TEST_P(FileVersioningTest,
       updateFileShouldThrowIfTheRepositoryBranchGivenCannotBeFound) {
  EXPECT_THROW(UpdateFile(masterlistPath, url_, "missing-branch"),
               std::system_error);
}

TEST_P(FileVersioningTest,
       updateFileShouldThrowIfTheRepositoryBranchGivenIsEmpty) {
  EXPECT_THROW(UpdateFile(masterlistPath, url_, ""),
               std::invalid_argument);
}

TEST_P(
    FileVersioningTest,
    updateFileShouldSucceedIfPassedValidParametersAndOutputTrueIfTheMasterlistWasUpdated) {
  bool updated = false;
  EXPECT_NO_THROW(updated =
                      UpdateFile(masterlistPath, url_, branch_));
  EXPECT_TRUE(updated);
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(
    FileVersioningTest,
    updateFileShouldSucceedIfCalledRepeatedlyButOnlyOutputTrueForTheFirstCall) {
  bool updated = false;
  EXPECT_NO_THROW(updated =
                      UpdateFile(masterlistPath, url_, branch_));
  EXPECT_TRUE(updated);

  EXPECT_NO_THROW(updated =
                      UpdateFile(masterlistPath, url_, branch_));
  EXPECT_FALSE(updated);
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_P(FileVersioningTest,
       getFileRevisionShouldThrowIfNoMasterlistIsPresent) {
  FileRevision revision;
  EXPECT_THROW(revision = GetFileRevision(masterlistPath, false),
               FileAccessError);
  EXPECT_TRUE(revision.id.empty());
  EXPECT_TRUE(revision.date.empty());
  EXPECT_FALSE(revision.is_modified);
}

TEST_P(
    FileVersioningTest,
    getFileRevisionShouldThrowIfANonVersionControlledMasterlistIsPresent) {
  ASSERT_NO_THROW(GenerateMasterlist());

  FileRevision revision;
  EXPECT_THROW(revision = GetFileRevision(masterlistPath, false),
               GitStateError);
  EXPECT_TRUE(revision.id.empty());
  EXPECT_TRUE(revision.date.empty());
  EXPECT_FALSE(revision.is_modified);
}

TEST_P(
    FileVersioningTest,
    getFileRevisionShouldOutputLongStringsAndBooleanFalseIfAVersionControlledMasterlistIsPresentAndGetShortIdParameterIsFalse) {
  ASSERT_NO_THROW(UpdateFile(masterlistPath, url_, branch_));

  FileRevision revision;
  EXPECT_NO_THROW(revision = GetFileRevision(masterlistPath, false));
  EXPECT_EQ(40, revision.id.length());
  EXPECT_EQ(10, revision.date.length());
  EXPECT_FALSE(revision.is_modified);
}

TEST_P(
    FileVersioningTest,
    getFileRevisionShouldOutputShortStringsAndBooleanFalseIfAVersionControlledMasterlistIsPresentAndGetShortIdParameterIsTrue) {
  ASSERT_NO_THROW(UpdateFile(masterlistPath, url_, branch_));

  FileRevision revision;
  EXPECT_NO_THROW(revision = GetFileRevision(masterlistPath, false));
  EXPECT_GE(size_t(40), revision.id.length());
  EXPECT_LE(size_t(7), revision.id.length());
  EXPECT_EQ(10, revision.date.length());
  EXPECT_FALSE(revision.is_modified);
}

TEST_P(
    FileVersioningTest,
    getFileRevisionShouldSucceedIfAnEditedVersionControlledMasterlistIsPresent) {
  ASSERT_NO_THROW(UpdateFile(masterlistPath, url_, branch_));
  ASSERT_NO_THROW(GenerateMasterlist());

  FileRevision revision;
  EXPECT_NO_THROW(revision = GetFileRevision(masterlistPath, false));
  EXPECT_EQ(40, revision.id.length());
  EXPECT_EQ(10, revision.date.length());
  EXPECT_TRUE(revision.is_modified);
}

TEST_P(
    FileVersioningTest,
    isLatestFileShouldReturnFalseIfTheCurrentRevisionIsNotTheLatestRevisionInTheGivenBranch) {
  ASSERT_NO_THROW(UpdateFile(masterlistPath, url_, oldBranch_));

  EXPECT_FALSE(IsLatestFile(masterlistPath, branch_));
}

TEST_P(
    FileVersioningTest,
    isLatestFileShouldReturnTrueIfTheCurrentRevisionIsTheLatestRevisioninTheGivenBranch) {
  ASSERT_NO_THROW(UpdateFile(masterlistPath, url_, branch_));

  EXPECT_TRUE(IsLatestFile(masterlistPath, branch_));
}
}
}

#endif
