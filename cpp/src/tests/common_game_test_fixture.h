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

inline constexpr std::string_view FRENCH = "fr";
inline constexpr std::string_view GERMAN = "de";

inline constexpr std::string_view MISSING_ESP = "Blank.missing.esp";
inline constexpr std::string_view NON_PLUGIN_FILE = "NotAPlugin.esm";
inline constexpr std::string_view INVALID_PLUGIN = "Invalid.esm";
inline constexpr std::string_view BLANK_ESM = "Blank.esm";
inline constexpr std::string_view BLANK_FULL_ESM = "Blank.full.esm";
inline constexpr std::string_view BLANK_MEDIUM_ESM = "Blank.medium.esm";
inline constexpr std::string_view BLANK_DIFFERENT_ESM = "Blank - Different.esm";
inline constexpr std::string_view BLANK_MASTER_DEPENDENT_ESM =
    "Blank - Master Dependent.esm";
inline constexpr std::string_view BLANK_DIFFERENT_MASTER_DEPENDENT_ESM =
    "Blank - Different Master Dependent.esm";
inline constexpr std::string_view BLANK_ESL = "Blank.esl";
inline constexpr std::string_view BLANK_ESP = "Blank.esp";
inline constexpr std::string_view BLANK_DIFFERENT_ESP = "Blank - Different.esp";
inline constexpr std::string_view BLANK_MASTER_DEPENDENT_ESP =
    "Blank - Master Dependent.esp";
inline constexpr std::string_view BLANK_DIFFERENT_MASTER_DEPENDENT_ESP =
    "Blank - Different Master Dependent.esp";
inline constexpr std::string_view BLANK_PLUGIN_DEPENDENT_ESP =
    "Blank - Plugin Dependent.esp";
inline constexpr std::string_view BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP =
    "Blank - Different Plugin Dependent.esp";
inline constexpr std::string_view BLANK_SMALL_ESM = "Blank.small.esm";
inline constexpr std::string_view BLANK_OVERRIDE_FULL_ESM =
    "Blank - Override.full.esm";
inline constexpr std::string_view BLANK_OVERRIDE_ESP = "Blank - Override.esp";
static constexpr std::string_view NON_ASCII_ESP = u8"non\u00C1scii.esp";

class CommonGameTestFixture : public ::testing::Test {
protected:
  CommonGameTestFixture(GameType gameType) :
      gameType_(gameType),
      rootTestPath(getRootTestPath()),
      missingPath(rootTestPath / "missing"),
      gamePath(rootTestPath / "games" / "game"),
      dataPath(gamePath / getPluginsFolder()),
      localPath(rootTestPath / "local" / "game"),
      metadataFilesPath(rootTestPath / "metadata"),
      masterFile(getMasterFile()),
      missingEsp(MISSING_ESP),
      nonPluginFile(NON_PLUGIN_FILE),
      blankEsm(BLANK_ESM),
      blankFullEsm(BLANK_FULL_ESM),
      blankMediumEsm(BLANK_MEDIUM_ESM),
      blankDifferentEsm(BLANK_DIFFERENT_ESM),
      blankMasterDependentEsm(BLANK_MASTER_DEPENDENT_ESM),
      blankDifferentMasterDependentEsm(BLANK_DIFFERENT_MASTER_DEPENDENT_ESM),
      blankEsl(BLANK_ESL),
      blankEsp(BLANK_ESP),
      blankDifferentEsp(BLANK_DIFFERENT_ESP),
      blankMasterDependentEsp(BLANK_MASTER_DEPENDENT_ESP),
      blankDifferentMasterDependentEsp(BLANK_DIFFERENT_MASTER_DEPENDENT_ESP),
      blankPluginDependentEsp(BLANK_PLUGIN_DEPENDENT_ESP),
      blankDifferentPluginDependentEsp(BLANK_DIFFERENT_PLUGIN_DEPENDENT_ESP),
      blankEsmCrc(getBlankEsmCrc()) {
    using std::filesystem::create_directories;

    create_directories(dataPath);
    create_directories(localPath);
    create_directories(metadataFilesPath);
  }

  void copyPlugin(std::string_view filename) {
    copyDataFile(getSourcePluginsPath(), filename);
  }

  void copyPlugin(std::string_view sourceFilename,
                  std::string_view destinationFilename) {
    copyDataFile(getSourcePluginsPath(), sourceFilename, destinationFilename);
  }

  void copyDataFile(const std::filesystem::path& sourceParentPath,
                    std::string_view filename) {
    copyDataFile(sourceParentPath, filename, filename);
  }

  void copyDataFile(const std::filesystem::path& sourceParentPath,
                    std::string_view sourceFilename,
                    std::string_view destinationFilename) {
    std::filesystem::copy_file(
        sourceParentPath / std::filesystem::u8path(sourceFilename),
        dataPath / std::filesystem::u8path(destinationFilename));

    ASSERT_TRUE(std::filesystem::exists(
        dataPath / std::filesystem::u8path(destinationFilename)));
  }

  void TearDown() override {
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

  void SetBlueprintFlag(const std::filesystem::path& path) {
    auto bytes = ReadFile(path);
    bytes[9] = 0x8;
    WriteFile(path, bytes);
  }

  static bool startsWith(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
      return false;
    }

    auto view = std::string_view(str);
    view.remove_suffix(str.length() - prefix.length());

    return view == prefix;
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
  const std::filesystem::path missingPath;
  const std::filesystem::path gamePath;
  const std::filesystem::path dataPath;
  const std::filesystem::path localPath;
  const std::filesystem::path metadataFilesPath;

  const std::string masterFile;
  const std::string missingEsp;
  const std::string nonPluginFile;
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

protected:
  void setLoadOrder(
      const std::vector<std::pair<std::string, bool>>& loadOrder) const {
    if (gameType_ == GameType::tes3) {
      std::ofstream out(gamePath / "Morrowind.ini");
      out << "[Game Files]" << std::endl;

      size_t activeIndex = 0;
      for (const auto& plugin : loadOrder) {
        if (plugin.second) {
          out << "GameFile" << activeIndex << "=" << plugin.first << std::endl;
          activeIndex += 1;
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

private:
  static bool isLoadOrderTimestampBased(GameType gameType) {
    return gameType == GameType::tes3 || gameType == GameType::tes4 ||
           gameType == GameType::fo3 || gameType == GameType::fonv;
  }
};
}
}
#endif
