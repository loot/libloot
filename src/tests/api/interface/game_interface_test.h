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

#ifndef LOOT_TESTS_API_INTERFACE_GAME_INTERFACE_TEST
#define LOOT_TESTS_API_INTERFACE_GAME_INTERFACE_TEST

#include "loot/api.h"
#include "tests/api/interface/api_game_operations_test.h"

namespace loot {
namespace test {
class GameInterfaceTest : public ApiGameOperationsTest {
protected:
  GameInterfaceTest() :
      emptyFile("EmptyFile.esm"), nonAsciiEsm(u8"non\u00C1scii.esm") {
    // Make sure the plugin with a non-ASCII filename exists.
    std::filesystem::copy_file(dataPath / blankEsm,
                               dataPath / std::filesystem::u8path(nonAsciiEsm));

    if (GetParam() == GameType::starfield) {
      pluginsToLoad = {
          masterFile,
          blankEsm,
          blankFullEsm,
          blankMasterDependentEsm,
          blankEsp,
          blankMasterDependentEsp,
      };
    } else {
      pluginsToLoad = {
          // These are all ASCII filenames.
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
      };
    }
  }

  const std::string emptyFile;
  const std::string nonAsciiEsm;
  std::vector<std::filesystem::path> pluginsToLoad;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         GameInterfaceTest,
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

TEST_P(GameInterfaceTest, setAdditionalDataPathsShouldDoThat) {
  const auto paths = std::vector<std::filesystem::path>{
      localPath, localPath.parent_path() / "other"};

  handle_->SetAdditionalDataPaths(paths);

  EXPECT_EQ(paths, handle_->GetAdditionalDataPaths());
}

TEST_P(GameInterfaceTest, isValidPluginShouldReturnTrueForAValidPlugin) {
  EXPECT_TRUE(handle_->IsValidPlugin(blankEsm));
}

TEST_P(GameInterfaceTest,
       isValidPluginShouldReturnTrueForAValidNonAsciiPlugin) {
  EXPECT_TRUE(handle_->IsValidPlugin(std::filesystem::u8path(nonAsciiEsm)));
}

TEST_P(GameInterfaceTest, isValidPluginShouldReturnFalseForANonPluginFile) {
  EXPECT_FALSE(handle_->IsValidPlugin(nonPluginFile));
}

TEST_P(GameInterfaceTest, isValidPluginShouldReturnFalseForAnEmptyFile) {
  // Write out an empty file.
  touch(dataPath / emptyFile);
  ASSERT_TRUE(std::filesystem::exists(dataPath / emptyFile));

  EXPECT_FALSE(handle_->IsValidPlugin(emptyFile));
}

TEST_P(
    GameInterfaceTest,
    loadPluginsWithHeadersOnlyTrueShouldLoadTheHeadersOfAllGivenPlugins) {
  handle_->LoadPlugins(pluginsToLoad, true);
  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(6, handle_->GetLoadedPlugins().size());
  } else {
    EXPECT_EQ(11, handle_->GetLoadedPlugins().size());
  }

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(handle_->GetPlugin(masterFile));
  auto plugin = handle_->GetPlugin(masterFile);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that only the header has been read.
  EXPECT_FALSE(plugin->GetCRC());
}

TEST_P(GameInterfaceTest, loadPluginsShouldTrimDotGhostFileExtensions) {
  if (GetParam() == GameType::openmw) {
    // Ghosting is not supported for OpenMW.
    EXPECT_THROW(
        handle_->LoadPlugins({blankMasterDependentEsm + ".ghost"}, true),
        std::invalid_argument);
    return;
  } else {
    handle_->LoadPlugins({blankMasterDependentEsm + ".ghost"}, true);
  }

  EXPECT_EQ(1, handle_->GetLoadedPlugins().size());

  ASSERT_NO_THROW(handle_->GetPlugin(blankMasterDependentEsm));
  const auto plugin = handle_->GetPlugin(blankMasterDependentEsm);
  ASSERT_NE(nullptr, plugin);
  EXPECT_EQ(blankMasterDependentEsm, plugin->GetName());
}

TEST_P(GameInterfaceTest,
       loadPluginsWithHeadersOnlyFalseShouldFullyLoadAllInstalledPlugins) {
  handle_->LoadPlugins(pluginsToLoad, false);
  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(6, handle_->GetLoadedPlugins().size());
  } else {
    EXPECT_EQ(11, handle_->GetLoadedPlugins().size());
  }

  // Check that one plugin's header has been read.
  ASSERT_NO_THROW(handle_->GetPlugin(masterFile));
  auto plugin = handle_->GetPlugin(masterFile);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that not only the header has been read.
  EXPECT_EQ(blankEsmCrc, plugin->GetCRC().value());
}

TEST_P(GameInterfaceTest, loadPluginsWithANonAsciiPluginShouldLoadIt) {
  handle_->LoadPlugins({std::filesystem::u8path(nonAsciiEsm)}, false);
  EXPECT_EQ(1, handle_->GetLoadedPlugins().size());

  // Check that one plugin's header has been read.
  auto plugin = handle_->GetPlugin(nonAsciiEsm);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that not only the header has been read.
  EXPECT_EQ(blankEsmCrc, plugin->GetCRC().value());
}

TEST_P(GameInterfaceTest, clearLoadedPluginsShouldClearThePluginsCache) {
  handle_->LoadPlugins({std::filesystem::u8path(blankEsm)}, true);
  const auto pointer = handle_->GetPlugin(blankEsm);
  ASSERT_NE(nullptr, pointer);

  handle_->ClearLoadedPlugins();

  EXPECT_EQ(nullptr, handle_->GetPlugin(blankEsm));
}

TEST_P(GameInterfaceTest, getPluginThatIsNotCachedShouldReturnANullPointer) {
  EXPECT_FALSE(handle_->GetPlugin(blankEsm));
}

TEST_P(GameInterfaceTest,
       gettingPluginsShouldReturnAnEmptySetIfNoneHaveBeenLoaded) {
  EXPECT_TRUE(handle_->GetLoadedPlugins().empty());
}

TEST_P(GameInterfaceTest, sortPluginsShouldSucceedIfPassedValidArguments) {
  std::vector<std::string> expectedOrder;
  if (GetParam() == GameType::starfield) {
    expectedOrder = {
        masterFile,
        blankEsm,
        blankFullEsm,
        blankMasterDependentEsm,
        blankEsp,
        blankMasterDependentEsp,
    };
  } else {
    expectedOrder = {
        masterFile,
        blankEsm,
        blankMasterDependentEsm,
        blankDifferentEsm,
        blankDifferentMasterDependentEsm,
        blankMasterDependentEsp,
        blankDifferentMasterDependentEsp,
        blankEsp,
        blankPluginDependentEsp,
        blankDifferentEsp,
        blankDifferentPluginDependentEsp,
    };
  }

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    expectedOrder.insert(expectedOrder.begin() + 5, blankEsl);
  }

  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    pluginsToLoad.push_back(blankEsl);
  }

  handle_->LoadCurrentLoadOrderState();
  handle_->LoadPlugins(pluginsToLoad, false);

  std::vector<std::string> pluginsToSort;
  for (const auto& plugin : pluginsToLoad) {
    pluginsToSort.push_back(plugin.filename().u8string());
  }

  std::vector<std::string> actualOrder = handle_->SortPlugins(pluginsToSort);

  EXPECT_EQ(expectedOrder, actualOrder);
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldReturnFalseIfTheGivenPluginIsNotActive) {
  handle_->LoadCurrentLoadOrderState();

  EXPECT_TRUE(handle_->IsPluginActive(blankEsm));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldReturnTrueIfTheGivenPluginIsActive) {
  handle_->LoadCurrentLoadOrderState();
  EXPECT_FALSE(handle_->IsPluginActive(blankEsp));
}

TEST_P(GameInterfaceTest, getLoadOrderShouldReturnTheCurrentLoadOrder) {
  // Remove the non-ASCII duplicate plugin.
  std::filesystem::remove(dataPath / std::filesystem::u8path(nonAsciiEsm));

  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed. Don't clear the additional
  // data paths for OpenMW because they come from test config.
  if (GetParam() != GameType::openmw) {
    handle_->SetAdditionalDataPaths({});
  }

  handle_->LoadCurrentLoadOrderState();

  if (GetParam() == GameType::openmw) {
    ASSERT_EQ(std::vector<std::string>({
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
              handle_->GetLoadOrder());
  } else {
    ASSERT_EQ(getLoadOrder(), handle_->GetLoadOrder());
  }
}

TEST_P(GameInterfaceTest, setLoadOrderShouldSetTheLoadOrder) {
  // Remove the non-ASCII duplicate plugin.
  std::filesystem::remove(dataPath / std::filesystem::u8path(nonAsciiEsm));

  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed. Don't clear the additional
  // data paths for OpenMW because they come from test config.
  if (GetParam() != GameType::openmw) {
    handle_->SetAdditionalDataPaths({});
  }

  handle_->LoadCurrentLoadOrderState();

  const auto gameSupportsEsl =
      GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
      GetParam() == GameType::tes5se || GetParam() == GameType::tes5vr ||
      GetParam() == GameType::starfield;

  std::vector<std::string> loadOrder;
  if (GetParam() == GameType::starfield) {
    loadOrder = {
        masterFile,
        blankEsm,
        blankMasterDependentEsm,
        blankDifferentEsm,
        blankDifferentEsp,
        blankEsp,
        blankMasterDependentEsp,
    };
  } else if (GetParam() == GameType::openmw) {
    loadOrder = {
        blankDifferentMasterDependentEsm,
        blankDifferentPluginDependentEsp,
        blankDifferentEsm,
        blankDifferentEsp,
        blankMasterDependentEsm,
        blankMasterDependentEsp,
        blankPluginDependentEsp,
        blankEsp,
        masterFile,
        blankDifferentMasterDependentEsp,
        blankEsm,
    };
  } else {
    loadOrder = {
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
    };

    if (gameSupportsEsl) {
      loadOrder.insert(loadOrder.begin() + 5, blankEsl);
    }
  }

  EXPECT_NO_THROW(handle_->SetLoadOrder(loadOrder));

  EXPECT_EQ(loadOrder, handle_->GetLoadOrder());

  // It's not possible to persist the load order of inactive plugins for
  // OpenMW.
  if (GetParam() != GameType::openmw) {
    if (gameSupportsEsl) {
      loadOrder.erase(std::begin(loadOrder));
    }

    EXPECT_EQ(loadOrder, getLoadOrder());
  }
}
}
}

#endif
