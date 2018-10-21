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

#ifndef LOOT_TESTS_API_INTERNALS_HELPERS_VERSION_TEST
#define LOOT_TESTS_API_INTERNALS_HELPERS_VERSION_TEST

#include "api/helpers/version.h"
#include "loot/loot_version.h"

#include <gtest/gtest.h>

namespace loot {
namespace test {

TEST(Version, shouldExtractAVersionContainingASingleDigit) {
  EXPECT_EQ("5", ExtractVersion("5").value());
}

TEST(Version, shouldExtractAVersionContainingMultipleDigits) {
  EXPECT_EQ("10", ExtractVersion("10").value());
}

TEST(Version, shouldExtractAVersionContainingMultipleNumbers) {
  
  EXPECT_EQ("10.11.12.13", ExtractVersion("10.11.12.13").value());
}

TEST(Version, shouldExtractASemanticVersion) {
  EXPECT_EQ("1.0.0-x.7.z.92", ExtractVersion("1.0.0-x.7.z.92+exp.sha.5114f85").value());
}

TEST(Version,
     shouldExtractAPseudosemExtendedVersionStoppingAtTheFirstSpaceSeparator) {
  EXPECT_EQ("01.0.0_alpha:1-2", ExtractVersion("01.0.0_alpha:1-2 3").value());
}

TEST(Version, shouldExtractAVersionSubstring) {
  EXPECT_EQ("5.0", ExtractVersion("v5.0").value());
}

TEST(Version, shouldBeEmptyIfInputStringContainedNoVersion) {
  EXPECT_FALSE(ExtractVersion("The quick brown fox jumped over the lazy dog.").has_value());
}

TEST(Version, shouldExtractTimestampWithForwardslashDateSeparators) {
  // Found in a Bashed Patch. Though the timestamp isn't useful to
  // LOOT, it is semantically a version, and extracting it is far
  // easier than trying to skip it and the number of records changed.
  auto text = ExtractVersion("Updated: 10/09/2016 13:15:18\r\n\r\nRecords Changed: 43");
  EXPECT_EQ("10/09/2016 13:15:18", text.value());
}

TEST(Version, shouldNotExtractTrailingPeriods) {
  // Found in <http://www.nexusmods.com/fallout4/mods/2955/>.
  EXPECT_EQ("0.2", ExtractVersion("Version 0.2.").value());
}

TEST(Version, shouldExtractVersionAfterTextWhenPrecededByVersionColonString) {
  // Found in <http://www.nexusmods.com/skyrim/mods/71214/>.
  EXPECT_EQ("3.0.0", ExtractVersion("Legendary Edition\r\n\r\nVersion: 3.0.0").value());
}

TEST(Version, shouldIgnoreNumbersContainingCommas) {
  // Found in <http://www.nexusmods.com/oblivion/mods/5296/>.
  EXPECT_EQ("3.5.3", ExtractVersion("fixing over 2,300 bugs so far! Version: 3.5.3").value());
}

TEST(Version, shouldExtractVersionBeforeText) {
  // Found in <http://www.nexusmods.com/fallout3/mods/19122/>.
  EXPECT_EQ("2.1", ExtractVersion("Version: 2.1 The Unofficial Fallout 3 Patch").value());
}

TEST(Version, shouldExtractVersionWithPrecedingV) {
  // Found in <http://www.nexusmods.com/oblivion/mods/22795/>.
  EXPECT_EQ("2.11", ExtractVersion("V2.11\r\n\r\n{{BASH:Invent}}").value());
}

TEST(Version, shouldExtractVersionWithPrecedingColonPeriodWhitespace) {
  // Found in <http://www.nexusmods.com/oblivion/mods/45570>.
  EXPECT_EQ("1.09", ExtractVersion("Version:. 1.09").value());
}

TEST(Version, shouldExtractVersionWithLettersImmediatelyAfterNumbers) {
  // Found in <http://www.nexusmods.com/skyrim/mods/19>.
  auto text = ExtractVersion(
    "comprehensive bugfixing mod for The Elder Scrolls V: "
    "Skyrim\r\n\r\nVersion: 2.1.3b\r\n\r\n");
  EXPECT_EQ("2.1.3b", text.value());
}

TEST(Version, shouldExtractVersionWithPeriodAndNoPrecedingIdentifier) {
  // Found in <http://www.nexusmods.com/skyrim/mods/3863>.
  EXPECT_EQ("5.1", ExtractVersion("SkyUI 5.1").value());
}

TEST(Version, shouldNotExtractSingleDigitInSentence) {
  // Found in <http://www.nexusmods.com/skyrim/mods/4708>.
  auto text = ExtractVersion(
      "Adds 8 variants of Triss Merigold's outfit from \"The Witcher 2\"");
  EXPECT_FALSE(text.has_value());
}

TEST(Version, shouldPreferVersionPrefixedNumbersOverVersionsInSentence) {
  // Found in <http://www.nexusmods.com/skyrim/mods/47327>
  auto text = ExtractVersion(
      "Requires Skyrim patch 1.9.32.0.8 or greater.\n"
      "Requires Unofficial Skyrim Legendary Edition Patch 3.0.0 or greater.\n"
      "Version 2.0.0");
  EXPECT_EQ("2.0.0", text.value());
}

TEST(Version, shouldExtractSingleDigitVersionPrecededByV) {
  // Found in <http://www.nexusmods.com/skyrim/mods/19733>
  EXPECT_EQ("8", ExtractVersion("Immersive Armors v8 Main Plugin").value());
}

TEST(Version, shouldPreferVersionPrefixedNumbersOverVPrefixedNumber) {
  // Found in <http://www.nexusmods.com/skyrim/mods/43773>
  auto text = ExtractVersion(
      "Compatibility patch for AOS v2.5 and True Storms v1.5 (or "
      "later),\nPatch Version: 1.0");
  EXPECT_EQ("1.0", text.value());
}
}
}

#endif
