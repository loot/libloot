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
void testLoggingCallback(LogLevel, const char *) {
  // Do nothing.
}

struct TestLogger {
  void callback(LogLevel, const char *message) {
    loggedMessages += std::string(message);
  }

  std::string loggedMessages;
};

TEST(SetLoggingCallback, shouldAcceptAFreeFunction) {
  SetLoggingCallback(testLoggingCallback);

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    SetLoggingCallback([](LogLevel, const char *) {});
  }
}

TEST(SetLoggingCallback, shouldAcceptAMemberFunction) {
  TestLogger testLogger;
  auto boundCallback = std::bind(&TestLogger::callback,
                                 &testLogger,
                                 std::placeholders::_1,
                                 std::placeholders::_2);
  SetLoggingCallback(boundCallback);

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    EXPECT_EQ(
        "Attempting to create a game handle for game type \"The Elder Scrolls "
        "IV: Oblivion\" with game path \"dummy\" and game local path \"\"",
        testLogger.loggedMessages);

    SetLoggingCallback([](LogLevel, const char *) {});
  }
}

TEST(SetLoggingCallback, shouldAcceptALambdaFunction) {
  std::string loggedMessages;
  auto callback = [&](LogLevel, const char *string) {
    loggedMessages += std::string(string);
  };
  SetLoggingCallback(callback);

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    EXPECT_EQ(
        "Attempting to create a game handle for game type \"The Elder Scrolls "
        "IV: Oblivion\" with game path \"dummy\" and game local path \"\"",
        loggedMessages);

    SetLoggingCallback([](LogLevel, const char *) {});
  }
}

TEST(SetLoggingCallback,
     shouldNotBreakLoggingIfPassedLambdaFunctionGoesOutOfScope) {
  std::string loggedMessages;
  {
    SetLoggingCallback([&](LogLevel, const char *string) {
      loggedMessages += std::string(string);
    });
  }

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    EXPECT_EQ(
        "Attempting to create a game handle for game type \"The Elder Scrolls "
        "IV: Oblivion\" with game path \"dummy\" and game local path \"\"",
        loggedMessages);

    SetLoggingCallback([](LogLevel, const char *) {});
  }
}
}
}
