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

#ifndef LOOT_TESTS_API_INTERFACE_PLUGIN_INTERFACE_TEST
#define LOOT_TESTS_API_INTERFACE_PLUGIN_INTERFACE_TEST

#include "loot/api.h"
#include "tests/api/interface/api_game_operations_test.h"

namespace loot::test {
class PluginInterfaceTest : public ApiGameOperationsTest {
protected:
  PluginInterfaceTest() :
      ApiGameOperationsTest(),
      nonAsciiEsp(u8"non\u00C1scii.esp"),
      otherNonAsciiEsp(u8"other non\u00C1scii.esp"),
      blankArchive("Blank" + GetArchiveFileExtension(GetParam())),
      blankSuffixArchive("Blank - Different - suffix" +
                         GetArchiveFileExtension(GetParam())) {}

  void SetUp() override {
    ApiGameOperationsTest::SetUp();

    handle_->LoadPlugins(GetInstalledPlugins(), false);

    if (!supportsLightPlugins(GetParam())) {
      ASSERT_NO_THROW(
          std::filesystem::copy(dataPath / blankEsp, dataPath / blankEsl));
    }
    ASSERT_TRUE(std::filesystem::exists(dataPath / blankEsl));

    // Make sure the plugins with non-ASCII filenames exists.
    ASSERT_NO_THROW(std::filesystem::copy_file(
        dataPath / blankEsp, dataPath / std::filesystem::u8path(nonAsciiEsp)));
    ASSERT_NO_THROW(std::filesystem::copy_file(
        dataPath / blankEsp,
        dataPath / std::filesystem::u8path(otherNonAsciiEsp)));

    // Copy across archive files.
    std::filesystem::path blankMasterDependentArchive;
    if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
        GetParam() == GameType::starfield) {
      copyPlugin(getSourceArchivesPath(GetParam()), "Blank - Main.ba2");
      copyPlugin(getSourceArchivesPath(GetParam()), "Blank - Textures.ba2");

      blankMasterDependentArchive = "Blank - Master Dependent - Main.ba2";
      std::filesystem::copy_file(
          getSourceArchivesPath(GetParam()) / "Blank - Main.ba2",
          dataPath / blankMasterDependentArchive);
      ASSERT_TRUE(
          std::filesystem::exists(dataPath / blankMasterDependentArchive));
    } else if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw) {
      touch(dataPath / blankArchive);

      blankMasterDependentArchive = "Blank - Master Dependent.bsa";
      touch(dataPath / blankMasterDependentArchive);
    } else {
      copyPlugin(getSourcePluginsPath(), blankArchive);

      // Also create a copy for Blank - Master Dependent.esp to test overlap.
      blankMasterDependentArchive = "Blank - Master Dependent.bsa";
      std::filesystem::copy_file(getSourcePluginsPath() / blankArchive,
                                 dataPath / blankMasterDependentArchive);
      ASSERT_TRUE(
          std::filesystem::exists(dataPath / blankMasterDependentArchive));
    }

    // Create dummy archive files.
    touch(dataPath / blankSuffixArchive);

    auto nonAsciiArchivePath =
        dataPath / std::filesystem::u8path(u8"non\u00E1scii" +
                                           GetArchiveFileExtension(GetParam()));
    touch(dataPath / nonAsciiArchivePath);

    auto nonAsciiPrefixArchivePath =
        dataPath / std::filesystem::u8path(u8"other non\u00E1scii2 - suffix" +
                                           GetArchiveFileExtension(GetParam()));
    touch(dataPath / nonAsciiPrefixArchivePath);
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

  std::string nonAsciiEsp;
  std::string otherNonAsciiEsp;
  std::string blankArchive;
  std::string blankSuffixArchive;

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
  const auto plugin = LoadPluginHeader(blankEsm);

  EXPECT_EQ(blankEsm, plugin->GetName());
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
  const auto plugin = LoadPlugin(blankEsm);

  EXPECT_EQ(blankEsm, plugin->GetName());
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

  EXPECT_EQ(blankEsmCrc, plugin->GetCRC());
}

TEST_P(PluginInterfaceTest,
       loadingANonMasterPluginShouldReadTheMasterFlagAsFalse) {
  const auto plugin = LoadPluginHeader(blankMasterDependentEsp);

  EXPECT_FALSE(plugin->IsMaster());
}

TEST_P(
    PluginInterfaceTest,
    isLightPluginShouldBeTrueForAPluginWithEslFileExtensionForFallout4AndSkyrimSe) {
  const auto plugin1 = LoadPluginHeader(blankEsm);
  const auto plugin2 = LoadPluginHeader(blankMasterDependentEsp);

  EXPECT_FALSE(plugin1->IsLightPlugin());
  EXPECT_FALSE(plugin2->IsLightPlugin());

  if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
      GetParam() == GameType::tes5se || GetParam() == GameType::tes5vr ||
      GetParam() == GameType::starfield) {
    const auto plugin3 = LoadPluginHeader(blankEsl);
    EXPECT_TRUE(plugin3->IsLightPlugin());
  }
}

TEST_P(PluginInterfaceTest,
       isMediumPluginShouldBeTrueForAMediumFlaggedPluginForStarfield) {
  if (GetParam() != GameType::starfield) {
    auto bytes = ReadFile(dataPath / blankEsm);
    bytes[9] = 0x4;
    WriteFile(dataPath / blankEsm, bytes);
  }

  const auto& pluginName =
      GetParam() == GameType::starfield ? blankMediumEsm : blankEsm;
  const auto plugin = LoadPluginHeader(pluginName);

  EXPECT_EQ(GetParam() == GameType::starfield, plugin->IsMediumPlugin());
}

TEST_P(PluginInterfaceTest,
       isUpdatePluginShouldOnlyBeTrueForAStarfieldUpdatePlugin) {
  auto bytes = ReadFile(dataPath / blankMasterDependentEsp);
  bytes[9] = 0x2;
  WriteFile(dataPath / blankMasterDependentEsp, bytes);

  const auto plugin1 = LoadPluginHeader(blankEsp);
  const auto plugin2 = LoadPluginHeader(blankMasterDependentEsp);

  EXPECT_FALSE(plugin1->IsUpdatePlugin());
  EXPECT_EQ(GetParam() == GameType::starfield, plugin2->IsUpdatePlugin());
}

TEST_P(PluginInterfaceTest,
       isBlueprintPluginShouldOnlyBeTrueForAStarfieldBlueprintPlugin) {
  SetBlueprintFlag(dataPath / blankMasterDependentEsp);

  const auto plugin1 = LoadPluginHeader(blankEsp);
  const auto plugin2 = LoadPluginHeader(blankMasterDependentEsp);

  EXPECT_FALSE(plugin1->IsBlueprintPlugin());
  EXPECT_EQ(GetParam() == GameType::starfield, plugin2->IsBlueprintPlugin());
}

TEST_P(PluginInterfaceTest, loadingAPluginWithMastersShouldReadThemCorrectly) {
  const auto plugin = LoadPluginHeader(blankMasterDependentEsp);

  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(std::vector<std::string>({blankFullEsm}), plugin->GetMasters());
  } else {
    EXPECT_EQ(std::vector<std::string>({blankEsm}), plugin->GetMasters());
  }
}

TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveThatExactlyMatchesAnEsmFileBasenameShouldReturnTrueForAllGamesExceptMorrowindAndOblivion) {
  bool loadsArchive = LoadPluginHeader(blankEsm)->LoadsArchive();

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
  bool loadsArchive = LoadPluginHeader(nonAsciiEsp)->LoadsArchive();

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
  bool loadsArchive = LoadPluginHeader(blankEsp)->LoadsArchive();

  if (GetParam() == GameType::tes3 || GetParam() == GameType::openmw)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

TEST_P(
    PluginInterfaceTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheEsmFileBasenameShouldReturnTrueForOnlyTheFalloutGames) {
  bool loadsArchive = LoadPluginHeader(blankDifferentEsm)->LoadsArchive();

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
  bool loadsArchive = LoadPluginHeader(blankDifferentEsp)->LoadsArchive();

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
  bool loadsArchive = LoadPluginHeader(otherNonAsciiEsp)->LoadsArchive();

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
  const auto pluginName = GetParam() == GameType::starfield
                              ? blankDifferentEsp
                              : blankDifferentMasterDependentEsp;
  bool loadsArchive = LoadPluginHeader(pluginName)->LoadsArchive();

  EXPECT_FALSE(loadsArchive);
}

TEST_P(
    PluginInterfaceTest,
    isValidAsLightPluginShouldReturnTrueOnlyForASkyrimSEOrFallout4PluginWithNewFormIdsBetween0x800And0xFFFInclusive) {
  bool valid = LoadPlugin(blankEsm)->IsValidAsLightPlugin();
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
  bool valid = LoadPlugin(blankEsm)->IsValidAsMediumPlugin();
  if (GetParam() == GameType::starfield) {
    EXPECT_TRUE(valid);
  } else {
    EXPECT_FALSE(valid);
  }
}

TEST_P(
    PluginInterfaceTest,
    IsValidAsUpdatePluginShouldOnlyReturnTrueForAStarfieldPluginWithNoNewRecords) {
  const auto sourcePluginName =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsp;
  const auto updatePluginName = GetParam() == GameType::starfield
                                    ? blankMasterDependentEsp
                                    : blankDifferentPluginDependentEsp;

  std::vector<std::shared_ptr<const PluginInterface>> plugins;
  plugins.push_back(LoadPlugin(sourcePluginName));
  plugins.push_back(LoadPlugin(updatePluginName));

  EXPECT_FALSE(plugins[0]->IsValidAsUpdatePlugin());
  EXPECT_EQ(GetParam() == GameType::starfield,
            plugins[1]->IsValidAsUpdatePlugin());
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnFalseIfTheArgumentIsNotAPluginObject) {
  const auto plugin1 = LoadPlugin(blankEsm);
  TestPlugin plugin2;

  EXPECT_FALSE(plugin1->DoRecordsOverlap(plugin2));
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnFalseForTwoPluginsWithOnlyHeadersLoaded) {
  const auto plugin1 = LoadPluginHeader(blankEsm);
  const auto plugin2 = LoadPluginHeader(blankMasterDependentEsm);

  EXPECT_FALSE(plugin1->DoRecordsOverlap(*plugin2));
  EXPECT_FALSE(plugin2->DoRecordsOverlap(*plugin1));
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnFalseIfThePluginsHaveUnrelatedRecords) {
  const auto plugin1 = LoadPlugin(blankEsm);
  const auto plugin2 = LoadPlugin(blankEsp);

  EXPECT_FALSE(plugin1->DoRecordsOverlap(*plugin2));
  EXPECT_FALSE(plugin2->DoRecordsOverlap(*plugin1));
}

TEST_P(PluginInterfaceTest,
       doRecordsOverlapShouldReturnTrueIfOnePluginOverridesTheOthersRecords) {
  const auto plugin1Name =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsm;

  const auto plugin1 = LoadPlugin(plugin1Name);
  const auto plugin2 = LoadPlugin(blankMasterDependentEsm);

  EXPECT_TRUE(plugin1->DoRecordsOverlap(*plugin2));
  EXPECT_TRUE(plugin2->DoRecordsOverlap(*plugin1));
}

}

#endif
