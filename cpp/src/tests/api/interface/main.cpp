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

#include <filesystem>

#include "loot/api.h"
#include "tests/api/interface/metadata/file_test.h"
#include "tests/api/interface/metadata/group_test.h"
#include "tests/api/interface/metadata/location_test.h"
#include "tests/api/interface/metadata/message_content_test.h"
#include "tests/api/interface/metadata/message_test.h"
#include "tests/api/interface/metadata/plugin_cleaning_data_test.h"
#include "tests/api/interface/metadata/plugin_metadata_test.h"
#include "tests/api/interface/metadata/tag_test.h"
#include "tests/api/interface/create_game_handle_test.h"
#include "tests/api/interface/database_interface_test.h"
#include "tests/api/interface/game_interface_test.h"
#include "tests/api/interface/is_compatible_test.h"
#include "tests/api/interface/plugin_interface_test.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#ifdef _WIN32
TEST(Filesystem,
     pathStringConstructorDoesNotConvertCharacterEncodingFromUtf8ToNative) {
  std::string utf8 = u8"Andr\u00E9_settings.toml";
  std::u16string utf16 = u"Andr\u00E9_settings.toml";

  ASSERT_EQ('\xc3', utf8[4]);
  ASSERT_EQ('\xa9', utf8[5]);

  std::filesystem::path path(utf8);

  EXPECT_EQ(utf8, path.string());
  EXPECT_NE(utf8, path.u8string());
  EXPECT_NE(utf16, path.u16string());
}

TEST(
    Filesystem,
    pathStringAndLocaleConstructorDoesNotConvertCharacterEncodingFromUtf8WithClassicLocale) {
  std::string utf8 = u8"Andr\u00E9_settings.toml";
  std::u16string utf16 = u"Andr\u00E9_settings.toml";

  ASSERT_EQ('\xc3', utf8[4]);
  ASSERT_EQ('\xa9', utf8[5]);

  std::filesystem::path path(utf8, std::locale::classic());

  EXPECT_EQ(utf8, path.string());

  EXPECT_NE(utf8, path.u8string());
  EXPECT_NE(utf16, path.u16string());
}
#else
TEST(Filesystem, pathStringConstructorUsesNativeEncodingOfUtf8) {
  std::string utf8 = u8"Andr\u00E9_settings.toml";
  std::u16string utf16 = u"Andr\u00E9_settings.toml";

  ASSERT_EQ('\xc3', utf8[4]);
  ASSERT_EQ('\xa9', utf8[5]);

  std::filesystem::path path(utf8);

  EXPECT_EQ(utf8, path.string());
  EXPECT_EQ(utf8, path.u8string());
  EXPECT_EQ(utf16, path.u16string());
}
#endif

TEST(Filesystem, u8pathConvertsCharacterEncodingFromUtf8ToNative) {
  std::string utf8 = u8"Andr\u00E9_settings.toml";
  std::u16string utf16 = u"Andr\u00E9_settings.toml";

  ASSERT_EQ('\xc3', utf8[4]);
  ASSERT_EQ('\xa9', utf8[5]);

  std::filesystem::path path = std::filesystem::u8path(utf8);

#ifdef _WIN32
  EXPECT_NE(utf8, path.string());
#else
  EXPECT_EQ(utf8, path.string());
#endif

  EXPECT_EQ(utf8, path.u8string());
  EXPECT_EQ(utf16, path.u16string());
}

#ifdef _WIN32
TEST(WindowsRegistry, hasLongPathsEnabled) { 
    EXPECT_TRUE(loot::test::windowsHasLongPathsEnabled());
}
#endif

namespace loot {
namespace test {
void testLoggingCallback(LogLevel, std::string_view) {
  // Do nothing.
}

struct TestLogger {
  void callback(LogLevel, std::string_view message) {
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
    SetLoggingCallback([](LogLevel, std::string_view) {});
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
        "IV: Oblivion\" with game path \"dummy\"",
        testLogger.loggedMessages);

    SetLoggingCallback([](LogLevel, std::string_view) {});
  }
}

TEST(SetLoggingCallback, shouldAcceptALambdaFunction) {
  std::string loggedMessages;
  auto callback = [&](LogLevel, std::string_view string) {
    loggedMessages += std::string(string);
  };
  SetLoggingCallback(callback);

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    EXPECT_EQ(
        "Attempting to create a game handle for game type \"The Elder Scrolls "
        "IV: Oblivion\" with game path \"dummy\"",
        loggedMessages);

    SetLoggingCallback([](LogLevel, std::string_view) {});
  }
}

TEST(SetLoggingCallback,
     shouldNotBreakLoggingIfPassedLambdaFunctionGoesOutOfScope) {
  std::string loggedMessages;
  {
    SetLoggingCallback([&](LogLevel, std::string_view string) {
      loggedMessages += std::string(string);
    });
  }

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    EXPECT_EQ(
        "Attempting to create a game handle for game type \"The Elder Scrolls "
        "IV: Oblivion\" with game path \"dummy\"",
        loggedMessages);

    SetLoggingCallback([](LogLevel, std::string_view) {});
  }
}

TEST(SetLogLevel, shouldOnlyRunTheCallbackForMessagesAtOrAboveTheGivenLevel) {
  std::vector<std::pair<LogLevel, std::string>> loggedMessages;
  auto callback = [&](LogLevel level, std::string_view string) {
    loggedMessages.push_back(std::make_pair(level, std::string(string)));
  };
  SetLoggingCallback(callback);
  SetLogLevel(LogLevel::error);

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    EXPECT_TRUE(loggedMessages.empty());
  }

  SetLogLevel(LogLevel::info);

  try {
    CreateGameHandle(GameType::tes4, "dummy");
    FAIL();
  } catch (...) {
    ASSERT_EQ(1, loggedMessages.size());
    EXPECT_EQ(LogLevel::info, loggedMessages[0].first);
    EXPECT_EQ(
        "Attempting to create a game handle for game type \"The Elder Scrolls "
        "IV: Oblivion\" with game path \"dummy\"",
        loggedMessages[0].second);

    SetLoggingCallback([](LogLevel, std::string_view) {});
  }
}
}
}
