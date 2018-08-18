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

#ifndef LOOT_TESTS_API_INTERNALS_HELPERS_GIT_HELPER_TEST
#define LOOT_TESTS_API_INTERNALS_HELPERS_GIT_HELPER_TEST

#include "api/helpers/git_helper.h"

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
}
}

#endif
