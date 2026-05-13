/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2026 Oliver Hamlet

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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_FILENAME_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_FILENAME_TEST

#include <gtest/gtest.h>

#include "loot/metadata/filename.h"

namespace loot::test {
TEST(Filename, defaultConstructorShouldInitialiseEmptyString) {
  Filename filename;

  EXPECT_EQ("", std::string(filename));
}

TEST(Filename, stringConstructorShouldStoreGivenString) {
  Filename filename("name");

  EXPECT_EQ("name", std::string(filename));
}

TEST(Filename, equalityShouldBeCaseInsensitive) {
  Filename filename1("name");
  Filename filename2("name");

  EXPECT_TRUE(filename1 == filename2);

  filename1 = Filename("name");
  filename2 = Filename("Name");

  EXPECT_TRUE(filename1 == filename2);

  filename1 = Filename("name1");
  filename2 = Filename("name2");

  EXPECT_FALSE(filename1 == filename2);
}

TEST(Filename, inequalityShouldBeTheInverseOfEquality) {
  Filename filename1("name");
  Filename filename2("name");

  EXPECT_FALSE(filename1 != filename2);

  filename1 = Filename("name");
  filename2 = Filename("Name");

  EXPECT_FALSE(filename1 != filename2);

  filename1 = Filename("name1");
  filename2 = Filename("name2");

  EXPECT_TRUE(filename1 != filename2);
}

TEST(Filename,
     lessThanOperatorShouldUseCaseInsensitiveLexicographicalComparison) {
  Filename filename1("name");
  Filename filename2("name");

  EXPECT_FALSE(filename1 < filename2);
  EXPECT_FALSE(filename2 < filename1);

  filename1 = Filename("name");
  filename2 = Filename("Name");

  EXPECT_FALSE(filename1 < filename2);
  EXPECT_FALSE(filename2 < filename1);

  filename1 = Filename("name1");
  filename2 = Filename("name2");

  EXPECT_TRUE(filename1 < filename2);
  EXPECT_FALSE(filename2 < filename1);
}

TEST(Filename, shouldAllowComparisonUsingGreaterThanOperator) {
  Filename filename1("name");
  Filename filename2("name");

  EXPECT_FALSE(filename1 > filename2);
  EXPECT_FALSE(filename2 > filename1);

  filename1 = Filename("name1");
  filename2 = Filename("name2");

  EXPECT_FALSE(filename1 > filename2);
  EXPECT_TRUE(filename2 > filename1);
}

TEST(
    Filename,
    lessThanOrEqualToOperatorShouldReturnTrueIfFirstFilenameIsNotGreaterThanSecondFilename) {
  Filename filename1("name");
  Filename filename2("name");

  EXPECT_TRUE(filename1 <= filename2);
  EXPECT_TRUE(filename2 <= filename1);

  filename1 = Filename("name1");
  filename2 = Filename("name2");

  EXPECT_TRUE(filename1 <= filename2);
  EXPECT_FALSE(filename2 <= filename1);
}

TEST(
    Filename,
    greaterThanOrEqualToOperatorShouldReturnTrueIfFirstFilenameIsNotLessThanSecondFilename) {
  Filename filename1("name");
  Filename filename2("name");

  EXPECT_TRUE(filename1 >= filename2);
  EXPECT_TRUE(filename2 >= filename1);

  filename1 = Filename("name1");
  filename2 = Filename("name2");

  EXPECT_FALSE(filename1 >= filename2);
  EXPECT_TRUE(filename2 >= filename1);
}
}

#endif
