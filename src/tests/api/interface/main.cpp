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

#include <gtest/gtest.h>

#include "loot/api.h"
#include "tests/api/interface/create_game_handle_test.h"
#include "tests/api/interface/database_interface_test.h"
#include "tests/api/interface/game_interface_test.h"
#include "tests/api/interface/is_compatible_test.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

namespace loot {
namespace test {
TEST(SetLoggingCallback, shouldWriteMessagesToGivenCallback) {
  std::string loggedMessages;
  SetLoggingCallback([&](LogLevel level, const char *string) {
    loggedMessages += std::string(string);
  });

  try {
    CreateGameHandle(GameType::tes4, "dummy");
  } catch (...) {
    EXPECT_EQ(
        "Attempting to create a game handle with game path \"dummy\" "
        "and local path \"\"",
        loggedMessages);

    SetLoggingCallback([](LogLevel, const char *) {});
    return;
  }

  FAIL();
}
}
}
