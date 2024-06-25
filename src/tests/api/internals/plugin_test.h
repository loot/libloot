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

#ifndef LOOT_TESTS_API_INTERNALS_PLUGIN_TEST
#define LOOT_TESTS_API_INTERNALS_PLUGIN_TEST

#include "api/game/game.h"
#include "api/plugin.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginTest : public CommonGameTestFixture {
protected:
  PluginTest() :
      emptyFile("EmptyFile.esm"),
      lowercaseBlankEsp("blank.esp"),
      nonAsciiEsp(u8"non\u00C1scii.esp"),
      otherNonAsciiEsp(u8"other non\u00C1scii.esp"),
      blankArchive("Blank" + GetArchiveFileExtension(GetParam())),
      blankSuffixArchive("Blank - Different - suffix" +
                         GetArchiveFileExtension(GetParam())),
      game_(GetParam(), dataPath.parent_path(), localPath) {}

  void SetUp() override {
    CommonGameTestFixture::SetUp();

    game_.LoadCurrentLoadOrderState();

    // Write out an empty file.
    touch(dataPath / emptyFile);
    ASSERT_TRUE(std::filesystem::exists(dataPath / emptyFile));

#ifndef _WIN32
    ASSERT_NO_THROW(std::filesystem::copy(dataPath / blankEsp,
                                          dataPath / lowercaseBlankEsp));
#endif

    // Make sure the plugins with non-ASCII filenames exists.
    ASSERT_NO_THROW(std::filesystem::copy_file(
        dataPath / blankEsp, dataPath / std::filesystem::u8path(nonAsciiEsp)));
    ASSERT_NO_THROW(std::filesystem::copy_file(
        dataPath / blankEsp,
        dataPath / std::filesystem::u8path(otherNonAsciiEsp)));

    if (GetParam() != GameType::fo4 && GetParam() != GameType::fo4vr &&
        GetParam() != GameType::tes5se && GetParam() != GameType::tes5vr &&
        GetParam() != GameType::starfield) {
      ASSERT_NO_THROW(
          std::filesystem::copy(dataPath / blankEsp, dataPath / blankEsl));
    }

    // Copy across archive files.
    std::filesystem::path blankMasterDependentArchive;
    if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
        GetParam() == GameType::starfield) {
      copyPlugin("./Fallout 4/Data", "Blank - Main.ba2");
      copyPlugin("./Fallout 4/Data", "Blank - Textures.ba2");

      blankMasterDependentArchive = "Blank - Master Dependent - Main.ba2";
      std::filesystem::copy_file("./Fallout 4/Data/Blank - Main.ba2",
                                 dataPath / blankMasterDependentArchive);
      ASSERT_TRUE(
          std::filesystem::exists(dataPath / blankMasterDependentArchive));
    } else if (GetParam() == GameType::tes3) {
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
        dataPath /
        std::filesystem::u8path(u8"non\u00E1scii" +
                                GetArchiveFileExtension(game_.GetType()));
    touch(dataPath / nonAsciiArchivePath);

    auto nonAsciiPrefixArchivePath =
        dataPath /
        std::filesystem::u8path(u8"other non\u00E1scii2 - suffix" +
                                GetArchiveFileExtension(game_.GetType()));
    touch(dataPath / nonAsciiPrefixArchivePath);

    game_.GetCache().CacheArchivePaths({dataPath / "Blank - Main.ba2",
                                        dataPath / "Blank - Textures.ba2",
                                        dataPath / blankArchive,
                                        dataPath / blankMasterDependentArchive,
                                        dataPath / blankSuffixArchive,
                                        dataPath / nonAsciiArchivePath,
                                        dataPath / nonAsciiPrefixArchivePath});
  }

  uintmax_t getGhostedPluginFileSize() {
    switch (GetParam()) {
      case GameType::tes3:
        return 702;
      case GameType::tes4:
        return 390;
      default:
        return 1358;
    }
  }

  uintmax_t getNonAsciiEspFileSize() {
    switch (GetParam()) {
      case GameType::tes3:
        return 582;
      case GameType::tes4:
        return 55;
      default:
        return 1019;
    }
  }

  std::vector<char> ReadFile(const std::filesystem::path& path) {
    std::vector<char> bytes;
    std::ifstream in(path, std::ios::binary);

    std::copy(std::istreambuf_iterator<char>(in),
              std::istreambuf_iterator<char>(),
              std::back_inserter(bytes));

    return bytes;
  }

  void WriteFile(const std::filesystem::path& path,
                 const std::vector<char>& content) {
    std::ofstream out(path, std::ios::binary);

    out.write(content.data(), content.size());
  }

  const std::string emptyFile;
  const std::string lowercaseBlankEsp;
  const std::string nonAsciiEsp;
  const std::string otherNonAsciiEsp;
  const std::string blankArchive;
  const std::string blankSuffixArchive;

  Game game_;

private:
  static std::string GetArchiveFileExtension(const GameType gameType) {
    if (gameType == GameType::fo4 || gameType == GameType::fo4vr ||
        gameType == GameType::starfield)
      return ".ba2";
    else
      return ".bsa";
  }
};

class OtherPluginType final : public PluginSortingInterface {
public:
  std::string GetName() const override { return ""; }
  std::optional<float> GetHeaderVersion() const override { return 0.0f; }
  std::optional<std::string> GetVersion() const override {
    return std::nullopt;
  }
  std::vector<std::string> GetMasters() const override {
    return std::vector<std::string>();
  }
  std::vector<Tag> GetBashTags() const override { return std::vector<Tag>(); }
  std::optional<uint32_t> GetCRC() const override { return std::nullopt; }

  bool IsMaster() const override { return false; }
  bool IsLightPlugin() const override { return false; }
  bool IsMediumPlugin() const override { return false; }
  bool IsUpdatePlugin() const override { return false; }
  bool IsValidAsLightPlugin() const override { return false; }
  bool IsValidAsMediumPlugin() const override { return false; }
  bool IsValidAsUpdatePlugin() const override { return false; }
  bool IsEmpty() const override { return false; }
  bool LoadsArchive() const override { return false; }
  bool DoRecordsOverlap(const PluginInterface&) const override { return true; }

  size_t GetOverrideRecordCount() const override { return 0; };
  uint32_t GetRecordAndGroupCount() const override { return 0; };

  size_t GetOverlapSize(
      const std::vector<const PluginInterface*>&) const override {
    return 0;
  };

  size_t GetAssetCount() const override { return 0; };
  bool DoAssetsOverlap(const PluginSortingInterface&) const override {
    return true;
  }
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         PluginTest,
                         ::testing::Values(GameType::tes4,
                                           GameType::tes5,
                                           GameType::fo3,
                                           GameType::fonv,
                                           GameType::fo4,
                                           GameType::tes5se,
                                           GameType::fo4vr,
                                           GameType::tes5vr,
                                           GameType::tes3,
                                           GameType::starfield));

TEST_P(PluginTest, loadingShouldHandleNonAsciiFilenamesCorrectly) {
  Plugin plugin(game_.GetType(),
                game_.GetCache(),
                game_.DataPath() / std::filesystem::u8path(nonAsciiEsp),
                true);

  EXPECT_EQ(nonAsciiEsp, plugin.GetName());
  EXPECT_EQ(nonAsciiEsp, plugin.GetName());
}

TEST_P(PluginTest, loadingHeaderOnlyShouldReadHeaderData) {
  Plugin plugin(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);

  EXPECT_EQ(blankEsm, plugin.GetName());
  EXPECT_TRUE(plugin.GetMasters().empty());
  EXPECT_TRUE(plugin.IsMaster());
  EXPECT_FALSE(plugin.IsEmpty());
  EXPECT_EQ("5.0", plugin.GetVersion());

  if (GetParam() == GameType::tes3) {
    EXPECT_FLOAT_EQ(1.2f, plugin.GetHeaderVersion().value());
  } else if (GetParam() == GameType::tes4) {
    EXPECT_FLOAT_EQ(0.8f, plugin.GetHeaderVersion().value());
  } else if (GetParam() == GameType::starfield) {
    EXPECT_FLOAT_EQ(0.96f, plugin.GetHeaderVersion().value());
  } else {
    EXPECT_FLOAT_EQ(0.94f, plugin.GetHeaderVersion().value());
  }
}

TEST_P(PluginTest, loadingHeaderOnlyShouldNotReadFieldsOrCalculateCrc) {
  Plugin plugin(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);

  EXPECT_FALSE(plugin.GetCRC());
}

TEST_P(PluginTest, loadingWholePluginShouldReadHeaderData) {
  Plugin plugin(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);

  EXPECT_EQ(blankEsm, plugin.GetName());
  EXPECT_TRUE(plugin.GetMasters().empty());
  EXPECT_TRUE(plugin.IsMaster());
  EXPECT_FALSE(plugin.IsEmpty());
  EXPECT_EQ("5.0", plugin.GetVersion());

  if (GetParam() == GameType::tes3) {
    EXPECT_FLOAT_EQ(1.2f, plugin.GetHeaderVersion().value());
  } else if (GetParam() == GameType::tes4) {
    EXPECT_FLOAT_EQ(0.8f, plugin.GetHeaderVersion().value());
  } else if (GetParam() == GameType::starfield) {
    EXPECT_FLOAT_EQ(0.96f, plugin.GetHeaderVersion().value());
  } else {
    EXPECT_FLOAT_EQ(0.94f, plugin.GetHeaderVersion().value());
  }
}

TEST_P(PluginTest, loadingWholePluginShouldReadFields) {
  Plugin plugin(game_.GetType(),
                game_.GetCache(),
                game_.DataPath() / (blankMasterDependentEsm + ".ghost"),
                false);

  if (GetParam() == GameType::tes3) {
    Plugin master(
        game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, false);
    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&master});

    EXPECT_NO_THROW(plugin.ResolveRecordIds(pluginsMetadata.get()));

    EXPECT_EQ(4, plugin.GetOverrideRecordCount());
  } else if (GetParam() == GameType::starfield) {
    Plugin master(game_.GetType(),
                  game_.GetCache(),
                  game_.DataPath() / blankFullEsm,
                  true);
    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&master});

    EXPECT_NO_THROW(plugin.ResolveRecordIds(pluginsMetadata.get()));

    EXPECT_EQ(1, plugin.GetOverrideRecordCount());
  } else {
    EXPECT_EQ(4, plugin.GetOverrideRecordCount());
  }
}

TEST_P(PluginTest, loadingWholePluginShouldCalculateCrc) {
  Plugin plugin(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, false);

  EXPECT_EQ(blankEsmCrc, plugin.GetCRC());
}

TEST_P(PluginTest, loadingANonMasterPluginShouldReadTheMasterFlagAsFalse) {
  Plugin plugin(game_.GetType(),
                game_.GetCache(),
                game_.DataPath() / blankMasterDependentEsp,
                true);

  EXPECT_FALSE(plugin.IsMaster());
}

TEST_P(
    PluginTest,
    isLightPluginShouldBeTrueForAPluginWithEslFileExtensionForFallout4AndSkyrimSeAndFalseOtherwise) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankMasterDependentEsp,
                 true);
  Plugin plugin3(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsl, true);

  EXPECT_FALSE(plugin1.IsLightPlugin());
  EXPECT_FALSE(plugin2.IsLightPlugin());
  EXPECT_EQ(GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
                GetParam() == GameType::tes5se ||
                GetParam() == GameType::tes5vr ||
                GetParam() == GameType::starfield,
            plugin3.IsLightPlugin());
}

TEST_P(
  PluginTest,
       isMediumPluginShouldBeTrueForAMediumFlaggedPluginForStarfield) {
  if (GetParam() != GameType::starfield) {
    auto bytes = ReadFile(dataPath / blankEsm);
    bytes[9] = 0x4;
    WriteFile(dataPath / blankEsm, bytes);
  }

  const auto pluginName =
      GetParam() == GameType::starfield ? blankMediumEsm : blankEsm;
  Plugin plugin(game_.GetType(), game_.GetCache(), game_.DataPath() / pluginName,
                true);

  EXPECT_EQ(GetParam() == GameType::starfield, plugin.IsMediumPlugin());
}

TEST_P(PluginTest,
       isUpdatePluginShouldOnlyBeTrueForAStarfieldUpdatePlugin) {
  auto bytes = ReadFile(dataPath / blankMasterDependentEsp);
  bytes[9] = 0x2;
  WriteFile(dataPath / blankMasterDependentEsp, bytes);

  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, true);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankMasterDependentEsp,
                 true);

  EXPECT_FALSE(plugin1.IsUpdatePlugin());
  EXPECT_EQ(GetParam() == GameType::starfield, plugin2.IsUpdatePlugin());
}

TEST_P(PluginTest, loadingAPluginWithMastersShouldReadThemCorrectly) {
  Plugin plugin(game_.GetType(),
                game_.GetCache(),
                game_.DataPath() / blankMasterDependentEsp,
                true);

  if (GetParam() == GameType::starfield) {
    EXPECT_EQ(std::vector<std::string>({blankFullEsm}), plugin.GetMasters());
  } else {
    EXPECT_EQ(std::vector<std::string>({blankEsm}), plugin.GetMasters());
  }
}

TEST_P(PluginTest, loadingAPluginThatDoesNotExistShouldThrow) {
  EXPECT_THROW(Plugin(game_.GetType(),
                      game_.GetCache(),
                      game_.DataPath() / "Blank\\.esp",
                      true),
               FileAccessError);
}

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveThatExactlyMatchesAnEsmFileBasenameShouldReturnTrueForAllGamesExceptMorrowindAndOblivion) {
  bool loadsArchive =
      Plugin(
          game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true)
          .LoadsArchive();

  if (GetParam() == GameType::tes3 || GetParam() == GameType::tes4)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

#ifdef _WIN32
TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveThatExactlyMatchesANonAsciiEspFileBasenameShouldReturnTrueForAllGamesExceptMorrowindAndStarfield) {
  bool loadsArchive =
      Plugin(game_.GetType(),
             game_.GetCache(),
             game_.DataPath() / std::filesystem::u8path(nonAsciiEsp),
             true)
          .LoadsArchive();

  if (GetParam() == GameType::tes3 || GetParam() == GameType::starfield)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}
#endif

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveThatExactlyMatchesAnEspFileBasenameShouldReturnTrueForAllGamesExceptMorrowind) {
  bool loadsArchive =
      Plugin(
          game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, true)
          .LoadsArchive();

  if (GetParam() == GameType::tes3)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheEsmFileBasenameShouldReturnTrueForOnlyTheFalloutGames) {
  bool loadsArchive = Plugin(game_.GetType(),
                             game_.GetCache(),
                             game_.DataPath() / blankDifferentEsm,
                             true)
                          .LoadsArchive();

  if (GetParam() == GameType::fo3 || GetParam() == GameType::fonv ||
      GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr) {
    EXPECT_TRUE(loadsArchive);
  } else {
    EXPECT_FALSE(loadsArchive);
  }
}

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheEspFileBasenameShouldReturnTrueForOnlyOblivionAndTheFalloutGames) {
  bool loadsArchive = Plugin(game_.GetType(),
                             game_.GetCache(),
                             game_.DataPath() / blankDifferentEsp,
                             true)
                          .LoadsArchive();

  if (GetParam() == GameType::tes4 || GetParam() == GameType::fo3 ||
      GetParam() == GameType::fonv || GetParam() == GameType::fo4 ||
      GetParam() == GameType::fo4vr) {
    EXPECT_TRUE(loadsArchive);
  } else {
    EXPECT_FALSE(loadsArchive);
  }
}

#ifdef _WIN32
TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheNonAsciiEspFileBasenameShouldReturnTrueForOnlyOblivionAndTheFalloutGames) {
  bool loadsArchive =
      Plugin(game_.GetType(),
             game_.GetCache(),
             game_.DataPath() / std::filesystem::u8path(otherNonAsciiEsp),
             true)
          .LoadsArchive();

  if (GetParam() == GameType::tes4 || GetParam() == GameType::fo3 ||
      GetParam() == GameType::fonv || GetParam() == GameType::fo4 ||
      GetParam() == GameType::fo4vr) {
    EXPECT_TRUE(loadsArchive);
  } else {
    EXPECT_FALSE(loadsArchive);
  }
}
#endif

TEST_P(PluginTest,
       loadsArchiveShouldReturnFalseForAPluginThatDoesNotLoadAnArchive) {
  const auto pluginName = GetParam() == GameType::starfield
                              ? blankDifferentEsp
                              : blankDifferentMasterDependentEsp;
  EXPECT_FALSE(Plugin(game_.GetType(),
                      game_.GetCache(),
                      game_.DataPath() / pluginName,
                      true)
                   .LoadsArchive());
}

TEST_P(PluginTest, isValidShouldReturnTrueForAValidPlugin) {
  EXPECT_TRUE(Plugin::IsValid(game_.GetType(), game_.DataPath() / blankEsm));
}

TEST_P(PluginTest, isValidShouldReturnTrueForAValidNonAsciiPlugin) {
  EXPECT_TRUE(
      Plugin::IsValid(game_.GetType(),
                      game_.DataPath() / std::filesystem::u8path(nonAsciiEsp)));
}

TEST_P(PluginTest, isValidShouldReturnFalseForANonPluginFile) {
  EXPECT_FALSE(
      Plugin::IsValid(game_.GetType(), game_.DataPath() / nonPluginFile));
}

TEST_P(PluginTest, isValidShouldReturnFalseForAnEmptyFile) {
  EXPECT_FALSE(Plugin::IsValid(game_.GetType(), game_.DataPath() / emptyFile));
}

TEST_P(
    PluginTest,
    isValidAsLightPluginShouldReturnTrueOnlyForASkyrimSEOrFallout4PluginWithNewFormIdsBetween0x800And0xFFFInclusive) {
  bool valid =
      Plugin(
          game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true)
          .IsValidAsLightPlugin();
  if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
      GetParam() == GameType::tes5se || GetParam() == GameType::tes5vr ||
      GetParam() == GameType::starfield) {
    EXPECT_TRUE(valid);
  } else {
    EXPECT_FALSE(valid);
  }
}

TEST_P(
    PluginTest,
    isValidAsMediumPluginShouldReturnTrueOnlyForAStarfieldPluginWithNewFormIdsBetween0And0xFFFFInclusive) {
  bool valid =
      Plugin(
          game_.GetType(), game_.GetCache(), dataPath / blankEsm, true)
          .IsValidAsMediumPlugin();
  if (GetParam() == GameType::starfield) {
    EXPECT_TRUE(valid);
  } else {
    EXPECT_FALSE(valid);
  }
}

TEST_P(
    PluginTest,
    IsValidAsUpdatePluginShouldOnlyReturnTrueForAStarfieldPluginWithNoNewRecords) {
  const auto sourcePluginName = GetParam() == GameType::starfield
                                  ? blankFullEsm
                                  : blankEsp;
  const auto updatePluginName = GetParam() == GameType::starfield
                                  ? blankMasterDependentEsp
                                  : blankDifferentPluginDependentEsp;

  Plugin plugin1(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / sourcePluginName,
                 false);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / updatePluginName,
                 false);

  if (GetParam() == GameType::starfield) {
    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&plugin1});
    plugin2.ResolveRecordIds(pluginsMetadata.get());

    plugin1.ResolveRecordIds(nullptr);
    plugin2.ResolveRecordIds(nullptr);
  }

  EXPECT_FALSE(plugin1.IsValidAsUpdatePlugin());
  EXPECT_EQ(GetParam() == GameType::starfield,
            plugin2.IsValidAsUpdatePlugin());
}

TEST_P(PluginTest,
       doRecordsOverlapShouldReturnFalseIfTheArgumentIsNotAPluginObject) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, false);
  OtherPluginType plugin2;

  EXPECT_FALSE(plugin1.DoRecordsOverlap(plugin2));
  EXPECT_TRUE(plugin2.DoRecordsOverlap(plugin1));
}

TEST_P(PluginTest,
       doRecordsOverlapShouldReturnFalseForTwoPluginsWithOnlyHeadersLoaded) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / (blankMasterDependentEsm + ".ghost"),
                 true);

  EXPECT_FALSE(plugin1.DoRecordsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoRecordsOverlap(plugin1));
}

TEST_P(PluginTest,
       doRecordsOverlapShouldReturnFalseIfThePluginsHaveUnrelatedRecords) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, false);
  Plugin plugin2(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);

  if (GetParam() == GameType::starfield) {
    plugin1.ResolveRecordIds(nullptr);
    plugin2.ResolveRecordIds(nullptr);
  }

  EXPECT_FALSE(plugin1.DoRecordsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoRecordsOverlap(plugin1));
}

TEST_P(PluginTest,
       doRecordsOverlapShouldReturnTrueIfOnePluginOverridesTheOthersRecords) {
  const auto plugin1Name =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsm;

  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / plugin1Name, false);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / (blankMasterDependentEsm + ".ghost"),
                 false);

  if (GetParam() == GameType::starfield) {
    plugin1.ResolveRecordIds(nullptr);

    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&plugin1});
    plugin2.ResolveRecordIds(pluginsMetadata.get());
  }

  EXPECT_TRUE(plugin1.DoRecordsOverlap(plugin2));
  EXPECT_TRUE(plugin2.DoRecordsOverlap(plugin1));
}

TEST_P(PluginTest,
       getOverlapSizeShouldThrowIfGivenAVectorContainingANonPluginObject) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, false);
  OtherPluginType plugin2;

  EXPECT_THROW(plugin1.GetOverlapSize({&plugin2, &plugin2}),
               std::invalid_argument);
  EXPECT_EQ(0, plugin2.GetOverlapSize({&plugin1}));
}

TEST_P(PluginTest, getOverlapSizeShouldCountEachRecordOnce) {
  const auto plugin1Name =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsm;

  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / plugin1Name, false);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / (blankMasterDependentEsm + ".ghost"),
                 false);

  if (GetParam() == GameType::starfield) {
    plugin1.ResolveRecordIds(nullptr);

    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&plugin1});
    plugin2.ResolveRecordIds(pluginsMetadata.get());

    EXPECT_EQ(1, plugin1.GetOverlapSize({&plugin2, &plugin2}));
  } else {
    EXPECT_EQ(4, plugin1.GetOverlapSize({&plugin2, &plugin2}));
  }
}

TEST_P(PluginTest, getOverlapSizeShouldCheckAgainstAllGivenPlugins) {
  const auto plugin1Name =
      GetParam() == GameType::starfield ? blankFullEsm : blankEsm;

  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / plugin1Name, false);
  Plugin plugin2(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);
  Plugin plugin3(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / (blankMasterDependentEsm + ".ghost"),
                 false);

  if (GetParam() == GameType::starfield) {
    plugin1.ResolveRecordIds(nullptr);
    plugin2.ResolveRecordIds(nullptr);

    const auto pluginsMetadata = Plugin::GetPluginsMetadata({&plugin1});
    plugin3.ResolveRecordIds(pluginsMetadata.get());

    EXPECT_EQ(1, plugin1.GetOverlapSize({&plugin2, &plugin3}));
  } else {
    EXPECT_EQ(4, plugin1.GetOverlapSize({&plugin2, &plugin3}));
  }
}

TEST_P(PluginTest,
       getOverlapSizeShouldReturnZeroForPluginsWithOnlyHeadersLoaded) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / (blankMasterDependentEsm + ".ghost"),
                 true);

  EXPECT_EQ(0, plugin1.GetOverlapSize({&plugin2}));
}

TEST_P(PluginTest, getOverlapSizeShouldReturnZeroForPluginsThatDoNotOverlap) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, false);
  Plugin plugin2(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);

  if (GetParam() == GameType::starfield) {
    plugin1.ResolveRecordIds(nullptr);
    plugin2.ResolveRecordIds(nullptr);
  }

  EXPECT_EQ(0, plugin1.GetOverlapSize({&plugin2}));
}

TEST_P(PluginTest, getRecordAndGroupCountShouldReturnTheHeaderFieldValue) {
  Plugin plugin(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsm, true);

  if (GetParam() == GameType::tes3) {
    EXPECT_EQ(10u, plugin.GetRecordAndGroupCount());
  } else if (GetParam() == GameType::tes4) {
    EXPECT_EQ(14u, plugin.GetRecordAndGroupCount());
  } else {
    EXPECT_EQ(15u, plugin.GetRecordAndGroupCount());
  }
}

TEST_P(PluginTest,
       getAssetCountShouldReturnNumberOfFilesInArchivesLoadedByPlugin) {
  const auto assetCount =
      Plugin(
          game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false)
          .GetAssetCount();

  if (GetParam() == GameType::tes3) {
    EXPECT_EQ(0, assetCount);
  } else if (GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
             GetParam() == GameType::starfield) {
    EXPECT_EQ(2, assetCount);
  } else {
    EXPECT_EQ(1, assetCount);
  }
}

TEST_P(PluginTest, getAssetCountShouldReturnZeroIfOnlyPluginHeaderWasLoaded) {
  const auto assetCount =
      Plugin(
          game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, true)
          .GetAssetCount();

  EXPECT_EQ(0, assetCount);
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnFalseOrThrowIfTheArgumentIsNotAPluginObject) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);
  OtherPluginType plugin2;

  if (GetParam() == GameType::tes3) {
    EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
  } else {
    EXPECT_THROW(plugin1.DoAssetsOverlap(plugin2), std::invalid_argument);
  }
  EXPECT_TRUE(plugin2.DoAssetsOverlap(plugin1));
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnFalseForTwoPluginsWithOnlyHeadersLoaded) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, true);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankMasterDependentEsp,
                 true);

  EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoAssetsOverlap(plugin1));
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnFalseIfThePluginsDoNotLoadTheSameAssetPath) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);
  // Blank - Different.esp does not load any assets.
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankDifferentEsp,
                 false);

  EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoAssetsOverlap(plugin1));
}

TEST_P(PluginTest,
       doAssetsOverlapShouldReturnTrueIfThePluginsLoadTheSameAssetPath) {
  Plugin plugin1(
      game_.GetType(), game_.GetCache(), game_.DataPath() / blankEsp, false);
  Plugin plugin2(game_.GetType(),
                 game_.GetCache(),
                 game_.DataPath() / blankMasterDependentEsp,
                 false);

  if (GetParam() == GameType::tes3) {
    // Morrowind plugins can't load assets.
    EXPECT_FALSE(plugin1.DoAssetsOverlap(plugin2));
    EXPECT_FALSE(plugin2.DoAssetsOverlap(plugin1));
  } else {
    EXPECT_TRUE(plugin1.DoAssetsOverlap(plugin2));
    EXPECT_TRUE(plugin2.DoAssetsOverlap(plugin1));
  }
}

TEST_P(PluginTest,
       hasPluginFileExtensionShouldBeTrueIfFileEndsInDotEspOrDotEsm) {
  EXPECT_TRUE(hasPluginFileExtension("file.esp", GetParam()));
  EXPECT_TRUE(hasPluginFileExtension("file.esm", GetParam()));
  EXPECT_FALSE(hasPluginFileExtension("file.bsa", GetParam()));
}

TEST_P(
    PluginTest,
    hasPluginFileExtensionShouldBeTrueIfFileEndsInDotEslOnlyForFallout4AndLater) {
  bool result = hasPluginFileExtension("file.esl", GetParam());

  EXPECT_EQ(GetParam() == GameType::fo4 || GetParam() == GameType::fo4vr ||
                GetParam() == GameType::tes5se ||
                GetParam() == GameType::tes5vr ||
                GetParam() == GameType::starfield,
            result);
}

TEST(equivalent, shouldReturnTrueIfGivenEqualPathsThatExist) {
  auto path1 = std::filesystem::path("LICENSE");
  auto path2 = std::filesystem::path("LICENSE");

  EXPECT_TRUE(loot::equivalent(path1, path2));
}

TEST(equivalent, shouldReturnTrueIfGivenEqualPathsThatDoNotExist) {
  auto path1 = std::filesystem::path("LICENSE2");
  auto path2 = std::filesystem::path("LICENSE2");

  EXPECT_TRUE(loot::equivalent(path1, path2));
}

TEST(equivalent, shouldReturnFalseIfPathsAreNotCaseInsensitivelyEqual) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license2");

  EXPECT_FALSE(loot::equivalent(lower, upper));
}

#ifdef _WIN32
TEST(equivalent, shouldReturnTrueIfGivenCaseInsensitivelyEqualPathsThatExist) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license");

  EXPECT_TRUE(loot::equivalent(lower, upper));
}

TEST(equivalent,
     shouldReturnFalseIfGivenCaseInsensitivelyEqualPathsThatDoNotExist) {
  auto upper = std::filesystem::path("LICENSE2");
  auto lower = std::filesystem::path("license2");

  EXPECT_FALSE(loot::equivalent(lower, upper));
}

TEST(
    equivalent,
    shouldReturnTrueIfEqualPathsHaveCharactersThatAreUnrepresentableInTheSystemMultiByteCodePage) {
  auto path1 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.txt");
  auto path2 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.txt");

  EXPECT_TRUE(loot::equivalent(path1, path2));
}

TEST(
    equivalent,
    shouldReturnFalseIfCaseInsensitivelyEqualPathsHaveCharactersThatAreUnrepresentableInTheSystemMultiByteCodePage) {
  auto path1 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00E3\u00CE.txt");
  auto path2 = std::filesystem::u8path(
      u8"\u2551\u00BB\u00C1\u2510\u2557\u00FE\u00C3\u00CE.txt");

  EXPECT_FALSE(loot::equivalent(path1, path2));
}
#else
TEST(equivalent, shouldReturnFalseIfGivenCaseInsensitivelyEqualPathsThatExist) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license");

  EXPECT_FALSE(loot::equivalent(lower, upper));
}

TEST(equivalent,
     shouldReturnFalseIfGivenCaseInsensitivelyEqualPathsThatDoNotExist) {
  auto upper = std::filesystem::path("LICENSE2");
  auto lower = std::filesystem::path("license2");

  EXPECT_FALSE(loot::equivalent(lower, upper));
}
#endif
}
}

#endif
