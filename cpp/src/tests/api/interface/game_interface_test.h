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
constexpr unsigned int ESP_ERROR_PLUGIN_METADATA_NOT_FOUND = 14;

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
                         ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(GameInterfaceTest, setAdditionalDataPathsShouldDoThat) {
  const auto paths = std::vector<std::filesystem::path>{
      localPath, localPath.parent_path() / "other"};

  handle_->SetAdditionalDataPaths(paths);

  EXPECT_EQ(paths, handle_->GetAdditionalDataPaths());
}

TEST_P(GameInterfaceTest, setAdditionalDataPathsShouldClearTheConditionCache) {
  PluginMetadata metadata(blankEsm);
  metadata.SetLoadAfterFiles({File("plugin.esp", "", "file(\"plugin.esp\")")});
  handle_->GetDatabase().SetPluginUserMetadata(metadata);

  auto evaluatedMetadata =
      handle_->GetDatabase().GetPluginUserMetadata(blankEsm, true);
  EXPECT_FALSE(evaluatedMetadata.has_value());

  const auto dataFilePath = gamePath.parent_path() / "Data" / "plugin.esp";
  touch(dataFilePath);
  handle_->SetAdditionalDataPaths({dataFilePath.parent_path()});

  evaluatedMetadata =
      handle_->GetDatabase().GetPluginUserMetadata(blankEsm, true);
  EXPECT_FALSE(evaluatedMetadata.value().GetLoadAfterFiles().empty());
}

TEST_P(GameInterfaceTest,
       setAdditionalDataPathsShouldUpdateWhereLoadOrderPluginsAreFound) {
  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed.
  handle_->SetAdditionalDataPaths({});
  handle_->LoadCurrentLoadOrderState();
  auto loadOrder = handle_->GetLoadOrder();

  const auto filename = "plugin.esp";
  const auto dataFilePath = gamePath.parent_path() / "Data" / filename;
  std::filesystem::create_directories(dataFilePath.parent_path());
  std::filesystem::copy_file(getSourcePluginsPath() / blankEsp, dataFilePath);
  ASSERT_TRUE(std::filesystem::exists(dataFilePath));

  if (GetParam() == GameType::starfield) {
    std::filesystem::copy_file(getSourcePluginsPath() / blankEsp,
                               dataPath / filename);
    ASSERT_TRUE(std::filesystem::exists(dataPath / filename));
  }

  std::filesystem::last_write_time(
      dataFilePath,
      std::filesystem::file_time_type::clock().now() + std::chrono::hours(1));

  handle_->SetAdditionalDataPaths({dataFilePath.parent_path()});
  handle_->LoadCurrentLoadOrderState();

  loadOrder.push_back(filename);

  EXPECT_EQ(loadOrder, handle_->GetLoadOrder());
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

TEST_P(GameInterfaceTest,
       isValidPluginShouldResolveRelativePathsRelativeToDataPath) {
  const auto path = ".." / dataPath.filename() / blankEsm;

  EXPECT_TRUE(handle_->IsValidPlugin(path));
}

TEST_P(GameInterfaceTest, isValidPluginShouldUseAbsolutePathsAsGiven) {
  ASSERT_TRUE(dataPath.is_absolute());

  const auto path = dataPath / std::filesystem::u8path(blankEsm);

  EXPECT_TRUE(handle_->IsValidPlugin(path));
}

TEST_P(
    GameInterfaceTest,
    isValidPluginShouldTryGhostedPathIfGivenPluginDoesNotExistExceptForOpenMW) {
  if (GetParam() == GameType::openmw) {
    // This wasn't done for OpenMW during common setup.
    const auto pluginPath = dataPath / (blankMasterDependentEsm + ".ghost");
    std::filesystem::rename(dataPath / blankMasterDependentEsm, pluginPath);

    EXPECT_FALSE(handle_->IsValidPlugin(blankMasterDependentEsm));
  } else {
    EXPECT_TRUE(handle_->IsValidPlugin(blankMasterDependentEsm));
  }
}

TEST_P(GameInterfaceTest,
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
       loadPluginsWithHeadersOnlyTrueShouldLoadTheHeadersOfGivenPlugins) {
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

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldNotThrowIfAFilenameHasNonWindows1252EncodableCharacters) {
  const auto pluginName = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.esp");
  std::filesystem::copy(dataPath / blankEsp, dataPath / pluginName);

  EXPECT_NO_THROW(handle_->LoadPlugins({pluginName}, false));
}

TEST_P(GameInterfaceTest,
       loadPluginsWithANonPluginShouldNotAddItToTheLoadedPlugins) {
  ASSERT_THROW(handle_->LoadPlugins({nonPluginFile}, false),
               std::invalid_argument);

  ASSERT_TRUE(handle_->GetLoadedPlugins().empty());
}

TEST_P(GameInterfaceTest,
       loadPluginsWithAnInvalidPluginShouldNotAddItToTheLoadedPlugins) {
  ASSERT_FALSE(std::filesystem::exists(dataPath / invalidPlugin));
  ASSERT_NO_THROW(std::filesystem::copy_file(dataPath / blankEsm,
                                             dataPath / invalidPlugin));
  ASSERT_TRUE(std::filesystem::exists(dataPath / invalidPlugin));
  std::ofstream out(dataPath / invalidPlugin, std::fstream::app);
  out << "GRUP0";
  out.close();

  ASSERT_NO_THROW(handle_->LoadPlugins({invalidPlugin}, false));

  ASSERT_TRUE(handle_->GetLoadedPlugins().empty());
}

TEST_P(GameInterfaceTest, loadPluginsShouldNotClearThePluginsCache) {
  handle_->LoadPlugins({std::filesystem::u8path(blankEsm)}, true);
  ASSERT_EQ(1, handle_->GetLoadedPlugins().size());

  handle_->LoadPlugins({std::filesystem::u8path(blankEsp)}, true);

  EXPECT_EQ(2, handle_->GetLoadedPlugins().size());
}

TEST_P(GameInterfaceTest,
       loadPluginsShouldReplaceCacheEntriesForTheGivenPlugins) {
  handle_->LoadPlugins({std::filesystem::u8path(blankEsm)}, true);
  const auto pointer = handle_->GetPlugin(blankEsm);
  ASSERT_NE(nullptr, pointer);

  handle_->LoadPlugins({std::filesystem::u8path(blankEsm)}, false);

  const auto newPointer = handle_->GetPlugin(blankEsm);
  ASSERT_NE(nullptr, newPointer);
  EXPECT_NE(pointer, newPointer);
}

TEST_P(GameInterfaceTest,
       loadPluginsShouldThrowIfGivenVectorElementsWithTheSameFilename) {
  const auto dataPluginPath = dataPath / std::filesystem::u8path(blankEsm);
  const auto sourcePluginPath =
      getSourcePluginsPath() / std::filesystem::u8path(blankEsm);

  EXPECT_THROW(handle_->LoadPlugins(std::vector<std::filesystem::path>(
                                        {dataPluginPath, sourcePluginPath}),
                                    true),
               std::invalid_argument);
}

TEST_P(GameInterfaceTest,
       loadPluginsShouldResolveRelativePathsRelativeToDataPath) {
  const auto relativePath = ".." / dataPath.filename() / blankEsm;

  handle_->LoadPlugins(std::vector<std::filesystem::path>({relativePath}),
                       true);

  EXPECT_NE(nullptr, handle_->GetPlugin(blankEsm));
}

TEST_P(GameInterfaceTest, loadPluginsShouldUseAbsolutePathsAsGiven) {
  const auto absolutePath = dataPath / std::filesystem::u8path(blankEsm);

  handle_->LoadPlugins(std::vector<std::filesystem::path>({absolutePath}),
                       true);

  EXPECT_NE(nullptr, handle_->GetPlugin(blankEsm));
}

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldThrowIfFullyLoadingAPluginWithAMissingMasterIfGameIsMorrowindOrStarfield) {
  const auto pluginName =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsm;

  std::filesystem::remove(dataPath / pluginName);

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::starfield) {
    try {
      handle_->LoadPlugins({blankMasterDependentEsm}, false);
      FAIL();
    } catch (const std::system_error& e) {
      EXPECT_EQ(ESP_ERROR_PLUGIN_METADATA_NOT_FOUND, e.code().value());
      EXPECT_EQ(esplugin_category(), e.code().category());
    }
  } else {
    handle_->LoadPlugins({blankMasterDependentEsm}, false);

    EXPECT_NE(nullptr, handle_->GetPlugin(blankMasterDependentEsm));
  }
}

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldThrowIfAPluginHasAMasterThatIsNotInTheInputAndIsNotAlreadyLoadedAndGameIsMorrowindOrStarfield) {
  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::starfield) {
    try {
      handle_->LoadPlugins({blankMasterDependentEsm}, false);
      FAIL();
    } catch (const std::system_error& e) {
      EXPECT_EQ(ESP_ERROR_PLUGIN_METADATA_NOT_FOUND, e.code().value());
      EXPECT_EQ(esplugin_category(), e.code().category());
    }
  } else {
    handle_->LoadPlugins({blankMasterDependentEsm}, false);

    EXPECT_NE(nullptr, handle_->GetPlugin(blankMasterDependentEsm));
  }
}

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldNotThrowIfAPluginHasAMasterThatIsNotInTheInputButIsAlreadyLoaded) {
  const auto pluginName =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsm;

  handle_->LoadPlugins({pluginName}, true);

  handle_->LoadPlugins({blankMasterDependentEsm}, false);

  EXPECT_NE(nullptr, handle_->GetPlugin(blankMasterDependentEsm));
}

TEST_P(GameInterfaceTest,
       sortPluginsWithNoLoadedPluginsShouldReturnAnEmptyList) {
  const auto sorted = handle_->SortPlugins(handle_->GetLoadOrder());

  EXPECT_TRUE(sorted.empty());
}

TEST_P(GameInterfaceTest, sortPluginsShouldOnlySortTheGivenPlugins) {
  handle_->LoadPlugins(GetInstalledPlugins(), false);

  std::vector<std::string> plugins{blankEsp, blankDifferentEsp};
  const auto sorted = handle_->SortPlugins(plugins);

  EXPECT_EQ(plugins, sorted);
}

TEST_P(GameInterfaceTest,
       sortingShouldNotMakeUnnecessaryChangesToAnExistingLoadOrder) {
  std::filesystem::remove(dataPath / std::filesystem::u8path(nonAsciiEsm));

  handle_->LoadCurrentLoadOrderState();

  auto plugins = GetInstalledPlugins();
  handle_->LoadPlugins({plugins.front()}, true);
  plugins.erase(plugins.begin());
  handle_->LoadPlugins(plugins, false);

  std::vector<std::string> expectedSortedOrder;
  if (GetParam() == GameType::openmw) {
    // The existing load order for OpenMW doesn't have plugins loading after
    // their masters, because the game doesn't enforce that, and the test
    // setup cannot enforce the positions of inactive plugins.
    expectedSortedOrder = {
        blankDifferentEsm,
        blankDifferentMasterDependentEsm,
        blankDifferentEsp,
        blankDifferentPluginDependentEsp,
        blankEsm,
        blankMasterDependentEsm,
        blankMasterDependentEsp,
        blankEsp,
        blankPluginDependentEsp,
        masterFile,
        blankDifferentMasterDependentEsp,
    };
  } else {
    expectedSortedOrder = getLoadOrder();
  }

  // Check stability by running the sort 100 times.
  for (int i = 0; i < 100; i++) {
    auto input = handle_->GetLoadOrder();
    auto sorted = handle_->SortPlugins(handle_->GetLoadOrder());
    ASSERT_EQ(expectedSortedOrder, sorted) << " for sort " << i;
  }
}

TEST_P(GameInterfaceTest, sortPluginsShouldThrowIfAGivenPluginIsNotLoaded) {
  std::vector<std::string> plugins{blankEsp, blankDifferentEsp};

  EXPECT_THROW(handle_->SortPlugins(plugins), std::runtime_error);
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
       isPluginActiveShouldReturnTrueIfTheGivenPluginIsActive) {
  handle_->LoadCurrentLoadOrderState();

  EXPECT_TRUE(handle_->IsPluginActive(blankEsm));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldReturnFalseIfTheGivenPluginIsNotActive) {
  handle_->LoadCurrentLoadOrderState();
  EXPECT_FALSE(handle_->IsPluginActive(blankEsp));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldActivePluginAsActiveWithHeaderLoaded) {
  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({blankEsm}, true));

  EXPECT_TRUE(handle_->IsPluginActive(blankEsm));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldInactivePluginAsInactiveWithHeaderLoaded) {
  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({blankEsp}, true));

  EXPECT_FALSE(handle_->IsPluginActive(blankEsp));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldActivePluginAsActiveWhenFullyLoaded) {
  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({blankEsm}, false));

  EXPECT_TRUE(handle_->IsPluginActive(blankEsm));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldInactivePluginAsInactiveWhenFullyLoaded) {
  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({blankEsp}, false));

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
