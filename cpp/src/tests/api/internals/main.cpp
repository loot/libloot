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
    along with LOOT. If not, see
    <https://www.gnu.org/licenses/>.
    */

#include <gtest/gtest.h>

#include <filesystem>

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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
