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

#ifndef LOOT_TESTS_API_INTERFACE_PLUGIN_INTERFACE_TEST
#define LOOT_TESTS_API_INTERFACE_PLUGIN_INTERFACE_TEST

#include "loot/api.h"
#include "tests/api/interface/api_game_operations_test.h"

namespace loot::test {
class PluginInterfaceTest : public ApiGameOperationsTest {
protected:
  static constexpr std::string_view OTHER_NON_ASCII_ESP =
      u8"other non\u00C1scii.esp";

  PluginInterfaceTest() {}

  void SetUpTestArchives() {
    // Copy across archive files.
    const auto sourceArchivesPath = getSourceArchivesPath(GetParam());
    const auto blankArchive = "Blank" + GetArchiveFileExtension(GetParam());

    if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr) {
      copyDataFile(sourceArchivesPath, "Blank - Main.ba2");
      copyDataFile(sourceArchivesPath, "Blank - Textures.ba2");
      copyDataFile(sourceArchivesPath,
                   "Blank - Main.ba2",
                   "Blank - Master Dependent - Main.ba2");
    } else if (GetParam() == GameType::starfield) {
      copyDataFile(sourceArchivesPath, "Blank - Main.ba2");
      copyDataFile(sourceArchivesPath, "Blank - Textures.ba2");
      copyDataFile(sourceArchivesPath,
                   "Blank - Main.ba2",
                   "Blank - Master Dependent - Main.ba2");

      copyDataFile(
          sourceArchivesPath, "Blank - Main.ba2", "Blank.full - Main.ba2");
      copyDataFile(sourceArchivesPath,
                   "Blank - Textures.ba2",
                   "Blank.full - Textures.ba2");
    } else if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
      touch(dataPath / blankArchive);
      touch(dataPath / "Blank - Master Dependent.bsa");
    } else {
      copyDataFile(sourceArchivesPath, blankArchive);

      // Also create a copy for Blank - Master Dependent.esp to test overlap.
      copyDataFile(
          sourceArchivesPath, blankArchive, "Blank - Master Dependent.bsa");
    }

    // Create dummy archive files.
    const auto blankSuffixArchive =
        "Blank - Different - suffix" + GetArchiveFileExtension(GetParam());
    touch(dataPath / blankSuffixArchive);

    auto nonAsciiArchive = std::filesystem::u8path(
        u8"non\u00E1scii" + GetArchiveFileExtension(GetParam()));
    touch(dataPath / nonAsciiArchive);

    auto nonAsciiPrefixArchive =
        std::filesystem::u8path(u8"other non\u00E1scii2 - suffix" +
                                GetArchiveFileExtension(GetParam()));
    touch(dataPath / nonAsciiPrefixArchive);
  }

  std::shared_ptr<const PluginInterface> LoadPluginHeader(
      std::string_view pluginName) {
    handle_->LoadPlugins({std::filesystem::u8path(pluginName)}, true);

    return handle_->GetPlugin(pluginName);
  }

  std::shared_ptr<const PluginInterface> LoadPlugin(
      std::string_view pluginName) {
    handle_->LoadPlugins({std::filesystem::u8path(pluginName)}, false);

    return handle_->GetPlugin(pluginName);
  }

  std::shared_ptr<GameInterface> game_;

private:
  static std::string GetArchiveFileExtension(const GameType gameType) {
    if (gameType == GameType::fo4 || gameType == GameType::fo4vr ||
        gameType == GameType::starfield)
      return ".ba2";
    else
      return ".bsa";
  }
};

class TestPlugin : public PluginInterface {
public:
  std::string GetName() const override { return ""; }

  std::optional<float> GetHeaderVersion() const override {
    return std::optional<float>();
  }

  std::optional<std::string> GetVersion() const override {
    return std::optional<std::string>();
  }

  std::vector<std::string> GetMasters() const override { return {}; }

  std::vector<std::string> GetBashTags() const override { return {}; }

  std::optional<uint32_t> GetCRC() const override {
    return std::optional<uint32_t>();
  }

  bool IsMaster() const override { return false; }

  bool IsLightPlugin() const override { return false; }

  bool IsMediumPlugin() const override { return false; }

  bool IsUpdatePlugin() const override { return false; }

  bool IsBlueprintPlugin() const override { return false; }

  bool IsValidAsLightPlugin() const override { return false; }

  bool IsValidAsMediumPlugin() const override { return false; }

  bool IsValidAsUpdatePlugin() const override { return false; }

  bool IsEmpty() const override { return false; }

  bool LoadsArchive() const override { return false; }

  bool DoRecordsOverlap(const PluginInterface&) const override { return false; }
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         PluginInterfaceTest,
                         ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(PluginInterfaceTest,
       shouldBeAbleToGetHeaderDataFromPluginLoadedHeaderOnly) {
  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);

  const auto plugin = LoadPluginHeader(pluginName);

  EXPECT_EQ(pluginName, plugin->GetName());
  EXPECT_TRUE(plugin->GetMasters().empty());
  if (GetParam() == GameType::openmw ||
      GetParam() == GameType::oblivionRemastered) {
    EXPECT_FALSE(plugin->IsMaster());
  } else {
    EXPECT_TRUE(plugin->IsMaster());
  }
  EXPECT_FALSE(plugin->IsEmpty());
  EXPECT_EQ("5.0", plugin->GetVersion());

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
    EXPECT_FLOAT_EQ(1.2f, plugin->GetHeaderVersion().value());
  } else if (GetParam() == GameType::tes4 ||
             GetParam() == GameType::oblivionRemastered) {
    EXPECT_FLOAT_EQ(0.8f, plugin->GetHeaderVersion().value());
  } else if (GetParam() == GameType::starfield) {
    EXPECT_FLOAT_EQ(0.96f, plugin->GetHeaderVersion().value());
  } else {
    EXPECT_FLOAT_EQ(0.94f, plugin->GetHeaderVersion().value());
  }
}

TEST_P(PluginInterfaceTest, shouldBeAbleToGetAllDataFromFullyLoadedPlugin) {
  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);

  const auto plugin = LoadPlugin(pluginName);

  EXPECT_EQ(pluginName, plugin->GetName());
  EXPECT_TRUE(plugin->GetMasters().empty());
  if (GetParam() == GameType::openmw ||
      GetParam() == GameType::oblivionRemastered) {
    EXPECT_FALSE(plugin->IsMaster());
  } else {
    EXPECT_TRUE(plugin->IsMaster());
  }
  EXPECT_FALSE(plugin->IsEmpty());
  EXPECT_EQ("5.0", plugin->GetVersion());

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
    EXPECT_FLOAT_EQ(1.2f, plugin->GetHeaderVersion().value());
  } else if (GetParam() == GameType::tes4 ||
             GetParam() == GameType::oblivionRemastered) {
    EXPECT_FLOAT_EQ(0.8f, plugin->GetHeaderVersion().value());
  } else if (GetParam() == GameType::starfield) {
    EXPECT_FLOAT_EQ(0.96f, plugin->GetHeaderVersion().value());
  } else {
    EXPECT_FLOAT_EQ(0.94f, plugin->GetHeaderVersion().value());
  }

  EXPECT_EQ(getBlankEsmCrc(), plugin->GetCRC());
}

TEST_P(PluginInterfaceTest,
       loadingANonMasterPluginShouldReadTheMasterFlagAsFalse) {
  copyPlugin(BLANK_ESP);

  const auto plugin = LoadPluginHeader(BLANK_ESP);

  EXPECT_FALSE(plugin->IsMaster());
}

TEST_P(
    PluginInterfaceTest,
    isLightPluginShouldBeTrueForAPluginWithEslFileExtensionForFallout4AndSkyrimSe) {
  const auto masterName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  const auto lightPluginName =
      GetParam() == GameType::starfield ? BLANK_SMALL_ESM : BLANK_ESL;
  copyPlugin(masterName);
  copyPlugin(BLANK_ESP);

  EXPECT_FALSE(LoadPluginHeader(masterName)->IsLightPlugin());
  EXPECT_FALSE(LoadPluginHeader(BLANK_ESP)->IsLightPlugin());

  if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
      GetParam() == GameType::tes5se || GetParam() == GameType::tes5vr ||
      GetParam() == GameType::starfield) {
    copyPlugin(lightPluginName);

    EXPECT_TRUE(LoadPluginHeader(lightPluginName)->IsLightPlugin());
  }
}

TEST_P(PluginInterfaceTest,
       isMediumPluginShouldBeTrueForAMediumFlaggedPluginForStarfield) {
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_MEDIUM_ESM);
  } else {
    copyPlugin(BLANK_ESM);
    auto bytes = readFile(dataPath / BLANK_ESM);
    bytes[9] = 0x4;
    writeFile(dataPath / BLANK_ESM, bytes);
  }

  const auto& pluginName =
      GetParam() == GameType::starfield ? BLANK_MEDIUM_ESM : BLANK_ESM;
  const auto plugin = LoadPluginHeader(pluginName);

  EXPECT_EQ(GetParam() == GameType::starfield, plugin->IsMediumPlugin());
}

TEST_P(PluginInterfaceTest,
       isUpdatePluginShouldOnlyBeTrueForAStarfieldUpdatePlugin) {
  copyPlugin(BLANK_ESP);

  EXPECT_FALSE(LoadPluginHeader(BLANK_ESP)->IsUpdatePlugin());

  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_OVERRIDE_ESP, BLANK_MASTER_DEPENDENT_ESP);
  } else {
    copyPlugin(BLANK_MASTER_DEPENDENT_ESP);
  }

  auto bytes = readFile(dataPath / BLANK_MASTER_DEPENDENT_ESP);
  bytes[9] = 0x2;
  writeFile(dataPath / BLANK_MASTER_DEPENDENT_ESP, bytes);

  const auto plugin = LoadPluginHeader(BLANK_MASTER_DEPENDENT_ESP);

  EXPECT_EQ(GetParam() == GameType::starfield, plugin->IsUpdatePlugin());
}

TEST_P(PluginInterfaceTest,
       isBlueprintPluginShouldOnlyBeTrueForAStarfieldBlueprintPlugin) {
  copyPlugin(BLANK_ESP);

  EXPECT_FALSE(LoadPluginHeader(BLANK_ESP)->IsBlueprintPlugin());

  setBlueprintFlag(dataPath / BLANK_ESP);

  const auto plugin = LoadPluginHeader(BLANK_ESP);

  EXPECT_EQ(GetParam() == GameType::starfield, plugin->IsBlueprintPlugin());
}

TEST_P(PluginInterfaceTest, loadingAPluginWithMastersShouldReadThemCorrectly) {
  const auto pluginName = GetParam() == GameType::starfield
                              ? BLANK_OVERRIDE_ESP
                              : BLANK_MASTER_DEPENDENT_ESP;
  copyPlugin(pluginName);

  const auto plugin = LoadPluginHeader(pluginName);

  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(std::vector<std::string>({std::string(BLANK_FULL_ESM)}), plugin->GetMasters());
  } else {
    EXPECT_EQ(std::vector<std::string>({std::string(BLANK_ESM)}), plugin->GetMasters());
  }
}

TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveThatExactlyMatchesAnEsmFileBasenameShouldReturnTrueForAllGamesExceptMorrowindAndOblivion) {
  SetUpTestArchives();

  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);

  bool loadsArchive = LoadPluginHeader(pluginName)->LoadsArchive();

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::tes4 ||
      GetParam() == GameType::oblivionRemastered)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

#ifdef _WIN32
TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveThatExactlyMatchesANonAsciiEspFileBasenameShouldReturnTrueForAllGamesExceptMorrowindAndStarfield) {
  SetUpTestArchives();
  copyPlugin(BLANK_ESP, NON_ASCII_ESP);

  bool loadsArchive = LoadPluginHeader(NON_ASCII_ESP)->LoadsArchive();

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw ||
      GetParam() == GameType::starfield)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}
#endif

TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveThatExactlyMatchesAnEspFileBasenameShouldReturnTrueForAllGamesExceptMorrowind) {
  SetUpTestArchives();
  copyPlugin(BLANK_ESP);

  bool loadsArchive = LoadPluginHeader(BLANK_ESP)->LoadsArchive();

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheEsmFileBasenameShouldReturnTrueForOnlyTheFalloutGames) {
  SetUpTestArchives();
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_FULL_ESM, BLANK_DIFFERENT_ESM);
  } else {
    copyPlugin(BLANK_DIFFERENT_ESM);
  }

  bool loadsArchive = LoadPluginHeader(BLANK_DIFFERENT_ESM)->LoadsArchive();

  if (GetParam() == GameType::fo3 || GetParam() == GameType::fonv ||
      GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr) {
    EXPECT_TRUE(loadsArchive);
  } else {
    EXPECT_FALSE(loadsArchive);
  }
}

TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheEspFileBasenameShouldReturnTrueForOnlyOblivionAndTheFalloutGames) {
  SetUpTestArchives();
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_ESP, BLANK_DIFFERENT_ESP);
  } else {
    copyPlugin(BLANK_DIFFERENT_ESP);
  }

  bool loadsArchive = LoadPluginHeader(BLANK_DIFFERENT_ESP)->LoadsArchive();

  if (GetParam() == GameType::tes4 || GetParam() == GameType::fo3 ||
      GetParam() == GameType::fonv || GetParam() == GameType::fo4 ||
      GetParam() == GameType::fo4vr ||
      GetParam() == GameType::oblivionRemastered) {
    EXPECT_TRUE(loadsArchive);
  } else {
    EXPECT_FALSE(loadsArchive);
  }
}

#ifdef _WIN32
TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheNonAsciiEspFileBasenameShouldReturnTrueForOnlyOblivionAndTheFalloutGames) {
  SetUpTestArchives();
  copyPlugin(BLANK_ESP, OTHER_NON_ASCII_ESP);

  bool loadsArchive = LoadPluginHeader(OTHER_NON_ASCII_ESP)->LoadsArchive();

  if (GetParam() == GameType::tes4 || GetParam() == GameType::fo3 ||
      GetParam() == GameType::fonv || GetParam() == GameType::fo4 ||
      GetParam() == GameType::fo4vr ||
      GetParam() == GameType::oblivionRemastered) {
    EXPECT_TRUE(loadsArchive);
  } else {
    EXPECT_FALSE(loadsArchive);
  }
}
#endif

TEST_P(PluginInterfaceTest,
       loadsArchiveShouldReturnFalseForAPluginThatDoesNotLoadAnArchive) {
  SetUpTestArchives();
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_ESP, BLANK_DIFFERENT_MASTER_DEPENDENT_ESP);
  } else {
    copyPlugin(BLANK_DIFFERENT_MASTER_DEPENDENT_ESP);
  }

  bool loadsArchive =
      LoadPluginHeader(BLANK_DIFFERENT_MASTER_DEPENDENT_ESP)->LoadsArchive();

  EXPECT_FALSE(loadsArchive);
}

TEST_P(
    PluginInterfaceTest,
    isValidAsLightPluginShouldReturnTrueOnlyForASkyrimSEOrFallout4PluginWithNewFormIdsBetween0x800And0xFFFInclusive) {
  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);

  bool valid = LoadPlugin(pluginName)->IsValidAsLightPlugin();
  if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
      GetParam() == GameType::tes5se || GetParam() == GameType::tes5vr ||
      GetParam() == GameType::starfield) {
    EXPECT_TRUE(valid);
  } else {
    EXPECT_FALSE(valid);
  }
}

TEST_P(
    PluginInterfaceTest,
    isValidAsMediumPluginShouldReturnTrueOnlyForAStarfieldPluginWithNewFormIdsBetween0And0xFFFFInclusive) {
  if (GetParam() == GameType::starfield) {
    copyPlugin(BLANK_FULL_ESM);

    EXPECT_TRUE(LoadPlugin(BLANK_FULL_ESM)->IsValidAsMediumPlugin());
  } else {
    copyPlugin(BLANK_ESM);

    EXPECT_FALSE(LoadPlugin(BLANK_ESM)->IsValidAsMediumPlugin());
  }
}

TEST_P(
    PluginInterfaceTest,
    IsValidAsUpdatePluginShouldOnlyReturnTrueForAStarfieldPluginWithNoNewRecords) {
  const auto sourcePluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESP;
  const auto updatePluginName = GetParam() == GameType::starfield
                                    ? BLANK_OVERRIDE_ESP
                                    : BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP;
  copyPlugin(sourcePluginName);
  copyPlugin(updatePluginName);

  std::vector<std::shared_ptr<const PluginInterface>> plugins;

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
    copyPlugin(BLANK_DIFFERENT_ESP);
    plugins.push_back(LoadPlugin(BLANK_DIFFERENT_ESP));
  }

  plugins.push_back(LoadPlugin(sourcePluginName));
  plugins.push_back(LoadPlugin(updatePluginName));

  EXPECT_FALSE(plugins[0]->IsValidAsUpdatePlugin());
  EXPECT_EQ(GetParam() == GameType::starfield,
            plugins[1]->IsValidAsUpdatePlugin());
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnFalseIfTheArgumentIsNotAPluginObject) {
  const auto pluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName);

  const auto plugin1 = LoadPlugin(pluginName);
  TestPlugin plugin2;

  EXPECT_FALSE(plugin1->DoRecordsOverlap(plugin2));
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnFalseForTwoPluginsWithOnlyHeadersLoaded) {
  const auto basePluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  const auto dependentPluginName = GetParam() == GameType::starfield
                                       ? BLANK_OVERRIDE_FULL_ESM
                                       : BLANK_MASTER_DEPENDENT_ESM;
  copyPlugin(basePluginName);
  copyPlugin(dependentPluginName);

  const auto plugin1 = LoadPluginHeader(basePluginName);
  const auto plugin2 = LoadPluginHeader(dependentPluginName);

  EXPECT_FALSE(plugin1->DoRecordsOverlap(*plugin2));
  EXPECT_FALSE(plugin2->DoRecordsOverlap(*plugin1));
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnFalseIfThePluginsHaveUnrelatedRecords) {
  const auto pluginName1 =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  copyPlugin(pluginName1);
  copyPlugin(BLANK_ESP);

  const auto plugin1 = LoadPlugin(pluginName1);
  const auto plugin2 = LoadPlugin(BLANK_ESP);

  EXPECT_FALSE(plugin1->DoRecordsOverlap(*plugin2));
  EXPECT_FALSE(plugin2->DoRecordsOverlap(*plugin1));
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnTrueIfOnePluginOverridesTheOthersRecords) {
  const auto basePluginName =
      GetParam() == GameType::starfield ? BLANK_FULL_ESM : BLANK_ESM;
  const auto dependentPluginName = GetParam() == GameType::starfield
                                       ? BLANK_OVERRIDE_FULL_ESM
                                       : BLANK_MASTER_DEPENDENT_ESM;
  copyPlugin(basePluginName);
  copyPlugin(dependentPluginName);

  const auto plugin1 = LoadPlugin(basePluginName);
  const auto plugin2 = LoadPlugin(dependentPluginName);

  EXPECT_TRUE(plugin1->DoRecordsOverlap(*plugin2));
  EXPECT_TRUE(plugin2->DoRecordsOverlap(*plugin1));
}

}

#endif
