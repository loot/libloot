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

#ifndef LOOT_TESTS_API_INTERFACE_IS_COMPATIBLE_TEST
#define LOOT_TESTS_API_INTERFACE_IS_COMPATIBLE_TEST

#include <gtest/gtest.h>

#include "loot/api.h"

namespace loot {
namespace test {
TEST(IsCompatible,
     shouldReturnTrueWithEqualMajorAndMinorVersionsAndUnequalPatchVersion) {
  EXPECT_TRUE(IsCompatible(
      LIBLOOT_VERSION_MAJOR, LIBLOOT_VERSION_MINOR, LIBLOOT_VERSION_PATCH));
}

TEST(IsCompatible,
     shouldReturnFalseWithEqualMajorVersionAndUnequalMinorAndPatchVersions) {
  EXPECT_FALSE(IsCompatible(LIBLOOT_VERSION_MAJOR,
                            LIBLOOT_VERSION_MINOR + 1,
                            LIBLOOT_VERSION_PATCH + 1));
}

TEST(GetLiblootRevision, shouldReturnANonEmptyString) {
  EXPECT_FALSE(GetLiblootRevision().empty());
}

TEST(GetVersion, shouldConcatenateMajorMinorAndPatchVersionNumbersWithPeriods) {
  auto expectedVersion = std::to_string(LIBLOOT_VERSION_MAJOR) + "." +
                         std::to_string(LIBLOOT_VERSION_MINOR) + "." +
                         std::to_string(LIBLOOT_VERSION_PATCH);

  EXPECT_EQ(expectedVersion, GetLiblootVersion());
}
}
}

#endif
