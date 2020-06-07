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

#ifndef LOOT_TESTS_API_INTERNALS_HELPERS_TEXT_TEST
#define LOOT_TESTS_API_INTERNALS_HELPERS_TEXT_TEST

#include "api/helpers/text.h"
#include "loot/loot_version.h"

#include <gtest/gtest.h>

namespace loot {
namespace test {

TEST(ExtractBashTags, shouldExtractTagsFromPluginDescriptionText) {
  auto description = R"raw(Unofficial Skyrim Special Edition Patch

A comprehensive bugfixing mod for The Elder Scrolls V: Skyrim - Special Edition

Version: 4.1.4

Requires Skyrim Special Edition 1.5.39 or greater.

{{BASH:C.Climate,C.Encounter,C.ImageSpace,C.Light,C.Location,C.Music,C.Name,C.Owner,C.Water,Delev,Graphics,Invent,Names,Relev,Sound,Stats}})raw";

  auto tags = ExtractBashTags(description);

  std::vector<Tag> expectedTags({
      Tag("C.Climate"),
      Tag("C.Encounter"),
      Tag("C.ImageSpace"),
      Tag("C.Light"),
      Tag("C.Location"),
      Tag("C.Music"),
      Tag("C.Name"),
      Tag("C.Owner"),
      Tag("C.Water"),
      Tag("Delev"),
      Tag("Graphics"),
      Tag("Invent"),
      Tag("Names"),
      Tag("Relev"),
      Tag("Sound"),
      Tag("Stats"),
  });

  EXPECT_EQ(expectedTags, tags);
}

TEST(ExtractVersion, shouldExtractAVersionContainingASingleDigit) {
  EXPECT_EQ("5", ExtractVersion("5").value());
}

TEST(ExtractVersion, shouldExtractAVersionContainingMultipleDigits) {
  EXPECT_EQ("10", ExtractVersion("10").value());
}

TEST(ExtractVersion, shouldExtractAVersionContainingMultipleNumbers) {
  EXPECT_EQ("10.11.12.13", ExtractVersion("10.11.12.13").value());
}

TEST(ExtractVersion, shouldExtractASemanticVersion) {
  EXPECT_EQ("1.0.0-x.7.z.92",
            ExtractVersion("1.0.0-x.7.z.92+exp.sha.5114f85").value());
}

TEST(ExtractVersion,
     shouldExtractAPseudosemExtendedVersionStoppingAtTheFirstSpaceSeparator) {
  EXPECT_EQ("01.0.0_alpha:1-2", ExtractVersion("01.0.0_alpha:1-2 3").value());
}

TEST(ExtractVersion, shouldExtractAVersionSubstring) {
  EXPECT_EQ("5.0", ExtractVersion("v5.0").value());
}

TEST(ExtractVersion, shouldBeEmptyIfInputStringContainedNoVersion) {
  EXPECT_FALSE(ExtractVersion("The quick brown fox jumped over the lazy dog.")
                   .has_value());
}

TEST(ExtractVersion, shouldExtractTimestampWithForwardslashDateSeparators) {
  // Found in a Bashed Patch. Though the timestamp isn't useful to
  // LOOT, it is semantically a ExtractVersion, and extracting it is far
  // easier than trying to skip it and the number of records changed.
  auto text =
      ExtractVersion("Updated: 10/09/2016 13:15:18\r\n\r\nRecords Changed: 43");
  EXPECT_EQ("10/09/2016 13:15:18", text.value());
}

TEST(ExtractVersion, shouldNotExtractTrailingPeriods) {
  // Found in <http://www.nexusmods.com/fallout4/mods/2955/>.
  EXPECT_EQ("0.2", ExtractVersion("Version 0.2.").value());
}

TEST(ExtractVersion,
     shouldExtractVersionAfterTextWhenPrecededByVersionColonString) {
  // Found in <http://www.nexusmods.com/skyrim/mods/71214/>.
  EXPECT_EQ("3.0.0",
            ExtractVersion("Legendary Edition\r\n\r\nVersion: 3.0.0").value());
}

TEST(ExtractVersion, shouldIgnoreNumbersContainingCommas) {
  // Found in <http://www.nexusmods.com/oblivion/mods/5296/>.
  EXPECT_EQ(
      "3.5.3",
      ExtractVersion("fixing over 2,300 bugs so far! Version: 3.5.3").value());
}

TEST(ExtractVersion, shouldExtractVersionBeforeText) {
  // Found in <http://www.nexusmods.com/fallout3/mods/19122/>.
  EXPECT_EQ(
      "2.1",
      ExtractVersion("Version: 2.1 The Unofficial Fallout 3 Patch").value());
}

TEST(ExtractVersion, shouldExtractVersionWithPrecedingV) {
  // Found in <http://www.nexusmods.com/oblivion/mods/22795/>.
  EXPECT_EQ("2.11", ExtractVersion("V2.11\r\n\r\n{{BASH:Invent}}").value());
}

TEST(ExtractVersion, shouldExtractVersionWithPrecedingColonPeriodWhitespace) {
  // Found in <http://www.nexusmods.com/oblivion/mods/45570>.
  EXPECT_EQ("1.09", ExtractVersion("Version:. 1.09").value());
}

TEST(ExtractVersion, shouldExtractVersionWithLettersImmediatelyAfterNumbers) {
  // Found in <http://www.nexusmods.com/skyrim/mods/19>.
  auto text = ExtractVersion(
      "comprehensive bugfixing mod for The Elder Scrolls V: "
      "Skyrim\r\n\r\nVersion: 2.1.3b\r\n\r\n");
  EXPECT_EQ("2.1.3b", text.value());
}

TEST(ExtractVersion, shouldExtractVersionWithPeriodAndNoPrecedingIdentifier) {
  // Found in <http://www.nexusmods.com/skyrim/mods/3863>.
  EXPECT_EQ("5.1", ExtractVersion("SkyUI 5.1").value());
}

TEST(ExtractVersion, shouldNotExtractSingleDigitInSentence) {
  // Found in <http://www.nexusmods.com/skyrim/mods/4708>.
  auto text = ExtractVersion(
      "Adds 8 variants of Triss Merigold's outfit from \"The Witcher 2\"");
  EXPECT_FALSE(text.has_value());
}

TEST(ExtractVersion, shouldPreferVersionPrefixedNumbersOverVersionsInSentence) {
  // Found in <http://www.nexusmods.com/skyrim/mods/47327>
  auto text = ExtractVersion(
      "Requires Skyrim patch 1.9.32.0.8 or greater.\n"
      "Requires Unofficial Skyrim Legendary Edition Patch 3.0.0 or greater.\n"
      "Version 2.0.0");
  EXPECT_EQ("2.0.0", text.value());
}

TEST(ExtractVersion, shouldExtractSingleDigitVersionPrecededByV) {
  // Found in <http://www.nexusmods.com/skyrim/mods/19733>
  EXPECT_EQ("8", ExtractVersion("Immersive Armors v8 Main Plugin").value());
}

TEST(ExtractVersion, shouldPreferVersionPrefixedNumbersOverVPrefixedNumber) {
  // Found in <http://www.nexusmods.com/skyrim/mods/43773>
  auto text = ExtractVersion(
      "Compatibility patch for AOS v2.5 and True Storms v1.5 (or "
      "later),\nPatch Version: 1.0");
  EXPECT_EQ("1.0", text.value());
}

TEST(ExtractVersion, shouldExtractSingleDigitAfterVersionColonSpace) {
  // Found in <https://www.nexusmods.com/oblivion/mods/14720>
  EXPECT_EQ("2", ExtractVersion("Version: 2 {{BASH:C.Water}}").value());
}

// MSVC interprets source files in the default code page, so
// for me u8"\xC3\x9C" != u8"\u00DC", which is a lot of fun.
// To avoid insanity, write non-ASCII characters as \uXXXX escapes.
// \u03a1 is greek rho uppercase 'Ρ'
// \u03c1 is greek rho lowercase 'ρ'
// \u03f1 is greek rho 'ϱ'
// \u0130 is turkish 'İ'
// \u0131 is turkish 'ı'

TEST(CompareFilenames, shouldBeCaseInsensitiveAndLocaleInvariant) {
  // ICU sees all three greek rhos as case-insensitively equal, unlike Windows.
  // A small enough deviation that it should hopefully be insignificant.
#ifdef _WIN32
  const char * turkishLocale = "tr-TR";
  const char * greekLocale = "el-GR";
  const int expectedRhoSymbolOrder = 1;
#else
  const char * turkishLocale = "tr_TR.UTF-8";
  const char * greekLocale = "el_GR.UTF-8";
  const int expectedRhoSymbolOrder = 0;
#endif

  EXPECT_EQ(0, CompareFilenames("i", "I"));
  EXPECT_EQ(-1, CompareFilenames("i", u8"\u0130"));
  EXPECT_EQ(-1, CompareFilenames("i", u8"\u0131"));
  EXPECT_EQ(-1, CompareFilenames("I", u8"\u0130"));
  EXPECT_EQ(-1, CompareFilenames("I", u8"\u0131"));
  EXPECT_EQ(-1, CompareFilenames(u8"\u0130", u8"\u0131"));
  EXPECT_EQ(expectedRhoSymbolOrder, CompareFilenames(u8"\u03f1", u8"\u03a1"));
  EXPECT_EQ(expectedRhoSymbolOrder, CompareFilenames(u8"\u03f1", u8"\u03c1"));
  EXPECT_EQ(0, CompareFilenames(u8"\u03a1", u8"\u03c1"));

  // Set locale to Turkish.
  std::locale::global(std::locale(turkishLocale));

  EXPECT_EQ(0, CompareFilenames("i", "I"));
  EXPECT_EQ(-1, CompareFilenames("i", u8"\u0130"));
  EXPECT_EQ(-1, CompareFilenames("i", u8"\u0131"));
  EXPECT_EQ(-1, CompareFilenames("I", u8"\u0130"));
  EXPECT_EQ(-1, CompareFilenames("I", u8"\u0131"));
  EXPECT_EQ(-1, CompareFilenames(u8"\u0130", u8"\u0131"));
  EXPECT_EQ(expectedRhoSymbolOrder, CompareFilenames(u8"\u03f1", u8"\u03a1"));
  EXPECT_EQ(expectedRhoSymbolOrder, CompareFilenames(u8"\u03f1", u8"\u03c1"));
  EXPECT_EQ(0, CompareFilenames(u8"\u03a1", u8"\u03c1"));

  // Set locale to Greek.
  std::locale::global(std::locale(greekLocale));

  EXPECT_EQ(0, CompareFilenames("i", "I"));
  EXPECT_EQ(-1, CompareFilenames("i", u8"\u0130"));
  EXPECT_EQ(-1, CompareFilenames("i", u8"\u0131"));
  EXPECT_EQ(-1, CompareFilenames("I", u8"\u0130"));
  EXPECT_EQ(-1, CompareFilenames("I", u8"\u0131"));
  EXPECT_EQ(-1, CompareFilenames(u8"\u0130", u8"\u0131"));
  EXPECT_EQ(expectedRhoSymbolOrder, CompareFilenames(u8"\u03f1", u8"\u03a1"));
  EXPECT_EQ(expectedRhoSymbolOrder, CompareFilenames(u8"\u03f1", u8"\u03c1"));
  EXPECT_EQ(0, CompareFilenames(u8"\u03a1", u8"\u03c1"));

  // Reset locale.
  std::locale::global(std::locale::classic());
}

#ifdef _WIN32
TEST(NormalizeFilename, shouldUppercaseStringsAndBeLocaleInvariant) {
  EXPECT_EQ("I", NormalizeFilename("i"));
  EXPECT_EQ("I", NormalizeFilename("I"));
  EXPECT_EQ(u8"\u0130", NormalizeFilename(u8"\u0130"));
  EXPECT_EQ(u8"\u0131", NormalizeFilename(u8"\u0131"));
  EXPECT_EQ(u8"\u03f1", NormalizeFilename(u8"\u03f1"));
  EXPECT_EQ(u8"\u03a1", NormalizeFilename(u8"\u03a1"));
  EXPECT_EQ(u8"\u03a1", NormalizeFilename(u8"\u03c1"));

  // Set locale to Turkish.
  std::locale::global(std::locale("tr-TR"));

  EXPECT_EQ("I", NormalizeFilename("i"));
  EXPECT_EQ("I", NormalizeFilename("I"));
  EXPECT_EQ(u8"\u0130", NormalizeFilename(u8"\u0130"));
  EXPECT_EQ(u8"\u0131", NormalizeFilename(u8"\u0131"));
  EXPECT_EQ(u8"\u03f1", NormalizeFilename(u8"\u03f1"));
  EXPECT_EQ(u8"\u03a1", NormalizeFilename(u8"\u03a1"));
  EXPECT_EQ(u8"\u03a1", NormalizeFilename(u8"\u03c1"));

  // Set locale to Greek.
  std::locale::global(std::locale("el-GR"));

  EXPECT_EQ("I", NormalizeFilename("i"));
  EXPECT_EQ("I", NormalizeFilename("I"));
  EXPECT_EQ(u8"\u0130", NormalizeFilename(u8"\u0130"));
  EXPECT_EQ(u8"\u0131", NormalizeFilename(u8"\u0131"));
  EXPECT_EQ(u8"\u03f1", NormalizeFilename(u8"\u03f1"));
  EXPECT_EQ(u8"\u03a1", NormalizeFilename(u8"\u03a1"));
  EXPECT_EQ(u8"\u03a1", NormalizeFilename(u8"\u03c1"));

  // Reset locale.
  std::locale::global(std::locale::classic());
}
#else
TEST(NormalizeFilename, shouldCaseFoldStringsAndBeLocaleInvariant) {
  // ICU folds all greek rhos to the lowercase rho, unlike Windows. The result
  // for uppercase turkish i is different from Windows but functionally
  // equivalent.
  // A small enough deviation that it should hopefully be insignificant.

  EXPECT_EQ("i", NormalizeFilename("i"));
  EXPECT_EQ("i", NormalizeFilename("I"));
  EXPECT_EQ(u8"i\u0307", NormalizeFilename(u8"\u0130"));
  EXPECT_EQ(u8"\u0131", NormalizeFilename(u8"\u0131"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03f1"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03a1"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03c1"));

  // Set locale to Turkish.
  std::locale::global(std::locale("tr_TR.UTF-8"));

  EXPECT_EQ("i", NormalizeFilename("i"));
  EXPECT_EQ("i", NormalizeFilename("I"));
  EXPECT_EQ(u8"i\u0307", NormalizeFilename(u8"\u0130"));
  EXPECT_EQ(u8"\u0131", NormalizeFilename(u8"\u0131"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03f1"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03a1"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03c1"));

  // Set locale to Greek.
  std::locale::global(std::locale("el_GR.UTF-8"));

  EXPECT_EQ("i", NormalizeFilename("i"));
  EXPECT_EQ("i", NormalizeFilename("I"));
  EXPECT_EQ(u8"i\u0307", NormalizeFilename(u8"\u0130"));
  EXPECT_EQ(u8"\u0131", NormalizeFilename(u8"\u0131"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03f1"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03a1"));
  EXPECT_EQ(u8"\u03c1", NormalizeFilename(u8"\u03c1"));

  // Reset locale.
  std::locale::global(std::locale::classic());
}
#endif
}
}

#endif
