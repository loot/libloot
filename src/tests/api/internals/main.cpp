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

#include <boost/locale.hpp>

#include "tests/api/internals/game/game_cache_test.h"
#include "tests/api/internals/game/game_test.h"
#include "tests/api/internals/game/load_order_handler_test.h"
#include "tests/api/internals/helpers/crc_test.h"
#include "tests/api/internals/helpers/git_helper_test.h"
#include "tests/api/internals/helpers/version_test.h"
#include "tests/api/internals/helpers/yaml_set_helpers_test.h"
#include "tests/api/internals/masterlist_test.h"
#include "tests/api/internals/metadata/condition_evaluator_test.h"
#include "tests/api/internals/metadata/condition_grammar_test.h"
#include "tests/api/internals/metadata/conditional_metadata_test.h"
#include "tests/api/internals/metadata/file_test.h"
#include "tests/api/internals/metadata/group_test.h"
#include "tests/api/internals/metadata/location_test.h"
#include "tests/api/internals/metadata/message_content_test.h"
#include "tests/api/internals/metadata/message_test.h"
#include "tests/api/internals/metadata/plugin_cleaning_data_test.h"
#include "tests/api/internals/metadata/plugin_metadata_test.h"
#include "tests/api/internals/metadata/tag_test.h"
#include "tests/api/internals/metadata_list_test.h"
#include "tests/api/internals/plugin_test.h"
#include "tests/api/internals/sorting/group_sort_test.h"
#include "tests/api/internals/sorting/plugin_sorter_test.h"

TEST(ModuloOperator, shouldConformToTheCpp11Standard) {
  // C++11 defines the modulo operator more strongly
  // (only x % 0 is left undefined), whereas C++03
  // only defined the operator for positive first operand.
  // Test that the modulo operator has been implemented
  // according to C++11.

  EXPECT_EQ(0, 20 % 5);
  EXPECT_EQ(0, 20 % -5);
  EXPECT_EQ(0, -20 % 5);
  EXPECT_EQ(0, -20 % -5);

  EXPECT_EQ(2, 9 % 7);
  EXPECT_EQ(2, 9 % -7);
  EXPECT_EQ(-2, -9 % 7);
  EXPECT_EQ(-2, -9 % -7);
}

TEST(YamlCpp, shouldSupportMergeKeys) {
  YAML::Node node = YAML::Load("{<<: {a: 1}}");
  ASSERT_TRUE(node["a"]);
  EXPECT_EQ(1, node["a"].as<int>());
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
#endif

// This relies on the global locale being set up correctly on Windows (see the
// main method).
TEST(Filesystem,
     pathStringAndLocaleConstructorConvertsCharacterEncodingFromUtf8ToNative) {
  std::string utf8 = u8"Andr\u00E9_settings.toml";
  std::u16string utf16 = u"Andr\u00E9_settings.toml";

  ASSERT_EQ('\xc3', utf8[4]);
  ASSERT_EQ('\xa9', utf8[5]);

  std::filesystem::path path(utf8, std::locale());

#ifdef _WIN32
  EXPECT_NE(utf8, path.string());
#else
  EXPECT_EQ(utf8, path.string());
#endif

  EXPECT_EQ(utf8, path.u8string());
  EXPECT_EQ(utf16, path.u16string());
}

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

TEST(Filesystem, shouldBeAbleToWriteToAndReadFromAUtf8Path) {
  std::string utf8 = u8"Andr\u00E9_settings.toml";
  auto path = std::filesystem::u8path(utf8);
  std::string output = u8"Test cont\u00E9nt";

  std::ofstream out(path);
  out << output;
  out.close();

  EXPECT_TRUE(std::filesystem::exists(path));

  std::string input;
  std::ifstream in(path);
  std::getline(in, input);
  in.close();

  EXPECT_EQ(output, input);

  std::filesystem::remove(path);
}

TEST(Filesystem, equalityShouldBeCaseSensitive) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license");

  EXPECT_NE(lower, upper);
}

#ifdef _WIN32
TEST(Filesystem, equivalentShouldRequireThatBothPathsExist) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license2");

  EXPECT_THROW(std::filesystem::equivalent(lower, upper), std::filesystem::filesystem_error);
}
#else
TEST(Filesystem, equivalentShouldNotRequireThatBothPathsExist) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license2");

  EXPECT_FALSE(std::filesystem::equivalent(lower, upper));
}
#endif

TEST(Filesystem, canonicalShouldRequireThatThePathExists) {
  EXPECT_THROW(std::filesystem::canonical("license2"), std::filesystem::filesystem_error);
}

#ifdef _WIN32
TEST(Filesystem, equivalentShouldBeCaseInsensitive) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license");

  EXPECT_TRUE(std::filesystem::equivalent(lower, upper));
}

TEST(Filesystem, canonicalShouldFoldCase) {
  auto upper = std::filesystem::canonical("LICENSE");
  auto lower = std::filesystem::canonical("license");

  EXPECT_EQ(lower, upper);
}
#else
TEST(Filesystem, equivalentShouldBeCaseSensitive) {
  auto upper = std::filesystem::path("LICENSE");
  auto lower = std::filesystem::path("license");

  EXPECT_FALSE(std::filesystem::equivalent(lower, upper));
}

TEST(Filesystem, canonicalShouldNotFoldCase) {
  std::ofstream out("license");
  out.close();

  auto upper = std::filesystem::canonical("LICENSE");
  auto lower = std::filesystem::canonical("license");

  EXPECT_NE(lower, upper);
}
#endif

int main(int argc, char **argv) {
  // Set the locale to get encoding conversions working correctly.
  std::locale::global(boost::locale::generator().generate(""));

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
