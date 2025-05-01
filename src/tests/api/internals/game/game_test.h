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

#ifndef LOOT_TESTS_API_INTERNALS_GAME_GAME_TEST
#define LOOT_TESTS_API_INTERNALS_GAME_GAME_TEST

#include "api/game/game.h"
#include "loot/exception/error_categories.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class GameTest : public CommonGameTestFixture,
                 public testing::WithParamInterface<GameType> {
protected:
  GameTest() :
      CommonGameTestFixture(GetParam()),
      blankArchive("Blank" + GetArchiveFileExtension(GetParam())) {
    touch(dataPath / blankArchive);
  }

  void loadInstalledPlugins(Game& game, bool headersOnly) {
    const auto plugins = GetInstalledPlugins();
    game.LoadPlugins(plugins, headersOnly);
  }

  const std::string blankArchive;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(, GameTest, ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(GameTest, constructingShouldStoreTheGivenValues) {
  Game game = Game(GetParam(), gamePath, localPath);

  EXPECT_EQ(GetParam(), game.GetType());
  EXPECT_EQ(dataPath, game.DataPath());
}

TEST_P(
    GameTest,
    loadPluginsShouldFindAndCacheArchivesForLoadDetectionWhenLoadingPlugins) {
  Game game = Game(GetParam(), gamePath, localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, false));

  auto expected = std::set<std::filesystem::path>({dataPath / blankArchive});
  EXPECT_EQ(expected, game.GetCache().GetArchivePaths());
}

TEST_P(GameTest, loadPluginsShouldFindArchivesInAdditionalDataPaths) {
  // Create a couple of external archive files.
  const std::string archiveFileExtension =
      GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
              GetParam() == GameType::starfield
          ? ".ba2"
          : ".bsa";

  const auto ba2Path1 =
      gamePath /
      ("../../Fallout 4- Far Harbor (PC)/Content/Data/DLCCoast - Main" +
       archiveFileExtension);
  const auto ba2Path2 =
      gamePath / ("../../Fallout 4- Nuka-World (PC)/Content/Data/DLCNukaWorld "
                  "- Voices_it" +
                  archiveFileExtension);
  touch(ba2Path1);
  touch(ba2Path2);

  Game game = Game(GetParam(), gamePath, localPath);

  game.SetAdditionalDataPaths({ba2Path1.parent_path(), ba2Path2.parent_path()});

  EXPECT_NO_THROW(loadInstalledPlugins(game, true));

  const auto archivePaths = game.GetCache().GetArchivePaths();

  EXPECT_EQ(std::set<std::filesystem::path>(
                {ba2Path1, ba2Path2, dataPath / blankArchive}),
            archivePaths);
}

TEST_P(GameTest, loadPluginsShouldClearTheArchivesCacheBeforeFindingArchives) {
  Game game = Game(GetParam(), gamePath, localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, false));
  EXPECT_NO_THROW(loadInstalledPlugins(game, false));
  EXPECT_EQ(1, game.GetCache().GetArchivePaths().size());
}
}
}

#endif
