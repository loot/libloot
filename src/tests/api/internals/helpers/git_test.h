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

#ifndef LOOT_TESTS_API_INTERNALS_HELPERS_GIT_TEST
#define LOOT_TESTS_API_INTERNALS_HELPERS_GIT_TEST

#include "api/helpers/git.h"

#include <gtest/gtest.h>

#include "loot/exception/git_state_error.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class GitHelperTest : public ::testing::Test {
protected:
  GitHelperTest() :
    rootTestPath(getRootTestPath()),
    repoRoot(rootTestPath / "testing-metadata"),
    repoSubdirectory(repoRoot / "invalid"),
    changedFile("LICENSE"),
    unchangedFile("README.md"),
    untrackedFile("untracked.txt") {}

  inline void SetUp() {
    using std::filesystem::exists;

    copy(std::filesystem::absolute("./testing-metadata"), repoRoot);
    ASSERT_TRUE(exists(repoRoot));
    ASSERT_TRUE(exists(repoSubdirectory));
    ASSERT_TRUE(std::filesystem::exists(repoRoot / unchangedFile));

    // Run git reset --hard to ensure there are no changes in the working copy.
    // The initial checkout can detect changes due to line ending mismatch.
    auto currentPath = std::filesystem::current_path();
    std::filesystem::current_path(repoRoot);
    system("git reset --hard");
    std::filesystem::current_path(currentPath);

    // Edit a tracked file
    std::ofstream changedOut(repoRoot / changedFile);
    changedOut.close();
    ASSERT_TRUE(exists(repoRoot / changedFile));

    // Create a new file in the repository
    std::ofstream untrackedOut(repoRoot / untrackedFile);
    untrackedOut.close();
    ASSERT_TRUE(exists(repoRoot / untrackedFile));

    // Create a new file outside the repository
    std::ofstream outOfRepoOut(rootTestPath / untrackedFile);
    outOfRepoOut.close();
    ASSERT_TRUE(exists(rootTestPath / untrackedFile));
  }

  inline void TearDown() {
    // Grant write permissions to everything in rootTestPath
    // in case the test made anything read only.
    for (const auto& path : std::filesystem::recursive_directory_iterator(rootTestPath)) {
      std::filesystem::permissions(path, std::filesystem::perms::all);
    }
    std::filesystem::remove_all(rootTestPath);
  }

  GitHelper git_;

  const std::filesystem::path rootTestPath;
  const std::filesystem::path repoRoot;
  const std::filesystem::path repoSubdirectory;

  const std::string changedFile;
  const std::string unchangedFile;
  const std::string untrackedFile;

private:
  void copy(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (std::filesystem::is_directory(from)) {
      std::filesystem::create_directories(to);
      for (auto entry : std::filesystem::directory_iterator(from)) {
        copy(entry.path(), to / entry.path().filename());
      }
    } else {
      std::filesystem::copy(from, to);
    }
  }
};

TEST_F(GitHelperTest, destructorShouldCallLibgit2CleanupFunction) {
  ASSERT_EQ(2, git_libgit2_init());

  GitHelper* gitPointer = new GitHelper();
  ASSERT_EQ(4, git_libgit2_init());

  delete gitPointer;
  EXPECT_EQ(2, git_libgit2_shutdown());
}

TEST_F(GitHelperTest, isRepositoryShouldReturnTrueForARepositoryRoot) {
  EXPECT_TRUE(GitHelper::IsRepository(repoRoot));
}

TEST_F(GitHelperTest, isRepositoryShouldReturnFalseForRepositorySubdirectory) {
  EXPECT_FALSE(GitHelper::IsRepository(repoSubdirectory));
}

TEST_F(GitHelperTest, isFileDifferentShouldThrowIfGivenANonRepositoryPath) {
  EXPECT_THROW(GitHelper::IsFileDifferent(rootTestPath, untrackedFile),
               GitStateError);
}

TEST_F(GitHelperTest, isFileDifferentShouldReturnFalseForAnUntrackedFile) {
  // New files not in the index are not tracked by Git, so aren't considered
  // different.
  EXPECT_FALSE(GitHelper::IsFileDifferent(repoRoot, untrackedFile));
}

TEST_F(GitHelperTest,
       isFileDifferentShouldReturnFalseForAnUnchangedTrackedFile) {
  EXPECT_FALSE(GitHelper::IsFileDifferent(repoRoot, unchangedFile));
}

TEST_F(GitHelperTest,
       isFileDifferentShouldReturnTrueForAChangedTrackedFile) {
  EXPECT_TRUE(GitHelper::IsFileDifferent(repoRoot, changedFile));
}

class GitTest : public ::testing::Test {
protected:
  GitTest() :
      repoBranch("master"),
      oldBranch("old-branch"),
      remoteRepoPath("./testing-metadata.git"),
      rootTestPath(getRootTestPath()),
      metadataFilesPath(rootTestPath / "metadata"),
      masterlistPath(rootTestPath / "masterlist.yaml"),
      nonAsciiMasterlistPath(
          rootTestPath / std::filesystem::u8path(u8"masterl\u00EDst.yaml")) {}

  void SetUp() {
    create_directories(metadataFilesPath);
    ASSERT_TRUE(exists(metadataFilesPath));

    auto sourceDirectory = std::filesystem::absolute("./testing-metadata");
    std::filesystem::copy(sourceDirectory / "masterlist.yaml",
                          metadataFilesPath / "masterlist.yaml");
    ASSERT_TRUE(std::filesystem::exists(metadataFilesPath / "masterlist.yaml"));

    ASSERT_FALSE(std::filesystem::exists(masterlistPath));
    ASSERT_FALSE(std::filesystem::exists(nonAsciiMasterlistPath));
    ASSERT_FALSE(std::filesystem::exists(rootTestPath / ".git"));
  }

  void TearDown() {
    // Grant write permissions to everything in rootTestPath
    // in case the test made anything read only.
    for (const auto& path :
         std::filesystem::recursive_directory_iterator(rootTestPath)) {
      std::filesystem::permissions(path, std::filesystem::perms::all);
    }
    std::filesystem::remove_all(rootTestPath);
  }

  void runRepoCommand(const std::string& command) {
    auto testPath = std::filesystem::current_path();
    std::filesystem::current_path(masterlistPath.parent_path());
    system(command.c_str());
    std::filesystem::current_path(testPath);
  }

  const std::string remoteRepoPath;
  const std::string repoBranch;
  const std::string oldBranch;

  const std::filesystem::path rootTestPath;
  const std::filesystem::path metadataFilesPath;
  const std::filesystem::path masterlistPath;
  const std::filesystem::path nonAsciiMasterlistPath;
};

TEST_F(GitTest, updateFileShouldThrowIfAnInvalidPathIsGiven) {
  EXPECT_THROW(git::UpdateFile("//\?", remoteRepoPath, repoBranch),
               std::system_error);
}

TEST_F(GitTest, updateFileShouldThrowIfABlankPathIsGiven) {
  EXPECT_THROW(git::UpdateFile("", remoteRepoPath, repoBranch),
               std::invalid_argument);
}

TEST_F(GitTest, updateFileShouldThrowIfABranchThatDoesNotExistIsGiven) {
  EXPECT_THROW(git::UpdateFile(masterlistPath, remoteRepoPath, "missing-branch"),
               std::system_error);
}

TEST_F(GitTest, updateFileShouldThrowIfABlankBranchIsGiven) {
  EXPECT_THROW(git::UpdateFile(masterlistPath, remoteRepoPath, ""),
               std::invalid_argument);
}

TEST_F(GitTest, updateFileShouldThrowIfAUrlThatDoesNotExistIsGiven) {
  EXPECT_THROW(git::UpdateFile(masterlistPath,
                                 "https://github.com/loot/does-not-exist.git",
                                 repoBranch),
               std::system_error);
}

TEST_F(GitTest, updateFileShouldThrowIfABlankUrlIsGiven) {
  EXPECT_THROW(git::UpdateFile(masterlistPath, "", repoBranch),
               std::invalid_argument);
}

TEST_F(GitTest, updateFileShouldBeAbleToCloneAGitHubRepository) {
  EXPECT_NO_THROW(
      git::UpdateFile(masterlistPath,
                        "https://github.com/loot/testing-metadata.git",
                        repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_F(GitTest, updateFileShouldBeAbleToCloneALocalRepository) {
  EXPECT_NO_THROW(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_F(GitTest, updateFileShouldReturnTrueIfNoMasterlistExists) {
  EXPECT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_F(GitTest, updateFileShouldReturnFalseIfAnUpToDateMasterlistExists) {
  EXPECT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));

  EXPECT_FALSE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_F(
    GitTest,
    updateFileShouldReturnFalseIfAnUpToDateMasterlistWithANonAsciiFilenameExists) {
  EXPECT_TRUE(git::UpdateFile(nonAsciiMasterlistPath, remoteRepoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(nonAsciiMasterlistPath));

  EXPECT_FALSE(git::UpdateFile(nonAsciiMasterlistPath, remoteRepoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(nonAsciiMasterlistPath));
}

TEST_F(GitTest,
       updateFileShouldDiscardLocalHistoryIfRemoteHistoryIsDifferent) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));

  runRepoCommand("git config commit.gpgsign false");
  runRepoCommand("git commit --amend -m \"changing local history\"");

  EXPECT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));
  EXPECT_TRUE(std::filesystem::exists(masterlistPath));
}

TEST_F(GitTest, getVersionInfoShouldThrowIfNoMasterlistExistsAtTheGivenPath) {
  EXPECT_THROW(git::GetVersionInfo(masterlistPath, false), FileAccessError);
}

TEST_F(GitTest,
       getVersionInfoShouldThrowIfTheGivenPathDoesNotBelongToAGitRepository) {
  ASSERT_NO_THROW(std::filesystem::copy(metadataFilesPath / "masterlist.yaml",
                                        masterlistPath));
  EXPECT_THROW(git::GetVersionInfo(masterlistPath, false), GitStateError);
}

TEST_F(
    GitTest,
    getVersionInfoShouldReturnRevisionAndDateStringsOfTheCorrectLengthsWhenRequestingALongId) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));

  FileRevision revision = git::GetVersionInfo(masterlistPath, false);
  EXPECT_EQ(40, revision.id.length());
  EXPECT_EQ(10, revision.date.length());
  EXPECT_FALSE(revision.is_modified);
}

TEST_F(
    GitTest,
    getVersionInfoShouldReturnRevisionAndDateStringsOfTheCorrectLengthsWhenRequestingAShortId) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));

  FileRevision revision = git::GetVersionInfo(masterlistPath, true);
  EXPECT_GE((unsigned)40, revision.id.length());
  EXPECT_LE((unsigned)7, revision.id.length());
  EXPECT_EQ(10, revision.date.length());
  EXPECT_FALSE(revision.is_modified);
}

TEST_F(
    GitTest,
    getVersionInfoShouldAppendSuffixesToReturnedStringsIfTheMasterlistHasBeenEdited) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));
  std::ofstream out(masterlistPath);
  out.close();

  FileRevision revision = git::GetVersionInfo(masterlistPath, false);
  EXPECT_EQ(40, revision.id.length());
  EXPECT_EQ(10, revision.date.length());
  EXPECT_TRUE(revision.is_modified);
}

TEST_F(GitTest,
       getVersionInfoShouldDetectWhenAMasterlistWithANonAsciiFilenameHasBeenEdited) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));

  auto nonAsciiPath = masterlistPath.parent_path() /
                      std::filesystem::u8path(u8"non\u00C1scii.yaml");
  std::filesystem::copy_file(masterlistPath, nonAsciiPath);

  runRepoCommand("git add " + nonAsciiPath.string());
  std::ofstream out(nonAsciiPath);
  out.close();

  FileRevision revision = git::GetVersionInfo(nonAsciiPath, false);
  EXPECT_EQ(40, revision.id.length());
  EXPECT_EQ(10, revision.date.length());
  EXPECT_TRUE(revision.is_modified);
}

TEST_F(GitTest,
       isLatestShouldThrowIfTheGivenPathDoesNotBelongToAGitRepository) {
  ASSERT_NO_THROW(std::filesystem::copy(metadataFilesPath / "masterlist.yaml",
                                        masterlistPath));

  EXPECT_THROW(git::IsLatest(masterlistPath, repoBranch), GitStateError);
}

TEST_F(GitTest, isLatestShouldThrowIfTheGivenBranchIsAnEmptyString) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));

  EXPECT_THROW(git::IsLatest(masterlistPath, ""), std::invalid_argument);
}

TEST_F(
    GitTest,
    isLatestShouldReturnFalseIfTheCurrentRevisionIsNotTheLatestRevisionInTheGivenBranch) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, oldBranch));

  EXPECT_FALSE(git::IsLatest(masterlistPath, repoBranch));
}

TEST_F(
    GitTest,
    isLatestShouldReturnTrueIfTheCurrentRevisionIsTheLatestRevisioninTheGivenBranch) {
  ASSERT_TRUE(git::UpdateFile(masterlistPath, remoteRepoPath, repoBranch));

  EXPECT_TRUE(git::IsLatest(masterlistPath, repoBranch));
}
}
}

#endif
