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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
inline constexpr std::string_view NON_ASCII_ESP = u8"non\u00C1scii.esp";

class CommonGameTestFixture : public ::testing::Test {
protected:
  CommonGameTestFixture(GameType gameType) :
      gameType_(gameType),
      rootTestPath(getRootTestPath()),
      missingPath(rootTestPath / "missing"),
      gamePath(rootTestPath / "games" / "game"),
      dataPath(gamePath / getPluginsFolder()),
      localPath(rootTestPath / "local" / "game") {
    std::filesystem::create_directories(dataPath);
    std::filesystem::create_directories(localPath);
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

  void TearDown() override { std::filesystem::remove_all(rootTestPath); }

  std::vector<std::string> getLoadOrder() {
    std::vector<std::string> actual;
    if (isLoadOrderTimestampBased(gameType_)) {
      std::map<std::filesystem::file_time_type, std::string> loadOrder;
      for (std::filesystem::directory_iterator it(dataPath);
           it != std::filesystem::directory_iterator();
           ++it) {
        if (std::filesystem::is_regular_file(it->status())) {
          std::string filename = it->path().filename().u8string();
          if (filename == NON_PLUGIN_FILE)
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

private:
  GameType gameType_;
  std::filesystem::path rootTestPath;

protected:
  std::filesystem::path missingPath;
  std::filesystem::path gamePath;
  std::filesystem::path dataPath;
  std::filesystem::path localPath;
  std::filesystem::path metadataFilesPath;

private:
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

protected:
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
};
}
}
#endif
