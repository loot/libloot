/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2026 Oliver Hamlet

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
  GameInterfaceTest() {}

  void createNonPluginFile(std::string_view filename) {
    const auto path = dataPath / std::filesystem::u8path(filename);

    std::ofstream out(path);
    out << "This isn't a valid plugin file.";
    out.close();

    ASSERT_TRUE(exists(path));
  }
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
  PluginMetadata metadata(BLANK_ESM);
  metadata.SetLoadAfterFiles({File("plugin.esp", "", "file(\"plugin.esp\")")});
  handle_->GetDatabase().SetPluginUserMetadata(metadata);

  auto evaluatedMetadata =
      handle_->GetDatabase().GetPluginUserMetadata(BLANK_ESM, true);
  EXPECT_FALSE(evaluatedMetadata.has_value());

  const auto dataFilePath = gamePath.parent_path() / "Data" / "plugin.esp";
  touch(dataFilePath);
  handle_->SetAdditionalDataPaths({dataFilePath.parent_path()});

  evaluatedMetadata =
      handle_->GetDatabase().GetPluginUserMetadata(BLANK_ESM, true);
  EXPECT_FALSE(evaluatedMetadata.value().GetLoadAfterFiles().empty());
}

TEST_P(GameInterfaceTest,
       setAdditionalDataPathsShouldUpdateWhereLoadOrderPluginsAreFound) {
  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed.
  handle_->SetAdditionalDataPaths({});
  setLoadOrder({});
  handle_->LoadCurrentLoadOrderState();
  auto loadOrder = handle_->GetLoadOrder();

  const auto filename = "plugin.esp";
  const auto dataFilePath = gamePath.parent_path() / "Data" / filename;
  std::filesystem::create_directories(dataFilePath.parent_path());
  std::filesystem::copy_file(getSourcePluginsPath() / BLANK_ESP, dataFilePath);
  ASSERT_TRUE(std::filesystem::exists(dataFilePath));

  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_ESP, filename);
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
  copyPlugin(BLANK_ESP);

  EXPECT_TRUE(handle_->IsValidPlugin(BLANK_ESP));
}

TEST_P(GameInterfaceTest,
       isValidPluginShouldReturnTrueForAValidNonAsciiPlugin) {
  copyPlugin(BLANK_ESP, NON_ASCII_ESP);

  EXPECT_TRUE(handle_->IsValidPlugin(std::filesystem::u8path(NON_ASCII_ESP)));
}

TEST_P(GameInterfaceTest, isValidPluginShouldReturnFalseForANonPluginFile) {
  createNonPluginFile(NON_PLUGIN_FILE);

  EXPECT_FALSE(handle_->IsValidPlugin(NON_PLUGIN_FILE));
}

TEST_P(GameInterfaceTest, isValidPluginShouldReturnFalseForAnEmptyFile) {
  // Write out an empty file.
  const auto emptyFilePath = dataPath / "EmptyFile.esm";
  touch(emptyFilePath);
  ASSERT_TRUE(std::filesystem::exists(emptyFilePath));

  EXPECT_FALSE(handle_->IsValidPlugin(emptyFilePath));
}

TEST_P(GameInterfaceTest,
       isValidPluginShouldResolveRelativePathsRelativeToDataPath) {
  copyPlugin(BLANK_ESP);

  const auto path = ".." / dataPath.filename() / BLANK_ESP;

  EXPECT_TRUE(handle_->IsValidPlugin(path));
}

TEST_P(GameInterfaceTest, isValidPluginShouldUseAbsolutePathsAsGiven) {
  copyPlugin(BLANK_ESP);

  ASSERT_TRUE(dataPath.is_absolute());

  const auto path = dataPath / std::filesystem::u8path(BLANK_ESP);

  EXPECT_TRUE(handle_->IsValidPlugin(path));
}

TEST_P(
    GameInterfaceTest,
    isValidPluginShouldTryGhostedPathIfGivenPluginDoesNotExistExceptForOpenMW) {
  const auto ghostedName = std::string(BLANK_ESP) + ".ghost";
  copyPlugin(BLANK_ESP, ghostedName);

  if (GetParam() == GameType::openmw) {
    EXPECT_FALSE(handle_->IsValidPlugin(ghostedName));
    EXPECT_FALSE(handle_->IsValidPlugin(BLANK_ESP));
  } else {
    EXPECT_TRUE(handle_->IsValidPlugin(ghostedName));
    EXPECT_TRUE(handle_->IsValidPlugin(BLANK_ESP));
  }
}

TEST_P(GameInterfaceTest, loadPluginsShouldTrimDotGhostFileExtensions) {
  const auto ghostedName = std::string(BLANK_ESP) + ".ghost";
  copyPlugin(BLANK_ESP, ghostedName);

  if (GetParam() == GameType::openmw) {
    // Ghosting is not supported for OpenMW.
    EXPECT_THROW(handle_->LoadPlugins({ghostedName}, true),
                 std::invalid_argument);
    return;
  } else {
    handle_->LoadPlugins({ghostedName}, true);
  }

  EXPECT_EQ(1, handle_->GetLoadedPlugins().size());

  const auto plugin = handle_->GetPlugin(BLANK_ESP);
  ASSERT_NE(nullptr, plugin);
  EXPECT_EQ(BLANK_ESP, plugin->GetName());
}

TEST_P(GameInterfaceTest, loadPluginsShouldResolveGhostedPlugins) {
  const auto ghostedName = std::string(BLANK_ESP) + ".ghost";
  copyPlugin(BLANK_ESP, ghostedName);

  if (GetParam() == GameType::openmw) {
    // Ghosting is not supported for OpenMW.
    EXPECT_THROW(handle_->LoadPlugins({BLANK_ESP}, true),
                 std::invalid_argument);
    return;
  } else {
    handle_->LoadPlugins({BLANK_ESP}, true);
  }

  EXPECT_EQ(1, handle_->GetLoadedPlugins().size());

  const auto plugin = handle_->GetPlugin(BLANK_ESP);
  ASSERT_NE(nullptr, plugin);
  EXPECT_EQ(BLANK_ESP, plugin->GetName());
}

TEST_P(GameInterfaceTest,
       loadPluginsWithHeadersOnlyTrueShouldLoadTheHeadersOfAllGivenPlugins) {
  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);
  copyPlugin(BLANK_ESP);

  handle_->LoadPlugins({pluginName, BLANK_ESP}, true);

  EXPECT_EQ(2, handle_->GetLoadedPlugins().size());

  // Check that one plugin's header has been read.
  auto plugin = handle_->GetPlugin(pluginName);
  ASSERT_NE(nullptr, plugin);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that only the header has been read.
  EXPECT_FALSE(plugin->GetCRC());
}

TEST_P(GameInterfaceTest,
       loadPluginsWithHeadersOnlyFalseShouldFullyLoadAllGivenPlugins) {
  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);
  copyPlugin(BLANK_ESP);

  handle_->LoadPlugins({pluginName, BLANK_ESP}, false);

  EXPECT_EQ(2, handle_->GetLoadedPlugins().size());

  // Check that one plugin's header has been read.
  auto plugin = handle_->GetPlugin(pluginName);
  ASSERT_NE(nullptr, plugin);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that not only the header has been read.
  EXPECT_EQ(getBlankEsmCrc(), plugin->GetCRC().value());
}

TEST_P(GameInterfaceTest, loadPluginsWithANonAsciiPluginShouldLoadIt) {
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_FULL_ESM, NON_ASCII_ESP);
  } else {
    copyPlugin(BLANK_ESM, NON_ASCII_ESP);
  }

  handle_->LoadPlugins({std::filesystem::u8path(NON_ASCII_ESP)}, false);
  EXPECT_EQ(1, handle_->GetLoadedPlugins().size());

  // Check that one plugin's header has been read.
  auto plugin = handle_->GetPlugin(NON_ASCII_ESP);
  ASSERT_NE(nullptr, plugin);
  EXPECT_EQ("5.0", plugin->GetVersion().value());

  // Check that not only the header has been read.
  EXPECT_EQ(getBlankEsmCrc(), plugin->GetCRC().value());
}

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldNotThrowIfAFilenameHasNonWindows1252EncodableCharacters) {
  const auto pluginName =
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.esp";
  copyPlugin(BLANK_ESP, pluginName);

  EXPECT_NO_THROW(
      handle_->LoadPlugins({std::filesystem::u8path(pluginName)}, false));
}

TEST_P(GameInterfaceTest,
       loadPluginsWithANonPluginShouldNotAddItToTheLoadedPlugins) {
  createNonPluginFile(NON_PLUGIN_FILE);

  try {
    handle_->LoadPlugins({NON_PLUGIN_FILE}, false);
    FAIL();
  } catch (std::invalid_argument& e) {
    EXPECT_TRUE(startsWith(
        e.what(), "failed validation of input plugin paths: the file at \""));
    EXPECT_TRUE(endsWith(
        e.what(), "NotAPlugin.esm\" does not have a valid plugin header"));
  }

  ASSERT_TRUE(handle_->GetLoadedPlugins().empty());
}

TEST_P(GameInterfaceTest,
       loadPluginsWithAnInvalidPluginShouldNotAddItToTheLoadedPlugins) {
  copyPlugin(BLANK_ESP, INVALID_PLUGIN);

  std::ofstream out(dataPath / INVALID_PLUGIN, std::fstream::app);
  out << "GRUP0";
  out.close();

  ASSERT_NO_THROW(handle_->LoadPlugins({INVALID_PLUGIN}, false));

  ASSERT_TRUE(handle_->GetLoadedPlugins().empty());
}

TEST_P(GameInterfaceTest, loadPluginsShouldNotClearThePluginsCache) {
  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);
  copyPlugin(BLANK_ESP);

  handle_->LoadPlugins({std::filesystem::u8path(pluginName)}, true);
  ASSERT_EQ(1, handle_->GetLoadedPlugins().size());
  ASSERT_NE(nullptr, handle_->GetPlugin(pluginName));

  handle_->LoadPlugins({std::filesystem::u8path(BLANK_ESP)}, true);

  EXPECT_EQ(2, handle_->GetLoadedPlugins().size());
  ASSERT_NE(nullptr, handle_->GetPlugin(pluginName));
  ASSERT_NE(nullptr, handle_->GetPlugin(BLANK_ESP));
}

TEST_P(GameInterfaceTest,
       loadPluginsShouldReplaceCacheEntriesForTheGivenPlugins) {
  copyPlugin(BLANK_ESP);

  handle_->LoadPlugins({std::filesystem::u8path(BLANK_ESP)}, true);
  const auto pointer = handle_->GetPlugin(BLANK_ESP);
  ASSERT_NE(nullptr, pointer);

  handle_->LoadPlugins({std::filesystem::u8path(BLANK_ESP)}, false);

  const auto newPointer = handle_->GetPlugin(BLANK_ESP);
  ASSERT_NE(nullptr, newPointer);
  EXPECT_NE(pointer, newPointer);
}

TEST_P(GameInterfaceTest,
       loadPluginsShouldThrowIfGivenVectorElementsWithTheSameFilename) {
  copyPlugin(BLANK_ESP);

  const auto dataPluginPath = dataPath / std::filesystem::u8path(BLANK_ESP);
  const auto sourcePluginPath =
      getSourcePluginsPath() / std::filesystem::u8path(BLANK_ESP);

  EXPECT_THROW(handle_->LoadPlugins(std::vector<std::filesystem::path>(
                                        {dataPluginPath, sourcePluginPath}),
                                    true),
               std::invalid_argument);
}

TEST_P(GameInterfaceTest,
       loadPluginsShouldResolveRelativePathsRelativeToDataPath) {
  copyPlugin(BLANK_ESP);

  const auto relativePath = ".." / dataPath.filename() / BLANK_ESP;

  handle_->LoadPlugins(std::vector<std::filesystem::path>({relativePath}),
                       true);

  EXPECT_NE(nullptr, handle_->GetPlugin(BLANK_ESP));
}

TEST_P(GameInterfaceTest, loadPluginsShouldUseAbsolutePathsAsGiven) {
  copyPlugin(BLANK_ESP);

  const auto absolutePath = dataPath / std::filesystem::u8path(BLANK_ESP);

  handle_->LoadPlugins(std::vector<std::filesystem::path>({absolutePath}),
                       true);

  EXPECT_NE(nullptr, handle_->GetPlugin(BLANK_ESP));
}

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldThrowIfFullyLoadingAPluginWithAMissingMasterIfGameIsMorrowindOrStarfield) {
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_OVERRIDE_FULL_ESM, BLANK_MASTER_DEPENDENT_ESM);
  } else {
    copyPlugin(BLANK_MASTER_DEPENDENT_ESM);
  }

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::starfield) {
    EXPECT_THROW(handle_->LoadPlugins({BLANK_MASTER_DEPENDENT_ESM}, false),
                 PluginNotLoadedError);
  } else {
    handle_->LoadPlugins({BLANK_MASTER_DEPENDENT_ESM}, false);

    EXPECT_NE(nullptr, handle_->GetPlugin(BLANK_MASTER_DEPENDENT_ESM));
  }
}

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldThrowIfAPluginHasAMasterThatIsNotInTheInputAndIsNotAlreadyLoadedAndGameIsMorrowindOrStarfield) {
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_OVERRIDE_FULL_ESM, BLANK_MASTER_DEPENDENT_ESM);
  } else {
    copyPlugin(BLANK_MASTER_DEPENDENT_ESM);
  }

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::starfield) {
    EXPECT_THROW(handle_->LoadPlugins({BLANK_MASTER_DEPENDENT_ESM}, false),
                 PluginNotLoadedError);
  } else {
    handle_->LoadPlugins({BLANK_MASTER_DEPENDENT_ESM}, false);

    EXPECT_NE(nullptr, handle_->GetPlugin(BLANK_MASTER_DEPENDENT_ESM));
  }
}

TEST_P(
    GameInterfaceTest,
    loadPluginsShouldNotThrowIfAPluginHasAMasterThatIsNotInTheInputButIsAlreadyLoaded) {
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_FULL_ESM);
    copyPlugin(BLANK_OVERRIDE_FULL_ESM, BLANK_MASTER_DEPENDENT_ESM);

    handle_->LoadPlugins({BLANK_FULL_ESM}, true);
  } else {
    copyPlugin(BLANK_ESM);
    copyPlugin(BLANK_MASTER_DEPENDENT_ESM);

    handle_->LoadPlugins({BLANK_ESM}, true);
  }

  handle_->LoadPlugins({BLANK_MASTER_DEPENDENT_ESM}, false);

  EXPECT_NE(nullptr, handle_->GetPlugin(BLANK_MASTER_DEPENDENT_ESM));
}

TEST_P(GameInterfaceTest,
       sortPluginsWithNoLoadedPluginsShouldReturnAnEmptyList) {
  const auto sorted = handle_->SortPlugins(handle_->GetLoadOrder());

  EXPECT_TRUE(sorted.empty());
}

TEST_P(GameInterfaceTest, sortPluginsShouldOnlySortTheGivenPlugins) {
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_FULL_ESM, BLANK_ESM);
    copyPlugin(BLANK_ESP);
    copyPlugin(BLANK_ESP, BLANK_DIFFERENT_ESP);
  } else {
    copyPlugin(BLANK_ESM);
    copyPlugin(BLANK_ESP);
    copyPlugin(BLANK_DIFFERENT_ESP);
  }

  handle_->LoadPlugins({BLANK_ESM, BLANK_ESP, BLANK_DIFFERENT_ESP}, false);

  std::vector<std::string> plugins{std::string(BLANK_ESP),
                                   std::string(BLANK_DIFFERENT_ESP)};
  const auto sorted = handle_->SortPlugins(plugins);

  EXPECT_EQ(plugins, sorted);
}

TEST_P(GameInterfaceTest,
       sortingShouldNotMakeUnnecessaryChangesToAnExistingLoadOrder) {
  std::vector<std::string> initialOrder;
  if (GetParam() == GameType::starfield) {
    initialOrder = {
        std::string(BLANK_FULL_ESM),
        std::string(BLANK_OVERRIDE_FULL_ESM),
        std::string(BLANK_ESP),
        std::string(BLANK_OVERRIDE_ESP),
    };
  } else {
    initialOrder = {
        std::string(BLANK_ESM),
        std::string(BLANK_DIFFERENT_ESM),
        std::string(BLANK_MASTER_DEPENDENT_ESM),
        std::string(BLANK_DIFFERENT_MASTER_DEPENDENT_ESM),
        std::string(BLANK_ESP),
        std::string(BLANK_DIFFERENT_ESP),
        std::string(BLANK_MASTER_DEPENDENT_ESP),
        std::string(BLANK_DIFFERENT_MASTER_DEPENDENT_ESP),
        std::string(BLANK_PLUGIN_DEPENDENT_ESP),
        std::string(BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP),
    };
  }

  std::vector<std::filesystem::path> pluginsToLoad;
  for (const auto& plugin : initialOrder) {
    copyPlugin(plugin);
    pluginsToLoad.push_back(std::filesystem::u8path(plugin));
  }

  handle_->LoadPlugins(pluginsToLoad, false);

  // Check stability by running the sort 100 times.
  for (int i = 0; i < 100; i++) {
    const auto sorted = handle_->SortPlugins(initialOrder);
    ASSERT_EQ(initialOrder, sorted) << " for sort " << i;
  }
}

TEST_P(GameInterfaceTest, sortPluginsShouldThrowIfAGivenPluginIsNotLoaded) {
  std::vector<std::string> plugins{std::string(BLANK_ESP),
                                   std::string(BLANK_DIFFERENT_ESP)};

  try {
    handle_->SortPlugins(plugins);
    FAIL();
  } catch (PluginNotLoadedError& e) {
    EXPECT_STREQ("The plugin \"Blank.esp\" has not been loaded", e.what());
  }
}

TEST_P(GameInterfaceTest, sortPluginsShouldThrowIfACyclicInteractionOccurs) {
  copyPlugin(BLANK_ESP);
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_ESP, BLANK_DIFFERENT_ESP);
  } else {
    copyPlugin(BLANK_DIFFERENT_ESP);
  }

  std::vector<std::string> plugins{std::string(BLANK_ESP),
                                   std::string(BLANK_DIFFERENT_ESP)};
  handle_->LoadPlugins({BLANK_ESP, BLANK_DIFFERENT_ESP}, false);

  auto& db = handle_->GetDatabase();
  PluginMetadata blankEspMetadata(BLANK_ESP);
  blankEspMetadata.SetLoadAfterFiles({File(BLANK_DIFFERENT_ESP)});
  db.SetPluginUserMetadata(blankEspMetadata);

  PluginMetadata blankDifferentEspMetadata(BLANK_DIFFERENT_ESP);
  blankDifferentEspMetadata.SetLoadAfterFiles({File(BLANK_ESP)});
  db.SetPluginUserMetadata(blankDifferentEspMetadata);

  try {
    handle_->SortPlugins(plugins);
    FAIL();
  } catch (CyclicInteractionError& e) {
    const auto cycle = e.GetCycle();
    ASSERT_EQ(2, cycle.size());
    EXPECT_EQ(BLANK_DIFFERENT_ESP, cycle[0].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter, cycle[0].GetTypeOfEdgeToNextVertex());
    EXPECT_EQ(BLANK_ESP, cycle[1].GetName());
    EXPECT_EQ(EdgeType::userLoadAfter, cycle[1].GetTypeOfEdgeToNextVertex());
  }
}

TEST_P(GameInterfaceTest, clearLoadedPluginsShouldClearThePluginsCache) {
  copyPlugin(BLANK_ESP);

  handle_->LoadPlugins({std::filesystem::u8path(BLANK_ESP)}, true);
  const auto pointer = handle_->GetPlugin(BLANK_ESP);
  ASSERT_NE(nullptr, pointer);

  handle_->ClearLoadedPlugins();

  EXPECT_EQ(nullptr, handle_->GetPlugin(BLANK_ESP));
}

TEST_P(GameInterfaceTest, getPluginThatIsNotCachedShouldReturnANullPointer) {
  EXPECT_FALSE(handle_->GetPlugin(BLANK_ESM));
}

TEST_P(GameInterfaceTest,
       getPluginReturnsDifferentPointersForConsecutiveCallsGivenTheSamePlugin) {
  copyPlugin(BLANK_ESP);

  handle_->LoadPlugins({std::filesystem::u8path(BLANK_ESP)}, true);

  const auto pointer1 = handle_->GetPlugin(BLANK_ESP);
  const auto pointer2 = handle_->GetPlugin(BLANK_ESP);

  EXPECT_NE(pointer1, pointer2);
}

TEST_P(GameInterfaceTest, getPluginShouldStripGhostExtension) {
  if (GetParam() == GameType::openmw) {
    return;
  }

  const auto ghostedName = std::string(BLANK_ESP) + ".ghost";
  copyPlugin(BLANK_ESP, ghostedName);

  handle_->LoadPlugins({BLANK_ESP}, true);

  EXPECT_EQ(1, handle_->GetLoadedPlugins().size());

  const auto plugin = handle_->GetPlugin(ghostedName);
  ASSERT_NE(nullptr, plugin);
  EXPECT_EQ(BLANK_ESP, plugin->GetName());
}

TEST_P(GameInterfaceTest,
       getLoadedPluginsShouldReturnAnEmptySetIfNoneHaveBeenLoaded) {
  EXPECT_TRUE(handle_->GetLoadedPlugins().empty());
}

TEST_P(GameInterfaceTest,
       getLoadedPluginsReturnsDifferentPointersForConsecutiveCalls) {
  copyPlugin(BLANK_ESP);

  handle_->LoadPlugins({std::filesystem::u8path(BLANK_ESP)}, true);

  const auto pointers1 = handle_->GetLoadedPlugins();
  const auto pointers2 = handle_->GetLoadedPlugins();

  EXPECT_NE(pointers1, pointers2);
}

TEST_P(GameInterfaceTest, sortPluginsShouldSucceedIfPassedValidArguments) {
  std::vector<std::string> initialOrder;
  std::vector<std::string> expectedOrder;
  if (GetParam() == GameType::starfield) {
    initialOrder = {
        std::string(BLANK_FULL_ESM),
        std::string(BLANK_OVERRIDE_FULL_ESM),
        std::string(BLANK_ESP),
        std::string(BLANK_OVERRIDE_ESP),
    };
    expectedOrder = {
        std::string(BLANK_FULL_ESM),
        std::string(BLANK_OVERRIDE_FULL_ESM),
        std::string(BLANK_OVERRIDE_ESP),
        std::string(BLANK_ESP),
    };
  } else {
    initialOrder = {
        std::string(BLANK_ESM),
        std::string(BLANK_DIFFERENT_ESM),
        std::string(BLANK_MASTER_DEPENDENT_ESM),
        std::string(BLANK_DIFFERENT_MASTER_DEPENDENT_ESM),
        std::string(BLANK_ESP),
        std::string(BLANK_DIFFERENT_ESP),
        std::string(BLANK_MASTER_DEPENDENT_ESP),
        std::string(BLANK_DIFFERENT_MASTER_DEPENDENT_ESP),
        std::string(BLANK_PLUGIN_DEPENDENT_ESP),
        std::string(BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP),
    };
    expectedOrder = {
        std::string(BLANK_ESM),
        std::string(BLANK_MASTER_DEPENDENT_ESM),
        std::string(BLANK_DIFFERENT_ESM),
        std::string(BLANK_DIFFERENT_MASTER_DEPENDENT_ESM),
        std::string(BLANK_MASTER_DEPENDENT_ESP),
        std::string(BLANK_DIFFERENT_MASTER_DEPENDENT_ESP),
        std::string(BLANK_ESP),
        std::string(BLANK_PLUGIN_DEPENDENT_ESP),
        std::string(BLANK_DIFFERENT_ESP),
        std::string(BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP),
    };
  }

  if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
    initialOrder.push_back(std::string(BLANK_ESL));
    expectedOrder.insert(expectedOrder.begin() + 4, std::string(BLANK_ESL));
  }

  std::vector<std::filesystem::path> pluginsToLoad;
  for (const auto& plugin : initialOrder) {
    copyPlugin(plugin);
    pluginsToLoad.push_back(std::filesystem::u8path(plugin));
  }

  handle_->LoadPlugins(pluginsToLoad, false);

  ASSERT_NO_THROW(GenerateMasterlist());
  ASSERT_NO_THROW(handle_->GetDatabase().LoadMasterlist(masterlistPath));

  const auto actualOrder = handle_->SortPlugins(initialOrder);

  EXPECT_EQ(expectedOrder, actualOrder);
}

TEST_P(GameInterfaceTest, sortPluginsShouldSupportGhostedPlugins) {
  if (GetParam() == GameType::openmw) {
    return;
  }

  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;

  copyPlugin(pluginName);
  copyPlugin(BLANK_ESP, "Blank.esp.ghost");

  handle_->LoadPlugins({pluginName, BLANK_ESP}, false);

  const auto sortedOrder = handle_->SortPlugins(
      {std::string(pluginName) + ".ghost", std::string(BLANK_ESP)});

  std::vector<std::string> expectedOrder{std::string(pluginName),
                                         std::string(BLANK_ESP)};

  EXPECT_EQ(expectedOrder, sortedOrder);
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldReturnTrueIfTheGivenPluginIsActive) {
  copyPlugin(BLANK_ESP);
  setLoadOrder({{std::string(BLANK_ESP), true}});

  handle_->LoadCurrentLoadOrderState();

  EXPECT_TRUE(handle_->IsPluginActive(std::string(BLANK_ESP)));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldReturnFalseIfTheGivenPluginIsNotActive) {
  copyPlugin(BLANK_ESP);
  setLoadOrder({});

  handle_->LoadCurrentLoadOrderState();

  EXPECT_FALSE(handle_->IsPluginActive(std::string(BLANK_ESP)));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldActivePluginAsActiveWithHeaderLoaded) {
  copyPlugin(BLANK_ESP);
  setLoadOrder({{std::string(BLANK_ESP), true}});

  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({BLANK_ESP}, true));

  EXPECT_TRUE(handle_->IsPluginActive(std::string(BLANK_ESP)));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldInactivePluginAsInactiveWithHeaderLoaded) {
  copyPlugin(BLANK_ESP);
  setLoadOrder({});

  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({BLANK_ESP}, true));

  EXPECT_FALSE(handle_->IsPluginActive(std::string(BLANK_ESP)));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldActivePluginAsActiveWhenFullyLoaded) {
  copyPlugin(BLANK_ESP);
  setLoadOrder({{std::string(BLANK_ESP), true}});

  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({BLANK_ESP}, false));

  EXPECT_TRUE(handle_->IsPluginActive(std::string(BLANK_ESP)));
}

TEST_P(GameInterfaceTest,
       isPluginActiveShouldInactivePluginAsInactiveWhenFullyLoaded) {
  copyPlugin(BLANK_ESP);
  setLoadOrder({});

  handle_->LoadCurrentLoadOrderState();

  ASSERT_NO_THROW(handle_->LoadPlugins({BLANK_ESP}, false));

  EXPECT_FALSE(handle_->IsPluginActive(std::string(BLANK_ESP)));
}

TEST_P(GameInterfaceTest, getLoadOrderShouldReturnTheCurrentLoadOrder) {
  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed.
  if (GetParam() == GameType::starfield || GetParam() == GameType::fo4) {
    handle_->SetAdditionalDataPaths({});
  }

  std::vector<std::pair<std::string, bool>> loadOrder;
  if (GetParam() == GameType::starfield) {
    loadOrder = {{std::string(BLANK_FULL_ESM), true},
                 {std::string(BLANK_ESP), false},
                 {std::string(BLANK_OVERRIDE_ESP), false}};
  } else {
    loadOrder = {{std::string(BLANK_ESM), true},
                 {std::string(BLANK_DIFFERENT_ESM), true},
                 {std::string(BLANK_ESP), false},
                 {std::string(BLANK_MASTER_DEPENDENT_ESP), false}};
  }

  for (const auto& [plugin, isActive] : loadOrder) {
    copyPlugin(plugin);
  }
  setLoadOrder(loadOrder);

  handle_->LoadCurrentLoadOrderState();

  std::vector<std::string> expectedLoadOrder;
  if (GetParam() == GameType::openmw) {
    // OpenMW doesn't allow the load order of inactive plugins to be persisted.
    expectedLoadOrder = {std::string(BLANK_ESM),
                         std::string(BLANK_DIFFERENT_ESM),
                         std::string(BLANK_MASTER_DEPENDENT_ESP),
                         std::string(BLANK_ESP)};
  } else {
    for (const auto& [plugin, isActive] : loadOrder) {
      expectedLoadOrder.push_back(plugin);
    }
  }

  ASSERT_EQ(expectedLoadOrder, handle_->GetLoadOrder());
}

TEST_P(GameInterfaceTest, getLoadOrderShouldStripGhostExtensionsFromPlugins) {
  if (GetParam() == GameType::openmw) {
    return;
  }

  const auto ghostedName = "Blank.esp.ghost";
  copyPlugin(BLANK_ESP, ghostedName);

  setLoadOrder({{std::string(ghostedName), true}});

  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed.
  if (GetParam() == GameType::starfield || GetParam() == GameType::fo4) {
    handle_->SetAdditionalDataPaths({});
  }

  handle_->LoadCurrentLoadOrderState();

  ASSERT_EQ(std::vector<std::string>({std::string(BLANK_ESP)}),
            handle_->GetLoadOrder());
}

TEST_P(GameInterfaceTest, setLoadOrderShouldSetTheLoadOrder) {
  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed.
  if (GetParam() == GameType::starfield || GetParam() == GameType::fo4) {
    handle_->SetAdditionalDataPaths({});
  }

  std::vector<std::pair<std::string, bool>> initialLoadOrder;
  std::vector<std::string> newLoadOrder;
  if (GetParam() == GameType::starfield) {
    initialLoadOrder = {{std::string(BLANK_FULL_ESM), true},
                        {std::string(BLANK_ESP), false},
                        {std::string(BLANK_OVERRIDE_ESP), false}};
    newLoadOrder = {std::string(BLANK_FULL_ESM),
                    std::string(BLANK_OVERRIDE_ESP),
                    std::string(BLANK_ESP)};
  } else {
    initialLoadOrder = {{std::string(BLANK_ESM), true},
                        {std::string(BLANK_ESP), false},
                        {std::string(BLANK_DIFFERENT_ESP), false}};
    newLoadOrder = {std::string(BLANK_ESM),
                    std::string(BLANK_DIFFERENT_ESP),
                    std::string(BLANK_ESP)};
  }

  for (const auto& [plugin, isActive] : initialLoadOrder) {
    copyPlugin(plugin);
  }

  setLoadOrder(initialLoadOrder);
  handle_->LoadCurrentLoadOrderState();

  EXPECT_NO_THROW(handle_->SetLoadOrder(newLoadOrder));

  EXPECT_EQ(newLoadOrder, handle_->GetLoadOrder());

  // It's not possible to persist the load order of inactive plugins for
  // OpenMW.
  if (GetParam() != GameType::openmw) {
    EXPECT_EQ(newLoadOrder, getLoadOrder());
  }
}

TEST_P(GameInterfaceTest, setLoadOrderShouldStripGhostExtensionsFromPlugins) {
  if (GetParam() == GameType::openmw) {
    return;
  }

  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;

  copyPlugin(pluginName);
  copyPlugin(BLANK_ESP, "Blank.esp.ghost");

  // Set no additional data paths to avoid picking up non-test plugins on PCs
  // which have Starfield or Fallout 4 installed.
  if (GetParam() == GameType::starfield || GetParam() == GameType::fo4) {
    handle_->SetAdditionalDataPaths({});
  }

  EXPECT_NO_THROW(handle_->SetLoadOrder(
      {std::string(pluginName) + ".ghost", std::string(BLANK_ESP)}));

  ASSERT_EQ(std::vector<std::string>(
                {std::string(pluginName), std::string(BLANK_ESP)}),
            handle_->GetLoadOrder());
}
}
}

#endif
