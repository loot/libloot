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

TEST(Location, equalityShouldBeCaseSensitiveOnUrlAndName) {
  Location location1("http://www.example.com", "example");
  Location location2("http://www.example.com", "example");

  EXPECT_TRUE(location1 == location2);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("HTTP://WWW.EXAMPLE.COM", "example");

  EXPECT_FALSE(location1 == location2);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("http://www.example.com", "Example");

  EXPECT_FALSE(location1 == location2);

  location1 = Location("http://www.example1.com", "example");
  location2 = Location("http://www.example2.com", "example");

  EXPECT_FALSE(location1 == location2);

  location1 = Location("http://www.example.com", "example1");
  location2 = Location("http://www.example.com", "example2");

  EXPECT_FALSE(location1 == location2);
}

TEST(Location, inequalityShouldBeTheInverseOfEquality) {
  Location location1("http://www.example.com", "example");
  Location location2("http://www.example.com", "example");

  EXPECT_FALSE(location1 != location2);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("HTTP://WWW.EXAMPLE.COM", "example");

  EXPECT_TRUE(location1 != location2);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("http://www.example.com", "Example");

  EXPECT_TRUE(location1 != location2);

  location1 = Location("http://www.example1.com", "example");
  location2 = Location("http://www.example2.com", "example");

  EXPECT_TRUE(location1 != location2);

  location1 = Location("http://www.example.com", "example1");
  location2 = Location("http://www.example.com", "example2");

  EXPECT_TRUE(location1 != location2);
}

TEST(
    Location,
    lessThanOperatorShouldUseCaseSensitiveLexicographicalComparisonForNameAndUrl) {
  Location location1("http://www.example.com", "example");
  Location location2("http://www.example.com", "example");

  EXPECT_FALSE(location1 < location2);
  EXPECT_FALSE(location2 < location1);

  location1 = Location("http://www.example.com");
  location2 = Location("HTTP://WWW.EXAMPLE.COM");

  EXPECT_FALSE(location1 < location2);
  EXPECT_TRUE(location2 < location1);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("http://www.example.com", "Example");

  EXPECT_FALSE(location1 < location2);
  EXPECT_TRUE(location2 < location1);

  location1 = Location("http://www.example1.com");
  location2 = Location("http://www.example2.com");

  EXPECT_TRUE(location1 < location2);
  EXPECT_FALSE(location2 < location1);

  location1 = Location("http://www.example.com", "example1");
  location2 = Location("http://www.example.com", "example2");

  EXPECT_FALSE(location2 < location1);
  EXPECT_TRUE(location1 < location2);
}

TEST(Location,
     greaterThanOperatorShouldReturnTrueIfTheSecondLocationIsLessThanTheFirst) {
  Location location1("http://www.example.com", "example");
  Location location2("http://www.example.com", "example");

  EXPECT_FALSE(location1 > location2);
  EXPECT_FALSE(location2 > location1);

  location1 = Location("http://www.example.com");
  location2 = Location("HTTP://WWW.EXAMPLE.COM");

  EXPECT_TRUE(location1 > location2);
  EXPECT_FALSE(location2 > location1);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("http://www.example.com", "Example");

  EXPECT_TRUE(location1 > location2);
  EXPECT_FALSE(location2 > location1);

  location1 = Location("http://www.example1.com");
  location2 = Location("http://www.example2.com");

  EXPECT_FALSE(location1 > location2);
  EXPECT_TRUE(location2 > location1);

  location1 = Location("http://www.example.com", "example1");
  location2 = Location("http://www.example.com", "example2");

  EXPECT_TRUE(location2 > location1);
  EXPECT_FALSE(location1 > location2);
}

TEST(
    Location,
    lessThanOrEqualOperatorShouldReturnTrueIfTheFirstLocationIsNotGreaterThanTheSecond) {
  Location location1("http://www.example.com", "example");
  Location location2("http://www.example.com", "example");

  EXPECT_TRUE(location1 <= location2);
  EXPECT_TRUE(location2 <= location1);

  location1 = Location("http://www.example.com");
  location2 = Location("HTTP://WWW.EXAMPLE.COM");

  EXPECT_FALSE(location1 <= location2);
  EXPECT_TRUE(location2 <= location1);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("http://www.example.com", "Example");

  EXPECT_FALSE(location1 <= location2);
  EXPECT_TRUE(location2 <= location1);

  location1 = Location("http://www.example1.com");
  location2 = Location("http://www.example2.com");

  EXPECT_TRUE(location1 <= location2);
  EXPECT_FALSE(location2 <= location1);

  location1 = Location("http://www.example.com", "example1");
  location2 = Location("http://www.example.com", "example2");

  EXPECT_FALSE(location2 <= location1);
  EXPECT_TRUE(location1 <= location2);
}

TEST(
    Location,
    greaterThanOrEqualToOperatorShouldReturnTrueIfTheFirstLocationIsNotLessThanTheSecond) {
  Location location1("http://www.example.com", "example");
  Location location2("http://www.example.com", "example");

  EXPECT_TRUE(location1 >= location2);
  EXPECT_TRUE(location2 >= location1);

  location1 = Location("http://www.example.com");
  location2 = Location("HTTP://WWW.EXAMPLE.COM");

  EXPECT_TRUE(location1 >= location2);
  EXPECT_FALSE(location2 >= location1);

  location1 = Location("http://www.example.com", "example");
  location2 = Location("http://www.example.com", "Example");

  EXPECT_TRUE(location1 >= location2);
  EXPECT_FALSE(location2 >= location1);

  location1 = Location("http://www.example1.com");
  location2 = Location("http://www.example2.com");

  EXPECT_FALSE(location1 >= location2);
  EXPECT_TRUE(location2 >= location1);

  location1 = Location("http://www.example.com", "example1");
  location2 = Location("http://www.example.com", "example2");

  EXPECT_TRUE(location2 >= location1);
  EXPECT_FALSE(location1 >= location2);
}
}

#endif
