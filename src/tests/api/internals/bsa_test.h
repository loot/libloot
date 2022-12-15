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

#ifndef LOOT_TESTS_API_INTERNALS_BSA_TEST
#define LOOT_TESTS_API_INTERNALS_BSA_TEST

#include <gtest/gtest.h>

#include "api/bsa.h"

namespace loot::test {
TEST(GetAssetsInBethesdaArchive, shouldSupportV103BSAs) {
  const auto path = std::filesystem::u8path("./Oblivion/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0, assets.begin()->first);
  EXPECT_EQ(1, assets.at(0).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0).begin());
}

TEST(GetAssetsInBethesdaArchive, shouldSupportV104BSAs) {
  const auto path = std::filesystem::u8path("./Skyrim/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0x2E01002E, assets.begin()->first);
  EXPECT_EQ(1, assets.at(0x2E01002E).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0x2E01002E).begin());
}

TEST(GetAssetsInBethesdaArchive, shouldSupportV105BSAs) {
  const auto path = std::filesystem::u8path("./SkyrimSE/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0xB68102C964176E73, assets.begin()->first);
  EXPECT_EQ(1, assets.at(0xB68102C964176E73).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0xB68102C964176E73).begin());
}

TEST(GetAssetsInBethesdaArchive, shouldThrowIfFileCannotBeOpened) {
  const auto path = std::filesystem::u8path("invalid.bsa");

  EXPECT_THROW(GetAssetsInBethesdaArchive(path), std::runtime_error);
}

TEST(GetAssetsInBethesdaArchives, shouldSkipFilesThatCannotBeRead) {
  std::vector<std::filesystem::path> paths(
      {std::filesystem::u8path("invalid.bsa"),
       std::filesystem::u8path("./Skyrim/Data/Blank.bsa")});

  const auto assets = GetAssetsInBethesdaArchives(paths);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(1, assets.size());
  EXPECT_EQ(1, filesCount);
  EXPECT_EQ(0x2E01002E, assets.begin()->first);
  EXPECT_EQ(1, assets.begin()->second.size());
  EXPECT_EQ(0x4670B6836C077365, *assets.begin()->second.begin());
}

TEST(GetAssetsInBethesdaArchives, shouldCombineAssetsFromEachLoadedArchive) {
  std::vector<std::filesystem::path> paths(
      {std::filesystem::u8path("./Oblivion/Data/Blank.bsa"),
       std::filesystem::u8path("./Skyrim/Data/Blank.bsa"),
       std::filesystem::u8path("./SkyrimSE/Data/Blank.bsa")});

  const auto assets = GetAssetsInBethesdaArchives(paths);

  size_t filesCount = 0;
  for (const auto& folder : assets) {
    filesCount += folder.second.size();
  }

  EXPECT_EQ(3, assets.size());
  EXPECT_EQ(3, filesCount);

  EXPECT_EQ(1, assets.at(0).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0).begin());

  EXPECT_EQ(1, assets.at(0x2E01002E).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0x2E01002E).begin());

  EXPECT_EQ(1, assets.at(0xB68102C964176E73).size());
  EXPECT_EQ(0x4670B6836C077365, *assets.at(0xB68102C964176E73).begin());
}

TEST(DoAssetsIntersect, shouldReturnTrueIfTheSameFileExistsInTheSameFolder) {
  const auto path = std::filesystem::u8path("./Oblivion/Data/Blank.bsa");

  const auto assets = GetAssetsInBethesdaArchive(path);

  EXPECT_TRUE(DoAssetsIntersect(assets, assets));
}

TEST(DoAssetsIntersect,
     shouldReturnFalseIfTheSameFileExistsInDifferentFolders) {
  const auto path1 = std::filesystem::u8path("./Oblivion/Data/Blank.bsa");
  const auto assets1 = GetAssetsInBethesdaArchive(path1);

  const auto path2 = std::filesystem::u8path("./Skyrim/Data/Blank.bsa");
  const auto assets2 = GetAssetsInBethesdaArchive(path2);

  EXPECT_EQ(*assets2.at(0x2E01002E).begin(), *assets1.at(0).begin());

  EXPECT_FALSE(DoAssetsIntersect(assets1, assets2));
}
}

#endif
