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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_LOCATION_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_LOCATION_TEST

#include <gtest/gtest.h>

#include "loot/metadata/location.h"

namespace loot::test {
TEST(Location, defaultConstructorShouldInitialiseEmptyStrings) {
  Location location;

  EXPECT_EQ("", location.GetURL());
  EXPECT_EQ("", location.GetName());
}

TEST(Location, stringsConstructorShouldStoreGivenStrings) {
  Location location("http://www.example.com", "example");

  EXPECT_EQ("http://www.example.com", location.GetURL());
  EXPECT_EQ("example", location.GetName());
}
}

#endif
