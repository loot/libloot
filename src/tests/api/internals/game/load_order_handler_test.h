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
class LoadOrderHandlerTest : public CommonGameTestFixture,
                             public testing::WithParamInterface<GameType> {
protected:
  LoadOrderHandlerTest() :
      CommonGameTestFixture(GetParam()),
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
    } else if (GetParam() == GameType::starfield) {
      loadOrderToSet_ = {
          masterFile,
          blankEsm,
          blankMasterDependentEsm,
          blankDifferentEsm,
          blankDifferentEsp,
          blankEsp,
          blankMasterDependentEsp,
      };
    }
  }

  LoadOrderHandler createHandler() {
    return LoadOrderHandler(GetParam(), gamePath, localPath);
  }

  std::vector<std::string> getEarlyLoadingPlugins() {
    switch (GetParam()) {
      case GameType::openmw:
        return {"builtin.omwscripts"};
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
      case GameType::starfield:
        return {"Starfield.esm",
                "Constellation.esm",
                "OldMars.esm",
                "ShatteredSpace.esm",
                "SFBGS003.esm",
                "SFBGS004.esm",
                "SFBGS006.esm",
                "SFBGS007.esm",
                "SFBGS008.esm"};
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
                         ::testing::ValuesIn(ALL_GAME_TYPES));

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
  EXPECT_NO_THROW(LoadOrderHandler(GetParam(), gamePath));
}
#else
TEST_P(LoadOrderHandlerTest,
       constructorShouldNotThrowIfNoLocalPathIsSetAndGameTypeIsMorrowind) {
  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::oblivionRemastered) {
    EXPECT_NO_THROW(LoadOrderHandler(GetParam(), gamePath));
  } else {
    EXPECT_THROW(LoadOrderHandler(GetParam(), gamePath), std::system_error);
  }
}
#endif

TEST_P(LoadOrderHandlerTest,
       constructorShouldNotThrowIfAValidGameIdAndGamePathAndLocalPathAreSet) {
  EXPECT_NO_THROW(LoadOrderHandler(GetParam(), gamePath, localPath));
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

  if (GetParam() == GameType::openmw) {
    EXPECT_EQ(std::vector<std::string>({
                  blankDifferentEsm,
                  blankDifferentMasterDependentEsm,
                  blankDifferentEsp,
                  blankDifferentPluginDependentEsp,
                  blankMasterDependentEsm,
                  blankMasterDependentEsp,
                  blankEsp,
                  blankPluginDependentEsp,
                  masterFile,
                  blankEsm,
                  blankDifferentMasterDependentEsp,
              }),
              loadOrderHandler.GetLoadOrder());
  } else {
    ASSERT_EQ(getLoadOrder(), loadOrderHandler.GetLoadOrder());
  }
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

TEST_P(LoadOrderHandlerTest, getAdditionalDataPathsShouldReturnValidData) {
  if (GetParam() == GameType::fo4) {
    // Create the file that indicates it's a Microsoft Store install.
    touch(gamePath / "appxmanifest.xml");
  } else if (GetParam() == GameType::openmw) {
    std::ofstream out(gamePath / "openmw.cfg");
    out << "data-local=\"" << (localPath / "data").u8string() << "\""
        << std::endl
        << "config=\"" << localPath.u8string() << "\"";
  }

  auto loadOrderHandler = createHandler();

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
              loadOrderHandler.GetAdditionalDataPaths());
  } else if (GetParam() == GameType::starfield) {
    ASSERT_EQ(1, loadOrderHandler.GetAdditionalDataPaths().size());

    const auto expectedSuffix = std::filesystem::u8path("Documents") /
                                "My Games" / "Starfield" / "Data";
    EXPECT_TRUE(boost::ends_with(
        loadOrderHandler.GetAdditionalDataPaths()[0].u8string(),
        expectedSuffix.u8string()));
  } else if (GetParam() == GameType::openmw) {
    EXPECT_EQ(std::vector<std::filesystem::path>{localPath / "data"},
              loadOrderHandler.GetAdditionalDataPaths());
  } else {
    EXPECT_TRUE(loadOrderHandler.GetAdditionalDataPaths().empty());
  }
}

TEST_P(LoadOrderHandlerTest, setLoadOrderShouldSetTheLoadOrder) {
  auto loadOrderHandler = createHandler();
  loadOrderHandler.LoadCurrentState();

  EXPECT_NO_THROW(loadOrderHandler.SetLoadOrder(loadOrderToSet_));

  if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
      GetParam() == GameType::tes5se || GetParam() == GameType::tes5vr ||
      GetParam() == GameType::starfield)
    loadOrderToSet_.erase(begin(loadOrderToSet_));

  if (GetParam() == GameType::openmw) {
    // Can't set the load order positions of inactive plugins,
    // this reads what libloadorder has cached in memory instead of
    // what was actually saved.
    EXPECT_EQ(loadOrderToSet_, loadOrderHandler.GetLoadOrder());
  } else {
    EXPECT_EQ(loadOrderToSet_, getLoadOrder());
  }
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
