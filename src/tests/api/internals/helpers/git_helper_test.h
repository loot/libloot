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
    using boost::filesystem::exists;

    copy(boost::filesystem::absolute("./testing-metadata"), repoRoot);
    ASSERT_TRUE(exists(repoRoot));
    ASSERT_TRUE(exists(repoSubdirectory));
    ASSERT_TRUE(boost::filesystem::exists(repoRoot / unchangedFile));

    // Run git reset --hard to ensure there are no changes in the working copy.
    // The initial checkout can detect changes due to line ending mismatch.
    auto currentPath = boost::filesystem::current_path();
    boost::filesystem::current_path(repoRoot);
    system("git reset --hard");
    boost::filesystem::current_path(currentPath);

    // Edit a tracked file
    boost::filesystem::ofstream changedOut(repoRoot / changedFile);
    changedOut.close();
    ASSERT_TRUE(exists(repoRoot / changedFile));

    // Create a new file in the repository
    boost::filesystem::ofstream untrackedOut(repoRoot / untrackedFile);
    untrackedOut.close();
    ASSERT_TRUE(exists(repoRoot / untrackedFile));

    // Create a new file outside the repository
    boost::filesystem::ofstream outOfRepoOut(rootTestPath / untrackedFile);
    outOfRepoOut.close();
    ASSERT_TRUE(exists(rootTestPath / untrackedFile));
  }

  inline void TearDown() {
    // Grant write permissions to everything in rootTestPath
    // in case the test made anything read only.
    for (const auto& path : boost::filesystem::recursive_directory_iterator(rootTestPath)) {
      boost::filesystem::permissions(path, boost::filesystem::perms::all_all | boost::filesystem::perms::add_perms);
    }
    boost::filesystem::remove_all(rootTestPath);
  }

  GitHelper git_;

  const boost::filesystem::path rootTestPath;
  const boost::filesystem::path repoRoot;
  const boost::filesystem::path repoSubdirectory;

  const std::string changedFile;
  const std::string unchangedFile;
  const std::string untrackedFile;

private:
  void copy(const boost::filesystem::path& from, const boost::filesystem::path& to) {
    if (boost::filesystem::is_directory(from)) {
      boost::filesystem::create_directories(to);
      for (auto entry : boost::filesystem::directory_iterator(from)) {
        copy(entry.path(), to / entry.path().filename());
      }
    } else {
      boost::filesystem::copy(from, to);
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
