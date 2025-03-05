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

#ifndef LOOT_TESTS_API_INTERNALS_GAME_GAME_CACHE_TEST
#define LOOT_TESTS_API_INTERNALS_GAME_GAME_CACHE_TEST

#include "api/game/game.h"
#include "api/game/game_cache.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class GameCacheTest : public CommonGameTestFixture {
protected:
  GameCacheTest() :
      CommonGameTestFixture(GameType::tes5),
      game_(GameType::tes5, gamePath, localPath),
      condition("Condition"),
      conditionLowercase("condition") {}

  Game game_;
  GameCache cache_;

  const std::string condition;
  const std::string conditionLowercase;
};

TEST_F(GameCacheTest, addingAPluginThatDoesNotExistShouldSucceed) {
  cache_.AddPlugin(
      Plugin(game_.GetType(), GameCache(), game_.DataPath() / blankEsm, true));
  EXPECT_EQ(blankEsm, cache_.GetPlugin(blankEsm)->GetName());
}

TEST_F(GameCacheTest,
       addingAPluginThatIsAlreadyCachedShouldOverwriteExistingEntry) {
  cache_.AddPlugin(
      Plugin(game_.GetType(), GameCache(), game_.DataPath() / blankEsm, true));
  EXPECT_FALSE(cache_.GetPlugin(blankEsm)->GetCRC());

  cache_.AddPlugin(
      Plugin(game_.GetType(), GameCache(), game_.DataPath() / blankEsm, false));
  EXPECT_EQ(blankEsmCrc, cache_.GetPlugin(blankEsm)->GetCRC().value());
}

TEST_F(GameCacheTest, gettingAPluginThatIsNotCachedShouldReturnANullPointer) {
  EXPECT_FALSE(cache_.GetPlugin(blankEsm));
}

TEST_F(GameCacheTest, gettingAPluginShouldBeCaseInsensitive) {
  cache_.AddPlugin(
      Plugin(game_.GetType(), GameCache(), game_.DataPath() / blankEsm, true));
  EXPECT_EQ(blankEsm, cache_.GetPlugin(blankEsm)->GetName());
}

TEST_F(GameCacheTest,
       gettingPluginsShouldReturnAnEmptySetIfNoPluginsHaveBeenCached) {
  EXPECT_TRUE(cache_.GetPlugins().empty());
}

TEST_F(GameCacheTest,
       gettingPluginsShouldReturnASetOfCachedPluginsIfPluginsHaveBeenCached) {
  cache_.AddPlugin(
      Plugin(game_.GetType(), GameCache(), game_.DataPath() / blankEsm, true));
  cache_.AddPlugin(
      Plugin(game_.GetType(),
             GameCache(),
             game_.DataPath() / (blankMasterDependentEsm + ".ghost"),
             true));

  EXPECT_FALSE(cache_.GetPlugins().empty());
}

TEST_F(GameCacheTest,
       gettingArchivePathsShouldReturnAnEmptySetIfNoPathsHaveBeenCached) {
  EXPECT_TRUE(cache_.GetArchivePaths().empty());
}

TEST_F(GameCacheTest,
       gettingArchivePathsShouldReturnASetOfPathsIfPathsHaveBeenCached) {
  cache_.CacheArchivePaths({game_.DataPath() / blankEsm,
                            game_.DataPath() / blankMasterDependentEsm});

  auto expected = std::set<std::filesystem::path>({
      game_.DataPath() / blankEsm,
      game_.DataPath() / blankMasterDependentEsm,
  });

  EXPECT_EQ(expected, cache_.GetArchivePaths());
}

TEST_F(GameCacheTest, clearingCachedPluginsShouldNotThrowIfNoPluginsAreCached) {
  EXPECT_NO_THROW(cache_.ClearCachedPlugins());
}

TEST_F(GameCacheTest, clearingCachedPluginsShouldClearAnyCachedPlugins) {
  cache_.AddPlugin(
      Plugin(game_.GetType(), GameCache(), game_.DataPath() / blankEsm, true));
  cache_.ClearCachedPlugins();

  EXPECT_TRUE(cache_.GetPlugins().empty());
}
}
}

#endif
