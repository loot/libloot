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

#include "api/plugin.h"

#include "api/game/game.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginTest : public CommonGameTestFixture {
protected:
  PluginTest() :
      emptyFile("EmptyFile.esm"),
      lowercaseBlankEsp("blank.esp"),
      game_(GetParam(), dataPath.parent_path(), localPath),
      blankArchive("Blank" + GetArchiveFileExtension(game_.Type())),
      blankSuffixArchive("Blank - Different - suffix" +
                         GetArchiveFileExtension(game_.Type())) {}

  void SetUp() {
    CommonGameTestFixture::SetUp();

    game_.LoadCurrentLoadOrderState();

    // Write out an empty file.
    boost::filesystem::ofstream out(dataPath / emptyFile);
    out.close();
    ASSERT_TRUE(boost::filesystem::exists(dataPath / emptyFile));

#ifndef _WIN32
    ASSERT_NO_THROW(boost::filesystem::copy(dataPath / blankEsp,
                                            dataPath / lowercaseBlankEsp));
#endif

    if (GetParam() != GameType::fo4 && GetParam() != GameType::tes5se) {
      ASSERT_NO_THROW(
          boost::filesystem::copy(dataPath / blankEsp, dataPath / blankEsl));
    }

    // Create dummy archive files.
    out.open(dataPath / blankArchive);
    out.close();
    out.open(dataPath / blankSuffixArchive);
    out.close();

    game_.GetCache()->CacheArchivePath(dataPath / blankArchive);
    game_.GetCache()->CacheArchivePath(dataPath / blankSuffixArchive);
  }

  Game game_;

  const std::string emptyFile;
  const std::string lowercaseBlankEsp;
  const std::string blankArchive;
  const std::string blankSuffixArchive;

private:
  static std::string GetArchiveFileExtension(const GameType gameType) {
    if (gameType == GameType::fo4)
      return ".ba2";
    else
      return ".bsa";
  }
};

class OtherPluginType : public PluginInterface {
public:
  std::string GetName() const { return ""; }
  std::string GetLowercasedName() const { return ""; }
  std::optional<std::string> GetVersion() const { return std::nullopt; }
  std::vector<std::string> GetMasters() const {
    return std::vector<std::string>();
  }
  std::set<Tag> GetBashTags() const { return std::set<Tag>(); }
  std::optional<uint32_t> GetCRC() const { return std::nullopt; }

  bool IsMaster() const { return false; }
  bool IsLightMaster() const { return false; }
  bool IsEmpty() const { return false; }
  bool LoadsArchive() const { return false; }
  bool DoFormIDsOverlap(const PluginInterface& plugin) const { return true; }
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        PluginTest,
                        ::testing::Values(GameType::tes4,
                                          GameType::tes5,
                                          GameType::fo3,
                                          GameType::fonv,
                                          GameType::fo4,
                                          GameType::tes5se));

TEST_P(PluginTest, loadingHeaderOnlyShouldReadHeaderData) {
  Plugin plugin(game_.Type(),
                game_.DataPath(),
                game_.GetCache(),
                game_.GetLoadOrderHandler(),
                blankEsm,
                true);

  EXPECT_EQ(blankEsm, plugin.GetName());
  EXPECT_TRUE(plugin.GetMasters().empty());
  EXPECT_TRUE(plugin.IsMaster());
  EXPECT_FALSE(plugin.IsEmpty());
  EXPECT_EQ("5.0", plugin.GetVersion());
}

TEST_P(PluginTest, loadingHeaderOnlyShouldNotReadFieldsOrCalculateCrc) {
  Plugin plugin(game_.Type(),
                game_.DataPath(),
                game_.GetCache(),
                game_.GetLoadOrderHandler(),
                blankEsm,
                true);

  EXPECT_FALSE(plugin.GetCRC());
}

TEST_P(PluginTest, loadingWholePluginShouldReadHeaderData) {
  Plugin plugin(game_.Type(),
                game_.DataPath(),
                game_.GetCache(),
                game_.GetLoadOrderHandler(),
                blankEsm,
                true);

  EXPECT_EQ(blankEsm, plugin.GetName());
  EXPECT_TRUE(plugin.GetMasters().empty());
  EXPECT_TRUE(plugin.IsMaster());
  EXPECT_FALSE(plugin.IsEmpty());
  EXPECT_EQ("5.0", plugin.GetVersion());
}

TEST_P(PluginTest, loadingWholePluginShouldReadFields) {
  Plugin plugin(game_.Type(),
                game_.DataPath(),
                game_.GetCache(),
                game_.GetLoadOrderHandler(),
                blankMasterDependentEsm,
                false);

  EXPECT_EQ(4, plugin.NumOverrideFormIDs());
}

TEST_P(PluginTest, loadingWholePluginShouldCalculateCrc) {
  Plugin plugin(game_.Type(),
                game_.DataPath(),
                game_.GetCache(),
                game_.GetLoadOrderHandler(),
                blankEsm,
                false);

  EXPECT_EQ(blankEsmCrc, plugin.GetCRC());
}

TEST_P(PluginTest, loadingANonMasterPluginShouldReadTheMasterFlagAsFalse) {
  Plugin plugin(game_.Type(),
                game_.DataPath(),
                game_.GetCache(),
                game_.GetLoadOrderHandler(),
                blankMasterDependentEsp,
                true);

  EXPECT_FALSE(plugin.IsMaster());
}

TEST_P(
    PluginTest,
    isLightMasterShouldBeTrueForAPluginWithEslFileExtensionForFallout4AndSkyrimSeAndFalseOtherwise) {
  Plugin plugin1(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsm,
                 true);
  Plugin plugin2(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankMasterDependentEsp,
                 true);
  Plugin plugin3(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsl,
                 true);

  EXPECT_FALSE(plugin1.IsLightMaster());
  EXPECT_FALSE(plugin2.IsLightMaster());
  EXPECT_EQ(GetParam() == GameType::fo4 || GetParam() == GameType::tes5se,
            plugin3.IsLightMaster());
}

TEST_P(PluginTest, loadingAPluginWithMastersShouldReadThemCorrectly) {
  Plugin plugin(game_.Type(),
                game_.DataPath(),
                game_.GetCache(),
                game_.GetLoadOrderHandler(),
                blankMasterDependentEsp,
                true);

  EXPECT_EQ(std::vector<std::string>({blankEsm}), plugin.GetMasters());
}

TEST_P(PluginTest, loadingAPluginThatDoesNotExistShouldThrow) {
  EXPECT_THROW(Plugin(game_.Type(),
                      game_.DataPath(),
                game_.GetCache(),
                      game_.GetLoadOrderHandler(),
                      "Blank\\.esp",
                      true),
               FileAccessError);
}

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveThatExactlyMatchesAnEsmFileBasenameShouldReturnTrueForAllGamesExceptOblivion) {
  bool loadsArchive = Plugin(game_.Type(),
                             game_.DataPath(),
                game_.GetCache(),
                             game_.GetLoadOrderHandler(),
                             blankEsm,
                             true)
                          .LoadsArchive();

  if (GetParam() == GameType::tes4)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveThatExactlyMatchesAnEspFileBasenameShouldReturnTrue) {
  EXPECT_TRUE(Plugin(game_.Type(),
                     game_.DataPath(),
                game_.GetCache(),
                     game_.GetLoadOrderHandler(),
                     blankEsp,
                     true)
                  .LoadsArchive());
}

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheEsmFileBasenameShouldReturnTrueForAllGamesExceptOblivionAndSkyrim) {
  bool loadsArchive = Plugin(game_.Type(),
                             game_.DataPath(),
                game_.GetCache(),
                             game_.GetLoadOrderHandler(),
                             blankDifferentEsm,
                             true)
                          .LoadsArchive();

  if (GetParam() == GameType::tes4 || GetParam() == GameType::tes5)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

TEST_P(
    PluginTest,
    loadsArchiveForAnArchiveWithAFilenameWhichStartsWithTheEspFileBasenameShouldReturnTrueForAllGamesExceptSkyrim) {
  bool loadsArchive = Plugin(game_.Type(),
                             game_.DataPath(),
                game_.GetCache(),
                             game_.GetLoadOrderHandler(),
                             blankDifferentEsp,
                             true)
                          .LoadsArchive();

  if (GetParam() == GameType::tes5)
    EXPECT_FALSE(loadsArchive);
  else
    EXPECT_TRUE(loadsArchive);
}

TEST_P(PluginTest,
       loadsArchiveShouldReturnFalseForAPluginThatDoesNotLoadAnArchive) {
  EXPECT_FALSE(Plugin(game_.Type(),
                      game_.DataPath(),
                game_.GetCache(),
                      game_.GetLoadOrderHandler(),
                      blankMasterDependentEsp,
                      true)
                   .LoadsArchive());
}

TEST_P(PluginTest, isValidShouldReturnTrueForAValidPlugin) {
  EXPECT_TRUE(Plugin::IsValid(blankEsm, game_.Type(), game_.DataPath()));
}

TEST_P(PluginTest, isValidShouldReturnFalseForANonPluginFile) {
  EXPECT_FALSE(Plugin::IsValid(nonPluginFile, game_.Type(), game_.DataPath()));
}

TEST_P(PluginTest, isValidShouldReturnFalseForAnEmptyFile) {
  EXPECT_FALSE(Plugin::IsValid(emptyFile, game_.Type(), game_.DataPath()));
}

TEST_P(PluginTest, isActiveShouldReturnTrueForAPluginThatIsActive) {
  EXPECT_TRUE(Plugin(game_.Type(),
                     game_.DataPath(),
                game_.GetCache(),
                     game_.GetLoadOrderHandler(),
                     blankEsm,
                     true)
                  .IsActive());
}

TEST_P(PluginTest, isActiveShouldReturnFalseForAPluginThatIsNotActive) {
  EXPECT_FALSE(Plugin(game_.Type(),
                      game_.DataPath(),
                game_.GetCache(),
                      game_.GetLoadOrderHandler(),
                      blankEsp,
                      true)
                   .IsActive());
}

TEST_P(PluginTest,
       lessThanOperatorShouldUseCaseInsensitiveLexicographicalNameComparison) {
  Plugin plugin1(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsp,
                 true);
  Plugin plugin2(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 lowercaseBlankEsp,
                 true);

  EXPECT_FALSE(plugin1 < plugin2);
  EXPECT_FALSE(plugin2 < plugin1);

  Plugin plugin3 = Plugin(game_.Type(),
                          game_.DataPath(),
                game_.GetCache(),
                          game_.GetLoadOrderHandler(),
                          blankEsm,
                          true);
  Plugin plugin4 = Plugin(game_.Type(),
                          game_.DataPath(),
                game_.GetCache(),
                          game_.GetLoadOrderHandler(),
                          blankEsp,
                          true);

  EXPECT_TRUE(plugin3 < plugin4);
  EXPECT_FALSE(plugin4 < plugin3);
}

TEST_P(PluginTest,
       doFormIDsOverlapShouldReturnFalseIfTheArgumentIsNotAPluginObject) {
  Plugin plugin1(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsm,
                 false);
  OtherPluginType plugin2;

  EXPECT_FALSE(plugin1.DoFormIDsOverlap(plugin2));
  EXPECT_TRUE(plugin2.DoFormIDsOverlap(plugin1));
}

TEST_P(PluginTest,
       doFormIDsOverlapShouldReturnFalseForTwoPluginsWithOnlyHeadersLoaded) {
  Plugin plugin1(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsm,
                 true);
  Plugin plugin2(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankMasterDependentEsm,
                 true);

  EXPECT_FALSE(plugin1.DoFormIDsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoFormIDsOverlap(plugin1));
}

TEST_P(PluginTest,
       doFormIDsOverlapShouldReturnFalseIfThePluginsHaveUnrelatedRecords) {
  Plugin plugin1(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsm,
                 false);
  Plugin plugin2(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsp,
                 false);

  EXPECT_FALSE(plugin1.DoFormIDsOverlap(plugin2));
  EXPECT_FALSE(plugin2.DoFormIDsOverlap(plugin1));
}

TEST_P(PluginTest,
       doFormIDsOverlapShouldReturnTrueIfOnePluginOverridesTheOthersRecords) {
  Plugin plugin1(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankEsm,
                 false);
  Plugin plugin2(game_.Type(),
                 game_.DataPath(),
                game_.GetCache(),
                 game_.GetLoadOrderHandler(),
                 blankMasterDependentEsm,
                 false);

  EXPECT_TRUE(plugin1.DoFormIDsOverlap(plugin2));
  EXPECT_TRUE(plugin2.DoFormIDsOverlap(plugin1));
}

TEST_P(PluginTest,
       hasPluginFileExtensionShouldBeTrueIfFileEndsInDotEspOrDotEsm) {
  EXPECT_TRUE(hasPluginFileExtension("file.esp", GetParam()));
  EXPECT_TRUE(hasPluginFileExtension("file.esm", GetParam()));
  EXPECT_FALSE(hasPluginFileExtension("file.bsa", GetParam()));
}

TEST_P(
    PluginTest,
    hasPluginFileExtensionShouldBeTrueIfFileEndsInDotEslOnlyForFallout4AndSkyrimSE) {
  bool result = hasPluginFileExtension("file.esl", GetParam());

  EXPECT_EQ(GetParam() == GameType::fo4 || GetParam() == GameType::tes5se,
            result);
}
}
}

#endif
