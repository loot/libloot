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

#ifndef LOOT_TESTS_COMMON_GAME_TEST_FIXTURE
#define LOOT_TESTS_COMMON_GAME_TEST_FIXTURE

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_set>

#include "loot/enum/game_type.h"
#include "tests/test_helpers.h"

namespace loot {
namespace test {
static const std::array<GameType, 12> ALL_GAME_TYPES = {
    GameType::tes3,
    GameType::tes4,
    GameType::tes5,
    GameType::tes5se,
    GameType::tes5vr,
    GameType::fo3,
    GameType::fonv,
    GameType::fo4,
    GameType::fo4vr,
    GameType::starfield,
    GameType::openmw,
    GameType::oblivionRemastered,
};

class CommonGameTestFixture : public ::testing::Test {
protected:
  CommonGameTestFixture(GameType gameType) :
      gameType_(gameType),
      rootTestPath(getRootTestPath()),
      french("fr"),
      german("de"),
      missingPath(rootTestPath / "missing"),
      gamePath(rootTestPath / "games" / "game"),
      dataPath(gamePath / getPluginsFolder()),
      localPath(rootTestPath / "local" / "game"),
      metadataFilesPath(rootTestPath / "metadata"),
      masterFile(getMasterFile()),
      missingEsp("Blank.missing.esp"),
      nonPluginFile("NotAPlugin.esm"),
      invalidPlugin("Invalid.esm"),
      blankEsm("Blank.esm"),
      blankFullEsm("Blank.full.esm"),
      blankMediumEsm("Blank.medium.esm"),
      blankDifferentEsm("Blank - Different.esm"),
      blankMasterDependentEsm("Blank - Master Dependent.esm"),
      blankDifferentMasterDependentEsm(
          "Blank - Different Master Dependent.esm"),
      blankEsl("Blank.esl"),
      blankEsp("Blank.esp"),
      blankDifferentEsp("Blank - Different.esp"),
      blankMasterDependentEsp("Blank - Master Dependent.esp"),
      blankDifferentMasterDependentEsp(
          "Blank - Different Master Dependent.esp"),
      blankPluginDependentEsp("Blank - Plugin Dependent.esp"),
      blankDifferentPluginDependentEsp(
          "Blank - Different Plugin Dependent.esp"),
      blankEsmCrc(getBlankEsmCrc()) {
    assertInitialState();
  }

  void assertInitialState() {
    using std::filesystem::create_directories;
    using std::filesystem::exists;

    create_directories(dataPath);
    ASSERT_TRUE(exists(dataPath));

    create_directories(localPath);
    ASSERT_TRUE(exists(localPath));

    create_directories(metadataFilesPath);
    ASSERT_TRUE(exists(metadataFilesPath));

    auto sourcePluginsPath = getSourcePluginsPath();

    if (gameType_ == GameType::starfield) {
      copyPlugin(sourcePluginsPath, blankFullEsm);
      copyPlugin(sourcePluginsPath, blankMediumEsm);

      std::filesystem::copy_file(sourcePluginsPath / blankFullEsm,
                                 dataPath / blankEsm);
      ASSERT_TRUE(exists(dataPath / blankEsm));

      std::filesystem::copy_file(sourcePluginsPath / blankFullEsm,
                                 dataPath / blankDifferentEsm);
      ASSERT_TRUE(exists(dataPath / blankDifferentEsm));

      std::filesystem::copy_file(
          sourcePluginsPath / "Blank - Override.full.esm",
          dataPath / blankMasterDependentEsm);
      ASSERT_TRUE(exists(dataPath / blankMasterDependentEsm));

      std::filesystem::copy_file(sourcePluginsPath / "Blank.esp",
                                 dataPath / blankEsp);
      ASSERT_TRUE(exists(dataPath / blankEsp));

      std::filesystem::copy_file(sourcePluginsPath / blankEsp,
                                 dataPath / blankDifferentEsp);
      ASSERT_TRUE(exists(dataPath / blankDifferentEsp));

      std::filesystem::copy_file(sourcePluginsPath / "Blank - Override.esp",
                                 dataPath / blankMasterDependentEsp);
      ASSERT_TRUE(exists(dataPath / blankMasterDependentEsp));
    } else {
      copyPlugin(sourcePluginsPath, blankEsm);
      copyPlugin(sourcePluginsPath, blankDifferentEsm);
      copyPlugin(sourcePluginsPath, blankMasterDependentEsm);
      copyPlugin(sourcePluginsPath, blankDifferentMasterDependentEsm);
      copyPlugin(sourcePluginsPath, blankEsp);
      copyPlugin(sourcePluginsPath, blankDifferentEsp);
      copyPlugin(sourcePluginsPath, blankMasterDependentEsp);
      copyPlugin(sourcePluginsPath, blankDifferentMasterDependentEsp);
      copyPlugin(sourcePluginsPath, blankPluginDependentEsp);
      copyPlugin(sourcePluginsPath, blankDifferentPluginDependentEsp);
    }

    if (supportsLightPlugins(gameType_)) {
      if (gameType_ == GameType::starfield) {
        std::filesystem::copy_file(sourcePluginsPath / "Blank.small.esm",
                                   dataPath / blankEsl);
        ASSERT_TRUE(exists(dataPath / blankEsl));
      } else {
        copyPlugin(sourcePluginsPath, blankEsl);
      }
    }

    // Make sure the game master file exists.
    ASSERT_NO_THROW(
        std::filesystem::copy_file(dataPath / blankEsm, dataPath / masterFile));
    ASSERT_TRUE(exists(dataPath / masterFile));

    // Set initial load order and active plugins.
    setLoadOrder(getInitialLoadOrder());

    // Ghost a plugin, except for OpenMW.
    if (gameType_ != GameType::openmw) {
      ASSERT_NO_THROW(std::filesystem::rename(
          dataPath / blankMasterDependentEsm,
          dataPath / (blankMasterDependentEsm + ".ghost")));
      ASSERT_FALSE(exists(dataPath / blankMasterDependentEsm));
      ASSERT_TRUE(exists(dataPath / (blankMasterDependentEsm + ".ghost")));
    } else {
      touch(gamePath / "openmw.cfg");
    }

    // Write out an non-empty, non-plugin file.
    std::ofstream out(dataPath / nonPluginFile);
    out << "This isn't a valid plugin file.";
    out.close();
    ASSERT_TRUE(exists(dataPath / nonPluginFile));

    ASSERT_FALSE(exists(missingPath));
    ASSERT_FALSE(exists(dataPath / missingEsp));
  }

  void copyPlugin(const std::filesystem::path& sourceParentPath,
                  std::string_view filename) {
    std::filesystem::copy_file(sourceParentPath / filename,
                               dataPath / filename);
    ASSERT_TRUE(std::filesystem::exists(dataPath / filename));
  }

  void TearDown() override {
    // Grant write permissions to everything in rootTestPath
    // in case the test made anything read only.
    for (const auto& path :
         std::filesystem::recursive_directory_iterator(rootTestPath)) {
      std::filesystem::permissions(path, std::filesystem::perms::all);
    }
    std::filesystem::remove_all(rootTestPath);
  }

  std::vector<std::string> readFileLines(const std::filesystem::path& file) {
    std::ifstream in(file);

    std::vector<std::string> lines;
    while (in) {
      std::string line;
      std::getline(in, line);

      if (!line.empty()) {
        lines.push_back(line);
      }
    }

    return lines;
  }

  std::vector<std::string> getLoadOrder() {
    std::vector<std::string> actual;
    if (isLoadOrderTimestampBased(gameType_)) {
      std::map<std::filesystem::file_time_type, std::string> loadOrder;
      for (std::filesystem::directory_iterator it(dataPath);
           it != std::filesystem::directory_iterator();
           ++it) {
        if (std::filesystem::is_regular_file(it->status())) {
          std::string filename = it->path().filename().u8string();
          if (filename == nonPluginFile)
            continue;
          if (endsWith(filename, ".ghost"))
            filename = it->path().stem().u8string();
          if (endsWith(filename, ".esp") || endsWith(filename, ".esm"))
            loadOrder.emplace(std::filesystem::last_write_time(it->path()),
                              filename);
        }
      }
      for (const auto& plugin : loadOrder) actual.push_back(plugin.second);
    } else if (gameType_ == GameType::tes5 ||
               gameType_ == GameType::oblivionRemastered) {
      const auto& parentPath =
          gameType_ == GameType::oblivionRemastered ? dataPath : localPath;
      std::ifstream in(parentPath / "loadorder.txt");
      while (in) {
        std::string line;
        std::getline(in, line);

        if (!line.empty())
          actual.push_back(line);
      }
    } else if (gameType_ == GameType::openmw) {
      throw std::runtime_error(
          "OpenMW's load order derivation is too complicated to replicate "
          "accurately just for a test.");
    } else {
      actual = readFileLines(localPath / "Plugins.txt");
      for (auto& line : actual) {
        if (line[0] == '*')
          line = line.substr(1);
      }
    }

    return actual;
  }

  std::vector<std::pair<std::string, bool>> getInitialLoadOrder() const {
    std::vector<std::pair<std::string, bool>> loadOrder;

    if (gameType_ == GameType::starfield) {
      loadOrder = {
          {masterFile, true},
          {blankEsm, true},
          {blankDifferentEsm, false},
          {blankFullEsm, false},
          {blankMasterDependentEsm, false},
          {blankMediumEsm, false},
          {blankEsl, false},
          {blankEsp, false},
          {blankDifferentEsp, false},
          {blankMasterDependentEsp, false},
      };
    } else {
      loadOrder = {
          {masterFile, true},
          {blankEsm, true},
          {blankDifferentEsm, false},
          {blankMasterDependentEsm, false},
          {blankDifferentMasterDependentEsm, false},
          {blankEsp, false},
          {blankDifferentEsp, false},
          {blankMasterDependentEsp, false},
          {blankDifferentMasterDependentEsp, true},
          {blankPluginDependentEsp, false},
          {blankDifferentPluginDependentEsp, false},
      };

      if (supportsLightPlugins(gameType_)) {
        loadOrder.insert(loadOrder.begin() + 5,
                         std::make_pair(blankEsl, false));
      }
    }

    return loadOrder;
  }

  std::filesystem::path getSourcePluginsPath() const {
    return loot::test::getSourcePluginsPath(gameType_);
  }

  void touch(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out.close();
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

  std::vector<std::filesystem::path> GetInstalledPlugins() {
    if (gameType_ == GameType::starfield) {
      return {
          masterFile,
          blankEsm,
          blankDifferentEsm,
          blankFullEsm,
          blankMasterDependentEsm,
          blankMediumEsm,
          blankEsl,
          blankEsp,
          blankDifferentEsp,
          blankMasterDependentEsp,
      };
    } else if (supportsLightPlugins(gameType_)) {
      return {
          masterFile,
          blankEsm,
          blankDifferentEsm,
          blankMasterDependentEsm,
          blankDifferentMasterDependentEsm,
          blankEsl,
          blankEsp,
          blankDifferentEsp,
          blankMasterDependentEsp,
          blankDifferentMasterDependentEsp,
          blankPluginDependentEsp,
          blankDifferentPluginDependentEsp,
      };
    } else {
      return {
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

  void SetBlueprintFlag(const std::filesystem::path& path) {
    auto bytes = ReadFile(path);
    bytes[9] = 0x8;
    WriteFile(path, bytes);
  }

  static bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
      return false;
    }

    auto view = std::string_view(str);
    view.remove_prefix(str.length() - suffix.length());

    return view == suffix;
  }

private:
  GameType gameType_;
  const std::filesystem::path rootTestPath;

protected:
  const std::string french;
  const std::string german;

  const std::filesystem::path missingPath;
  const std::filesystem::path gamePath;
  const std::filesystem::path dataPath;
  const std::filesystem::path localPath;
  const std::filesystem::path metadataFilesPath;

  const std::string masterFile;
  const std::string missingEsp;
  const std::string nonPluginFile;
  const std::string invalidPlugin;
  const std::string blankEsm;
  const std::string blankFullEsm;
  const std::string blankMediumEsm;
  const std::string blankDifferentEsm;
  const std::string blankMasterDependentEsm;
  const std::string blankDifferentMasterDependentEsm;
  const std::string blankEsl;
  const std::string blankEsp;
  const std::string blankDifferentEsp;
  const std::string blankMasterDependentEsp;
  const std::string blankDifferentMasterDependentEsp;
  const std::string blankPluginDependentEsp;
  const std::string blankDifferentPluginDependentEsp;

  const uint32_t blankEsmCrc;

private:
  std::string getMasterFile() const {
    if (gameType_ == GameType::tes3 || gameType_ == GameType::openmw)
      return "Morrowind.esm";
    else if (gameType_ == GameType::tes4 ||
             gameType_ == GameType::oblivionRemastered)
      return "Oblivion.esm";
    else if (gameType_ == GameType::tes5 || gameType_ == GameType::tes5se ||
             gameType_ == GameType::tes5vr)
      return "Skyrim.esm";
    else if (gameType_ == GameType::fo3)
      return "Fallout3.esm";
    else if (gameType_ == GameType::fonv)
      return "FalloutNV.esm";
    else if (gameType_ == GameType::fo4 || gameType_ == GameType::fo4vr)
      return "Fallout4.esm";
    else if (gameType_ == GameType::starfield)
      return "Starfield.esm";
    else
      throw std::logic_error("Unrecognised game type");
  }

  std::string getPluginsFolder() const {
    if (gameType_ == GameType::openmw) {
      return "resources/vfs";
    } else if (gameType_ == GameType::tes3) {
      return "Data Files";
    } else if (gameType_ == GameType::oblivionRemastered) {
      return "OblivionRemastered/Content/Dev/ObvData/Data";
    } else {
      return "Data";
    }
  }

  uint32_t getBlankEsmCrc() const {
    switch (gameType_) {
      case GameType::tes3:
      case GameType::openmw:
        return 0x790DC6FB;
      case GameType::tes4:
      case GameType::oblivionRemastered:
        return 0x374E2A6F;
      case GameType::starfield:
        return 0xDE586309;
      default:
        return 0x6A1273DC;
    }
  }

  void setLoadOrder(
      const std::vector<std::pair<std::string, bool>>& loadOrder) const {
    if (gameType_ == GameType::tes3) {
      std::ofstream out(gamePath / "Morrowind.ini");
      for (const auto& plugin : loadOrder) {
        if (plugin.second) {
          out << "GameFile0=" << plugin.first << std::endl;
        }
      }
    } else if (gameType_ == GameType::openmw) {
      std::ofstream out(localPath / "openmw.cfg");

      for (const auto& plugin : loadOrder) {
        if (plugin.second) {
          out << "content=" << plugin.first << std::endl;
        }
      }
    } else {
      const auto& parentPath =
          gameType_ == GameType::oblivionRemastered ? dataPath : localPath;
      std::ofstream out(parentPath / "Plugins.txt");
      for (const auto& plugin : loadOrder) {
        if (supportsLightPlugins(gameType_)) {
          if (plugin.second)
            out << '*';
        } else if (!plugin.second)
          continue;

        out << plugin.first << std::endl;
      }
    }

    if (isLoadOrderTimestampBased(gameType_)) {
      std::filesystem::file_time_type modificationTime =
          std::filesystem::file_time_type::clock::now();
      for (const auto& plugin : loadOrder) {
        if (std::filesystem::exists(
                dataPath / std::filesystem::path(plugin.first + ".ghost"))) {
          std::filesystem::last_write_time(
              dataPath / std::filesystem::path(plugin.first + ".ghost"),
              modificationTime);
        } else {
          std::filesystem::last_write_time(dataPath / plugin.first,
                                           modificationTime);
        }
        modificationTime += std::chrono::seconds(60);
      }
    } else if (gameType_ == GameType::tes5 ||
               gameType_ == GameType::oblivionRemastered) {
      const auto& parentPath =
          gameType_ == GameType::oblivionRemastered ? dataPath : localPath;
      std::ofstream out(parentPath / "loadorder.txt");
      for (const auto& plugin : loadOrder) out << plugin.first << std::endl;
    }
  }

  static bool isLoadOrderTimestampBased(GameType gameType) {
    return gameType == GameType::tes3 || gameType == GameType::tes4 ||
           gameType == GameType::fo3 || gameType == GameType::fonv;
  }
};
}
}
#endif
