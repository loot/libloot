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

#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class GameTest : public CommonGameTestFixture {
protected:
  void loadInstalledPlugins(Game& game, bool headersOnly) {
    const std::vector<std::string> plugins({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
    });
    game.LoadPlugins(plugins, headersOnly);
  }
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        GameTest,
                        ::testing::Values(
                          GameType::tes4,
                          GameType::tes5,
                          GameType::fo3,
                          GameType::fonv,
                          GameType::fo4,
                          GameType::tes5se));

TEST_P(GameTest, constructingShouldStoreTheGivenValues) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_EQ(GetParam(), game.Type());
  EXPECT_EQ(dataPath, game.DataPath());
}

#ifndef _WIN32
        // Testing on Windows will find real game installs in the Registry, so cannot
        // test autodetection fully unless on Linux.
TEST_P(GameTest, constructingShouldThrowOnLinuxIfGamePathIsNotGiven) {
  EXPECT_THROW(Game(GetParam(), "", localPath), std::invalid_argument);
}

TEST_P(GameTest, constructingShouldThrowOnLinuxIfLocalPathIsNotGiven) {
  EXPECT_THROW(Game(GetParam(), dataPath.parent_path()), std::system_error);
}
#else
TEST_P(GameTest, constructingShouldNotThrowOnWindowsIfLocalPathIsNotGiven) {
  EXPECT_NO_THROW(Game(GetParam(), dataPath.parent_path(), localPath));
}
#endif

TEST_P(GameTest, constructingShouldNotThrowIfGameAndLocalPathsAreNotEmpty) {
  EXPECT_NO_THROW(Game(GetParam(), dataPath.parent_path(), localPath));
}

TEST_P(GameTest, loadPluginsWithHeadersOnlyTrueShouldLoadTheHeadersOfAllInstalledPlugins) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, true));
  EXPECT_EQ(11, game.GetCache()->GetPlugins().size());

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(game.GetPlugin(masterFile));
  auto plugin = game.GetPlugin(masterFile);
  EXPECT_EQ("5.0", plugin->GetVersion());

  // Check that only the header has been read.
  EXPECT_EQ(0, plugin->GetCRC());
}

TEST_P(GameTest, loadPluginsWithANonPluginShouldNotAddItToTheLoadedPlugins) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_THROW(game.LoadPlugins({ nonPluginFile }, false), std::invalid_argument);

  ASSERT_TRUE(game.GetLoadedPlugins().empty());
}

TEST_P(GameTest, loadPluginsWithAnInvalidPluginShouldNotAddItToTheLoadedPlugins) {
  ASSERT_FALSE(boost::filesystem::exists(dataPath / invalidPlugin));
  ASSERT_NO_THROW(boost::filesystem::copy_file(dataPath / blankEsm, dataPath / invalidPlugin));
  ASSERT_TRUE(boost::filesystem::exists(dataPath / invalidPlugin));
  boost::filesystem::ofstream out(dataPath / invalidPlugin, std::fstream::app);
  out << "GRUP0";
  out.close();

  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_NO_THROW(game.LoadPlugins({ invalidPlugin }, false));

  ASSERT_TRUE(game.GetLoadedPlugins().empty());
}

TEST_P(GameTest, loadPluginsWithHeadersOnlyFalseShouldFullyLoadAllInstalledPlugins) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_NO_THROW(loadInstalledPlugins(game, false));
  EXPECT_EQ(11, game.GetCache()->GetPlugins().size());

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(game.GetPlugin(blankEsm));
  auto plugin = game.GetPlugin(blankEsm);
  EXPECT_EQ("5.0", plugin->GetVersion());

  // Check that not only the header has been read.
  EXPECT_EQ(blankEsmCrc, plugin->GetCRC());
}

TEST_P(GameTest, shouldShowBlankEsmAsActiveIfItHasNotBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_TRUE(game.IsPluginActive(blankEsm));
}

TEST_P(GameTest, shouldShowBlankEspAsInactiveIfItHasNotBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  EXPECT_FALSE(game.IsPluginActive(blankEsp));
}

TEST_P(GameTest, shouldShowBlankEsmAsActiveIfItsHeaderHasBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_NO_THROW(loadInstalledPlugins(game, true));

  EXPECT_TRUE(game.IsPluginActive(blankEsm));
}

TEST_P(GameTest, shouldShowBlankEspAsInactiveIfItsHeaderHasBeenLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_NO_THROW(loadInstalledPlugins(game, true));

  EXPECT_FALSE(game.IsPluginActive(blankEsp));
}

TEST_P(GameTest, shouldShowBlankEsmAsActiveIfItHasBeenFullyLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_NO_THROW(loadInstalledPlugins(game, false));

  EXPECT_TRUE(game.IsPluginActive(blankEsm));
}

TEST_P(GameTest, shouldShowBlankEspAsInactiveIfItHasBeenFullyLoaded) {
  Game game = Game(GetParam(), dataPath.parent_path(), localPath);

  ASSERT_NO_THROW(loadInstalledPlugins(game, false));

  EXPECT_FALSE(game.IsPluginActive(blankEsp));
}
}
}

#endif
