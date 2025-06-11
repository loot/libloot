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

#ifndef LOOT_TESTS_TEST_HELPERS
#define LOOT_TESTS_TEST_HELPERS

#include <filesystem>
#include <random>
#include <string>

#include "loot/enum/game_type.h"

namespace loot::test {
bool supportsLightPlugins(GameType gameType) {
  return gameType == GameType::tes5se || gameType == GameType::tes5vr ||
         gameType == GameType::fo4 || gameType == GameType::fo4vr ||
         gameType == GameType::starfield;
}

std::filesystem::path getSourcePluginsPath(GameType gameType) {
  using std::filesystem::absolute;
  if (gameType == GameType::tes3 || gameType == GameType::openmw) {
    return absolute("./testing-plugins/Morrowind/Data Files");
  } else if (gameType == GameType::tes4 ||
             gameType == GameType::oblivionRemastered) {
    return absolute("./testing-plugins/Oblivion/Data");
  } else if (gameType == GameType::starfield) {
    return absolute("./testing-plugins/Starfield/Data");
  } else if (supportsLightPlugins(gameType)) {
    return absolute("./testing-plugins/SkyrimSE/Data");
  } else {
    return absolute("./testing-plugins/Skyrim/Data");
  }
}

std::filesystem::path getSourceArchivesPath(GameType gameType) {
  if (gameType == GameType::fo4 || gameType == GameType::fo4vr ||
      gameType == GameType::starfield) {
    return "./testing-plugins/Fallout 4/Data";
  } else {
    return getSourcePluginsPath(gameType);
  }
}

std::filesystem::path getRootTestPath() {
  std::random_device randomDevice;
  std::default_random_engine prng(randomDevice());
  std::uniform_int_distribution dist(0x61,
                                     0x7A);  // values of a-z in ASCII/UTF-8.

  // The non-ASCII character is there to ensure test coverage of non-ASCII path
  // handling.
  std::u8string directoryName = u8"libloot-t\u00E9st-";

  for (int i = 0; i < 16; i += 1) {
    directoryName.push_back(static_cast<char8_t>(dist(prng)));
  }

  return std::filesystem::absolute(std::filesystem::temp_directory_path() /
                                   std::filesystem::u8path(directoryName));
}
}

#endif
