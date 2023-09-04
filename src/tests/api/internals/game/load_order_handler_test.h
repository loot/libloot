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

#ifndef LOOT_TESTS_API_INTERNALS_LOAD_ORDER_HANDLER_TEST
#define LOOT_TESTS_API_INTERNALS_LOAD_ORDER_HANDLER_TEST

#include "api/game/load_order_handler.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class LoadOrderHandlerTest : public CommonGameTestFixture {
protected:
  LoadOrderHandlerTest() :
      loadOrderToSet_({
          masterFile,
          blankEsm,
          blankMasterDependentEsm,
          blankDifferentEsm,
          blankDifferentMasterDependentEsm,
          blankDifferentEsp,
          blankDifferentPluginDependentEsp,
          blankEsp,
          blankMasterDependentEsp,
          blankDifferentMasterDependentEsp,
          blankPluginDependentEsp,
      }) {
    if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
      loadOrderToSet_.insert(loadOrderToSet_.begin() + 5, blankEsl);
    }
  }

  LoadOrderHandler createHandler() {
    return LoadOrderHandler(GetParam(), dataPath.parent_path(), localPath);
  }

  std::vector<std::string> getEarlyLoadingPlugins() {
    switch (GetParam()) {
      case GameType::tes5:
        return {"Skyrim.esm"};
      case GameType::tes5se:
        return {"Skyrim.esm",
                "Update.esm",
                "Dawnguard.esm",
                "HearthFires.esm",
                "Dragonborn.esm"};
      case GameType::tes5vr:
        return {"Skyrim.esm",
                "Update.esm",
                "Dawnguard.esm",
                "HearthFires.esm",
                "Dragonborn.esm",
                "SkyrimVR.esm"};
      case GameType::fo4:
        return {"Fallout4.esm",
                "DLCRobot.esm",
                "DLCworkshop01.esm",
                "DLCCoast.esm",
                "DLCworkshop02.esm",
                "DLCworkshop03.esm",
                "DLCNukaWorld.esm",
                "DLCUltraHighResolution.esm"};
      case GameType::fo4vr:
        return {"Fallout4.esm", "Fallout4_VR.esm"};
      default:
        return {};
    }
  }

  std::vector<std::string> getActivePlugins() {
    std::vector<std::string> activePlugins;
    for (auto& pair : getInitialLoadOrder()) {
      if (pair.second) {
        activePlugins.push_back(pair.first);
      }
    }
    return activePlugins;
  }

  std::vector<std::string> loadOrderToSet_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         LoadOrderHandlerTest,
                         ::testing::Values(GameType::tes3,
                                           GameType::tes4,
                                           GameType::tes5,
                                           GameType::fo3,
                                           GameType::fonv,
                                           GameType::fo4,
                                           GameType::tes5se));

TEST_P(LoadOrderHandlerTest, constructorShouldThrowIfNoGamePathIsSet) {
  EXPECT_THROW(LoadOrderHandler(GetParam(), ""), std::invalid_argument);
  EXPECT_THROW(LoadOrderHandler(GetParam(), ""), std::invalid_argument);
  EXPECT_THROW(LoadOrderHandler(GetParam(), "", localPath),
               std::invalid_argument);
  EXPECT_THROW(LoadOrderHandler(GetParam(), "", localPath),
               std::invalid_argument);
}

#ifdef _WIN32
TEST_P(LoadOrderHandlerTest, constructorShouldNotThrowIfNoLocalPathIsSet) {
  EXPECT_NO_THROW(LoadOrderHandler(GetParam(), dataPath.parent_path()));
}
#else
TEST_P(LoadOrderHandlerTest,
       constructorShouldNotThrowIfNoLocalPathIsSetAndGameTypeIsMorrowind) {
  if (GetParam() == GameType::tes3) {
    EXPECT_NO_THROW(LoadOrderHandler(GetParam(), dataPath.parent_path()));
  } else {
    EXPECT_THROW(LoadOrderHandler(GetParam(), dataPath.parent_path()),
                 std::system_error);
  }
}
#endif

TEST_P(LoadOrderHandlerTest,
       constructorShouldNotThrowIfAValidGameIdAndGamePathAndLocalPathAreSet) {
  EXPECT_NO_THROW(
      LoadOrderHandler(GetParam(), dataPath.parent_path(), localPath));
}

TEST_P(LoadOrderHandlerTest,
       isAmbiguousShouldReturnFalseForAnUnambiguousLoadOrder) {
  auto loadOrderHandler = createHandler();

  EXPECT_FALSE(loadOrderHandler.IsAmbiguous());
}

TEST_P(LoadOrderHandlerTest,
       isPluginActiveShouldReturnFalseIfLoadOrderStateHasNotBeenLoaded) {
  auto loadOrderHandler = createHandler();

  EXPECT_FALSE(loadOrderHandler.IsPluginActive(masterFile));
  EXPECT_FALSE(loadOrderHandler.IsPluginActive(blankEsm));
  EXPECT_FALSE(loadOrderHandler.IsPluginActive(blankEsp));
}

TEST_P(LoadOrderHandlerTest,
       isPluginActiveShouldReturnCorrectPluginStatesAfterInitialisation) {
  auto loadOrderHandler = createHandler();
  loadOrderHandler.LoadCurrentState();

  EXPECT_TRUE(loadOrderHandler.IsPluginActive(masterFile));
  EXPECT_TRUE(loadOrderHandler.IsPluginActive(blankEsm));
  EXPECT_FALSE(loadOrderHandler.IsPluginActive(blankEsp));
}

TEST_P(LoadOrderHandlerTest,
       getLoadOrderShouldReturnAnEmptyVectorIfStateHasNotBeenLoaded) {
  auto loadOrderHandler = createHandler();

  EXPECT_TRUE(loadOrderHandler.GetLoadOrder().empty());
}

TEST_P(LoadOrderHandlerTest, getLoadOrderShouldReturnTheCurrentLoadOrder) {
  auto loadOrderHandler = createHandler();
  loadOrderHandler.LoadCurrentState();

  ASSERT_EQ(getLoadOrder(), loadOrderHandler.GetLoadOrder());
}

TEST_P(LoadOrderHandlerTest,
       getActivePluginsShouldReturnAnEmptyVectorIfStateHasNotBeenLoaded) {
  auto loadOrderHandler = createHandler();

  EXPECT_TRUE(loadOrderHandler.GetActivePlugins().empty());
}

TEST_P(LoadOrderHandlerTest, getActivePluginsShouldReturnOnlyActivePlugins) {
  auto loadOrderHandler = createHandler();
  loadOrderHandler.LoadCurrentState();

  ASSERT_EQ(getActivePlugins(), loadOrderHandler.GetActivePlugins());
}

TEST_P(LoadOrderHandlerTest,
       getEarlyLoadingPluginsShouldReturnValidDataEvenIfStateHasNotBeenLoaded) {
  auto loadOrderHandler = createHandler();

  ASSERT_EQ(getEarlyLoadingPlugins(),
            loadOrderHandler.GetEarlyLoadingPlugins());

  loadOrderHandler.LoadCurrentState();

  ASSERT_EQ(getEarlyLoadingPlugins(),
            loadOrderHandler.GetEarlyLoadingPlugins());
}

TEST_P(LoadOrderHandlerTest, setLoadOrderShouldSetTheLoadOrder) {
  auto loadOrderHandler = createHandler();
  loadOrderHandler.LoadCurrentState();

  EXPECT_NO_THROW(loadOrderHandler.SetLoadOrder(loadOrderToSet_));

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se)
    loadOrderToSet_.erase(begin(loadOrderToSet_));

  EXPECT_EQ(loadOrderToSet_, getLoadOrder());
}

TEST_P(LoadOrderHandlerTest, setExternalPluginPathsShouldAcceptAnEmptyVector) {
  auto loadOrderHandler = createHandler();
  EXPECT_NO_THROW(loadOrderHandler.SetAdditionalDataPaths({}));
}

TEST_P(LoadOrderHandlerTest,
       setExternalPluginPathsShouldAcceptANonEmptyVector) {
  auto loadOrderHandler = createHandler();
  EXPECT_NO_THROW(loadOrderHandler.SetAdditionalDataPaths(
      {std::filesystem::u8path("a"), std::filesystem::u8path("b")}));
}
}
}

#endif
