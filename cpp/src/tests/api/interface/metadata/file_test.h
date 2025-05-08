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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_FILE_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_FILE_TEST

#include <gtest/gtest.h>

#include "loot/metadata/file.h"

namespace loot::test {
TEST(File, defaultConstructorShouldInitialiseEmptyStrings) {
  File file;

  EXPECT_EQ("", std::string(file.GetName()));
  EXPECT_EQ("", file.GetDisplayName());
  EXPECT_EQ("", file.GetCondition());
  EXPECT_EQ("", file.GetConstraint());
}

TEST(File, stringsConstructorShouldStoreGivenStrings) {
  std::vector<MessageContent> detail = {MessageContent("text", "en")};
  File file("name", "display", "condition", detail, "constraint");

  EXPECT_EQ("name", std::string(file.GetName()));
  EXPECT_EQ("display", file.GetDisplayName());
  EXPECT_EQ("condition", file.GetCondition());
  EXPECT_EQ(detail, file.GetDetail());
  EXPECT_EQ("constraint", file.GetConstraint());
}

TEST(File, getDisplayNameShouldReturnDisplayString) {
  File file("name", "display");

  EXPECT_EQ("display", file.GetDisplayName());
}
}

#endif
