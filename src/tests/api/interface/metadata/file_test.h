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

TEST(File, equalityShouldBeCaseInsensitiveOnName) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_TRUE(file1 == file2);

  file1 = File("name", "display", "condition");
  file2 = File("Name", "display", "condition");

  EXPECT_TRUE(file1 == file2);

  file1 = File("name1", "display", "condition");
  file2 = File("name2", "display", "condition");

  EXPECT_FALSE(file1 == file2);
}

TEST(File, equalityShouldBeCaseSensitiveOnDisplayAndConditionAndConstraint) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_TRUE(file1 == file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display", "condition", {}, "constraint");
  file2 = File("name", "display", "condition", {}, "Constraint");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display1", "condition");
  file2 = File("name", "display2", "condition");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_FALSE(file1 == file2);

  file1 = File("name", "display", "condition", {}, "constraint1");
  file2 = File("name", "display", "condition", {}, "constraint2");

  EXPECT_FALSE(file1 == file2);
}

TEST(File, equalityShouldCompareTheDetailVectors) {
  File file1("", "", "", {MessageContent("text", "en")});
  File file2("", "", "", {MessageContent("text", "en")});

  EXPECT_TRUE(file1 == file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_FALSE(file1 == file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "En")});

  EXPECT_FALSE(file1 == file2);

  file1 = File(
      "", "", "", {MessageContent("text", "en"), MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 == file2);
}

TEST(File, inequalityShouldBeTheInverseOfEquality) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 != file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display", "condition", {}, "constraint");
  file2 = File("name", "display", "condition", {}, "Constraint");

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display1", "condition");
  file2 = File("name", "display2", "condition");

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_TRUE(file1 != file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_TRUE(file1 != file2);

  file1 = File("name", "display", "condition", {}, "constraint1");
  file2 = File("name", "display", "condition", {}, "constraint2");

  EXPECT_TRUE(file1 != file2);
}

TEST(File,
     lessThanOperatorShouldUseCaseInsensitiveLexicographicalComparisonForName) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_FALSE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name", "display", "condition");
  file2 = File("Name", "display", "condition");

  EXPECT_FALSE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name1");
  file2 = File("name2");

  EXPECT_TRUE(file1 < file2);
  EXPECT_FALSE(file2 < file1);
}

TEST(
    File,
    lessThanOperatorShouldUseCaseSensitiveLexicographicalComparisonForDisplayAndConditionAndConstraint) {
  File file1("name", "display", "condition");
  File file2("name", "display", "condition");

  EXPECT_FALSE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_TRUE(file2 < file1);
  EXPECT_FALSE(file1 < file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_TRUE(file2 < file1);
  EXPECT_FALSE(file1 < file2);

  file1 = File("name", "display", "condition", {}, "constraint");
  file2 = File("name", "display", "condition", {}, "Constraint");

  EXPECT_TRUE(file2 < file1);
  EXPECT_FALSE(file1 < file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_TRUE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_TRUE(file1 < file2);
  EXPECT_FALSE(file2 < file1);

  file1 = File("name", "display", "condition", {}, "constraint1");
  file2 = File("name", "display", "condition", {}, "constraint2");

  EXPECT_TRUE(file1 < file2);
  EXPECT_FALSE(file2 < file1);
}

TEST(File, lessThanOperatorShouldCompareTheDetailVectors) {
  File file1("", "", "", {MessageContent("text", "en")});
  File file2("", "", "", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 < file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_FALSE(file1 < file2);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "En")});

  EXPECT_FALSE(file1 < file2);

  file1 = File(
      "", "", "", {MessageContent("text", "en"), MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 < file2);
}

TEST(File, shouldAllowComparisonUsingGreaterThanOperator) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_FALSE(file1 > file2);
  EXPECT_FALSE(file2 > file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_FALSE(file2 > file1);
  EXPECT_TRUE(file1 > file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_FALSE(file2 > file1);
  EXPECT_TRUE(file1 > file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_FALSE(file1 > file2);
  EXPECT_TRUE(file2 > file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_FALSE(file1 > file2);
  EXPECT_TRUE(file2 > file1);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_TRUE(file1 > file2);
}

TEST(
    File,
    lessThanOrEqualToOperatorShouldReturnTrueIfFirstFileIsNotGreaterThanSecondFile) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_TRUE(file1 <= file2);
  EXPECT_TRUE(file2 <= file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_TRUE(file2 <= file1);
  EXPECT_FALSE(file1 <= file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_TRUE(file2 <= file1);
  EXPECT_FALSE(file1 <= file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_TRUE(file1 <= file2);
  EXPECT_FALSE(file2 <= file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_TRUE(file1 <= file2);
  EXPECT_FALSE(file2 <= file1);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_FALSE(file1 <= file2);
}

TEST(
    File,
    greaterThanOrEqualToOperatorShouldReturnTrueIfFirstFileIsNotLessThanSecondFile) {
  File file1("name", "display", "condition", {MessageContent("text", "en")});
  File file2("name", "display", "condition", {MessageContent("text", "en")});

  EXPECT_TRUE(file1 >= file2);
  EXPECT_TRUE(file2 >= file1);

  file1 = File("name", "display", "condition");
  file2 = File("name", "Display", "condition");

  EXPECT_FALSE(file2 >= file1);
  EXPECT_TRUE(file1 >= file2);

  file1 = File("name", "display", "condition");
  file2 = File("name", "display", "Condition");

  EXPECT_FALSE(file2 >= file1);
  EXPECT_TRUE(file1 >= file2);

  file1 = File("name", "display1");
  file2 = File("name", "display2");

  EXPECT_FALSE(file1 >= file2);
  EXPECT_TRUE(file2 >= file1);

  file1 = File("name", "display", "condition1");
  file2 = File("name", "display", "condition2");

  EXPECT_FALSE(file1 >= file2);
  EXPECT_TRUE(file2 >= file1);

  file1 = File("", "", "", {MessageContent("text", "en")});
  file2 = File("", "", "", {MessageContent("Text", "en")});

  EXPECT_TRUE(file1 >= file2);
}

TEST(File, getDisplayNameShouldReturnDisplayString) {
  File file("name", "display");

  EXPECT_EQ("display", file.GetDisplayName());
}
}

#endif
