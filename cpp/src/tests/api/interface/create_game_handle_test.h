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
      originalWorkingDirectory(std::filesystem::current_path()) {}

  void TearDown() override {
    std::filesystem::current_path(originalWorkingDirectory);
  }

  std::unique_ptr<GameInterface> handle_;

private:
  std::filesystem::path originalWorkingDirectory;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         CreateGameHandleTest,
                         ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(CreateGameHandleTest,
       shouldSucceedIfPassedValidParametersWithRelativePaths) {
  using std::filesystem::relative;

  // relative() doesn't work when the current working directory and the given
  // path are on separate drives, so ensure that's not the case.
  std::filesystem::current_path(gamePath.parent_path());

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
  const auto gamePathSymlink =
      std::filesystem::u8path(gamePath.u8string() + ".symlink");
  const auto localPathSymlink =
      std::filesystem::u8path(localPath.u8string() + ".symlink");

  std::filesystem::create_directory_symlink(gamePath, gamePathSymlink);
  ASSERT_EQ(std::filesystem::file_type::directory,
            std::filesystem::status(gamePathSymlink).type());

  std::filesystem::create_directory_symlink(localPath, localPathSymlink);
  ASSERT_EQ(std::filesystem::file_type::directory,
            std::filesystem::status(localPathSymlink).type());

  EXPECT_NO_THROW(handle_ = CreateGameHandle(
                      GetParam(), gamePathSymlink, localPathSymlink));
  EXPECT_TRUE(handle_);
}

#ifdef _WIN32
TEST_P(CreateGameHandleTest,
       shouldReturnOkIfPassedGameAndLocalPathJunctionLinks) {
  const auto gamePathJunctionLink =
      std::filesystem::u8path(gamePath.u8string() + ".junction");
  const auto localPathJunctionLink =
      std::filesystem::u8path(localPath.u8string() + ".junction");

  system(("mklink /J \"" +
          std::filesystem::absolute(gamePathJunctionLink).string() + "\" \"" +
          std::filesystem::absolute(dataPath).parent_path().string() + "\"")
             .c_str());
  system(("mklink /J \"" +
          std::filesystem::absolute(localPathJunctionLink).string() + "\" \"" +
          std::filesystem::absolute(localPath).string() + "\"")
             .c_str());

  EXPECT_NO_THROW(handle_ = CreateGameHandle(
                      GetParam(), gamePathJunctionLink, localPathJunctionLink));
  EXPECT_TRUE(handle_);
}
#endif

#ifndef _WIN32
TEST_P(
    CreateGameHandleTest,
    shouldThrowOnLinuxIfLocalPathIsNotGivenExceptForMorrowindOpenMWAndOblivionRemastered) {
  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::oblivionRemastered) {
    EXPECT_NO_THROW(CreateGameHandle(GetParam(), gamePath));
  } else {
    EXPECT_THROW(CreateGameHandle(GetParam(), gamePath), std::runtime_error);
  }
}
#else
TEST_P(CreateGameHandleTest, shouldNotThrowOnWindowsIfLocalPathIsNotGiven) {
  EXPECT_NO_THROW(CreateGameHandle(GetParam(), gamePath));
}
#endif

TEST_P(CreateGameHandleTest, shouldNotThrowIfGameAndLocalPathsAreNotEmpty) {
  EXPECT_NO_THROW(CreateGameHandle(GetParam(), gamePath, localPath));
}

TEST_P(CreateGameHandleTest,
       shouldSetAdditionalDataPathsForFallout4FromMicrosoftStoreOrStarfield) {
  if (GetParam() == GameType::fo4) {
    // Create the file that indicates it's a Microsoft Store install.
    touch(gamePath / "appxmanifest.xml");
  } else if (GetParam() == GameType::openmw) {
    std::ofstream out(gamePath / "openmw.cfg");
    out << "data-local=\"" << (localPath / "data").u8string() << "\""
        << std::endl
        << "config=\"" << localPath.u8string() << "\"";
  }

  const auto game = CreateGameHandle(GetParam(), gamePath, localPath);

  if (GetParam() == GameType::fo4) {
    const auto basePath = gamePath / ".." / "..";
    EXPECT_EQ(std::vector<std::filesystem::path>(
                  {basePath / "Fallout 4- Automatron (PC)" / "Content" / "Data",
                   basePath / "Fallout 4- Nuka-World (PC)" / "Content" / "Data",
                   basePath / "Fallout 4- Wasteland Workshop (PC)" / "Content" /
                       "Data",
                   basePath / "Fallout 4- High Resolution Texture Pack" /
                       "Content" / "Data",
                   basePath / "Fallout 4- Vault-Tec Workshop (PC)" / "Content" /
                       "Data",
                   basePath / "Fallout 4- Far Harbor (PC)" / "Content" / "Data",
                   basePath / "Fallout 4- Contraptions Workshop (PC)" /
                       "Content" / "Data"}),
              game->GetAdditionalDataPaths());
  } else if (GetParam() == GameType::starfield) {
    ASSERT_EQ(1, game->GetAdditionalDataPaths().size());

    const auto expectedSuffix = std::filesystem::u8path("Documents") /
                                "My Games" / "Starfield" / "Data";
    EXPECT_TRUE(endsWith(game->GetAdditionalDataPaths()[0].u8string(),
                         expectedSuffix.u8string()));
  } else if (GetParam() == GameType::openmw) {
    EXPECT_EQ(std::vector<std::filesystem::path>{localPath / "data"},
              game->GetAdditionalDataPaths());
  } else {
    EXPECT_TRUE(game->GetAdditionalDataPaths().empty());
  }
}
}
}

#endif
