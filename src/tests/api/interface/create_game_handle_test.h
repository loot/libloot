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

#ifndef LOOT_TESTS_API_INTERFACE_CREATE_GAME_HANDLE_TEST
#define LOOT_TESTS_API_INTERFACE_CREATE_GAME_HANDLE_TEST

#include <climits>

#include "loot/api.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class CreateGameHandleTest : public CommonGameTestFixture,
                             public testing::WithParamInterface<GameType> {
protected:
  CreateGameHandleTest() :
      CommonGameTestFixture(GetParam()),
      handle_(nullptr),
      gamePathSymlink(gamePath.string() + ".symlink"),
      localPathSymlink(localPath.string() + ".symlink"),
      gamePathJunctionLink(gamePath.string() + ".junction"),
      localPathJunctionLink(localPath.string() + ".junction"),
      originalWorkingDirectory(std::filesystem::current_path()) {}

  void SetUp() override {
    using std::filesystem::file_type;
    using std::filesystem::status;
    CommonGameTestFixture::SetUp();

    std::filesystem::create_directory_symlink(gamePath, gamePathSymlink);
    ASSERT_EQ(file_type::directory, status(gamePathSymlink).type());

    std::filesystem::create_directory_symlink(localPath, localPathSymlink);
    ASSERT_EQ(file_type::directory, status(localPathSymlink).type());

#ifdef _WIN32
    system(("mklink /J \"" +
            std::filesystem::absolute(gamePathJunctionLink).string() + "\" \"" +
            std::filesystem::absolute(dataPath).parent_path().string() + "\"")
               .c_str());
    system(("mklink /J \"" +
            std::filesystem::absolute(localPathJunctionLink).string() +
            "\" \"" + std::filesystem::absolute(localPath).string() + "\"")
               .c_str());
#endif

    // relative() doesn't work when the current working directory and the given
    // path are on separate drives, so ensure that's not the case.
    std::filesystem::current_path(gamePath.parent_path());
  }

  void TearDown() override {
    std::filesystem::current_path(originalWorkingDirectory);
  }

  std::unique_ptr<GameInterface> handle_;

  const std::filesystem::path gamePathSymlink;
  const std::filesystem::path localPathSymlink;
  const std::filesystem::path gamePathJunctionLink;
  const std::filesystem::path localPathJunctionLink;
  const std::filesystem::path originalWorkingDirectory;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         CreateGameHandleTest,
                         ::testing::Values(GameType::tes4,
                                           GameType::tes5,
                                           GameType::fo3,
                                           GameType::fonv,
                                           GameType::fo4,
                                           GameType::tes5se,
                                           GameType::fo4vr,
                                           GameType::tes5vr,
                                           GameType::tes3,
                                           GameType::starfield,
                                           GameType::openmw));

TEST_P(CreateGameHandleTest,
       shouldSucceedIfPassedValidParametersWithRelativePaths) {
  using std::filesystem::relative;
  EXPECT_NO_THROW(handle_ = CreateGameHandle(
                      GetParam(), relative(gamePath), relative(localPath)));
  EXPECT_TRUE(handle_);
}

TEST_P(CreateGameHandleTest,
       shouldSucceedIfPassedValidParametersWithAbsolutePaths) {
  EXPECT_NO_THROW(handle_ = CreateGameHandle(GetParam(), gamePath, localPath));
  EXPECT_TRUE(handle_);
}

TEST_P(CreateGameHandleTest, shouldThrowIfPassedAGamePathThatDoesNotExist) {
  EXPECT_THROW(CreateGameHandle(GetParam(), missingPath, localPath),
               std::invalid_argument);
}

TEST_P(CreateGameHandleTest, shouldSucceedIfPassedALocalPathThatDoesNotExist) {
  EXPECT_NO_THROW(handle_ =
                      CreateGameHandle(GetParam(), gamePath, missingPath));
  EXPECT_TRUE(handle_);
}

TEST_P(CreateGameHandleTest, shouldThrowIfPassedALocalPathThatIsNotADirectory) {
  EXPECT_THROW(CreateGameHandle(GetParam(), gamePath, dataPath / blankEsm),
               std::invalid_argument);
}

#ifdef _WIN32
TEST_P(CreateGameHandleTest, shouldReturnOkIfPassedAnEmptyLocalPathString) {
  EXPECT_NO_THROW(handle_ = CreateGameHandle(GetParam(), gamePath, ""));
  EXPECT_TRUE(handle_);
}
#endif

TEST_P(CreateGameHandleTest, shouldReturnOkIfPassedGameAndLocalPathSymlinks) {
  EXPECT_NO_THROW(handle_ = CreateGameHandle(
                      GetParam(), gamePathSymlink, localPathSymlink));
  EXPECT_TRUE(handle_);
}

#ifdef _WIN32
TEST_P(CreateGameHandleTest,
       shouldReturnOkIfPassedGameAndLocalPathJunctionLinks) {
  EXPECT_NO_THROW(handle_ = CreateGameHandle(
                      GetParam(), gamePathJunctionLink, localPathJunctionLink));
  EXPECT_TRUE(handle_);
}
#endif
}
}

#endif
