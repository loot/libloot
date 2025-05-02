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

#ifndef LOOT_TESTS_API_INTERFACE_METADATA_MESSAGE_CONTENT_TEST
#define LOOT_TESTS_API_INTERFACE_METADATA_MESSAGE_CONTENT_TEST

#include <gtest/gtest.h>

#include "loot/metadata/message_content.h"

namespace loot::test {
const std::string french = "fr";

TEST(MessageContent, defaultConstructorShouldSetEmptyEnglishLanguageString) {
  MessageContent content;

  EXPECT_TRUE(content.GetText().empty());
  EXPECT_EQ(MessageContent::DEFAULT_LANGUAGE, content.GetLanguage());
}

TEST(MessageContent, contentConstructorShouldStoreGivenStringAndLanguage) {
  MessageContent content("content", french);

  EXPECT_EQ("content", content.GetText());
  EXPECT_EQ(french, content.GetLanguage());
}

TEST(MessageContent,
     equalityShouldRequireCaseSensitiveEqualityOnTextAndLanguage) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_TRUE(content1 == content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_FALSE(content1 == content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_FALSE(content1 == content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_FALSE(content1 == content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_FALSE(content1 == content2);
}

TEST(MessageContent, inequalityShouldBeTheInverseOfEquality) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_FALSE(content1 != content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_TRUE(content1 != content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_TRUE(content1 != content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_TRUE(content1 != content2);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_TRUE(content1 != content2);
}

TEST(
    MessageContent,
    lessThanOperatorShouldUseCaseSensitiveLexicographicalComparisonForTextAndLanguage) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_FALSE(content1 < content2);
  EXPECT_FALSE(content2 < content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_FALSE(content1 < content2);
  EXPECT_TRUE(content2 < content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_TRUE(content2 < content1);
  EXPECT_FALSE(content1 < content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_TRUE(content1 < content2);
  EXPECT_FALSE(content2 < content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_TRUE(content2 < content1);
  EXPECT_FALSE(content1 < content2);
}

TEST(
    MessageContent,
    greaterThanOperatorShouldReturnTrueIfTheSecondMessageContentIsLessThanTheFirst) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_FALSE(content1 > content2);
  EXPECT_FALSE(content2 > content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_TRUE(content1 > content2);
  EXPECT_FALSE(content2 > content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_FALSE(content2 > content1);
  EXPECT_TRUE(content1 > content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_FALSE(content1 > content2);
  EXPECT_TRUE(content2 > content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_FALSE(content2 > content1);
  EXPECT_TRUE(content1 > content2);
}

TEST(
    MessageContent,
    lessThanOrEqualOperatorShouldReturnTrueIfTheFirstMessageContentIsNotGreaterThanTheSecond) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_TRUE(content1 <= content2);
  EXPECT_TRUE(content2 <= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_FALSE(content1 <= content2);
  EXPECT_TRUE(content2 <= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_TRUE(content2 <= content1);
  EXPECT_FALSE(content1 <= content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_TRUE(content1 <= content2);
  EXPECT_FALSE(content2 <= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_TRUE(content2 <= content1);
  EXPECT_FALSE(content1 <= content2);
}

TEST(
    MessageContent,
    greaterThanOrEqualToOperatorShouldReturnTrueIfTheFirstMessageContentIsNotLessThanTheSecond) {
  MessageContent content1("content", "fr");
  MessageContent content2("content", "fr");

  EXPECT_TRUE(content1 >= content2);
  EXPECT_TRUE(content2 >= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("Content", "fr");

  EXPECT_TRUE(content1 >= content2);
  EXPECT_FALSE(content2 >= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "Fr");

  EXPECT_FALSE(content2 >= content1);
  EXPECT_TRUE(content1 >= content2);

  content1 = MessageContent("content1", "fr");
  content2 = MessageContent("content2", "fr");

  EXPECT_FALSE(content1 >= content2);
  EXPECT_TRUE(content2 >= content1);

  content1 = MessageContent("content", "fr");
  content2 = MessageContent("content", "de");

  EXPECT_FALSE(content2 >= content1);
  EXPECT_TRUE(content1 >= content2);
}

TEST(SelectMessageContent, shouldReturnANulloptIfTheVectorIsEmpty) {
  auto content = SelectMessageContent(std::vector<MessageContent>(), "fr");

  EXPECT_FALSE(content.has_value());
}

TEST(SelectMessageContent, shouldReturnTheOnlyElementOfASingleElementVector) {
  MessageContent content("test", "de");
  auto chosen = SelectMessageContent({MessageContent("test", "de")}, "fr");

  EXPECT_EQ(content, chosen);
}

TEST(
    SelectMessageContent,
    shouldReturnAnEmptyEnglishMessageIfTheVectorHasNoEnglishOrMatchingLanguageContentWithTwoOrMoreElements) {
  auto contents = {MessageContent("test1", "de"),
                   MessageContent("test2", "fr")};
  auto content = SelectMessageContent(contents, "pt");

  EXPECT_FALSE(content.has_value());
}

TEST(SelectMessageContent,
     shouldReturnElementWithExactlyMatchingLocaleCodeIfPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt"),
                   MessageContent("test4", "pt_PT"),
                   MessageContent("test5", "pt_BR")};
  auto content = SelectMessageContent(contents, "pt_BR");

  EXPECT_TRUE(content.has_value());
  EXPECT_EQ("pt_BR", content.value().GetLanguage());
  EXPECT_EQ("test5", content.value().GetText());
}

TEST(
    SelectMessageContent,
    shouldReturnElementWithMatchingLanguageCodeIfExactlyMatchingLocaleCodeIsNotPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt_PT"),
                   MessageContent("test4", "pt")};
  auto content = SelectMessageContent(contents, "pt_BR");

  EXPECT_TRUE(content.has_value());
  EXPECT_EQ("pt", content.value().GetLanguage());
  EXPECT_EQ("test4", content.value().GetText());
}

TEST(SelectMessageContent,
     shouldReturnElementWithEnLanguageCodeIfNoMatchingLanguageCodeIsPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt_PT")};
  auto content = SelectMessageContent(contents, "pt_BR");

  EXPECT_TRUE(content.has_value());
  EXPECT_EQ("en", content.value().GetLanguage());
  EXPECT_EQ("test1", content.value().GetText());
}

TEST(SelectMessageContent,
     shouldReturnElementWithExactlyMatchingLanguageCodeIfLanguageCodeIsGiven) {
  auto contents = {
      MessageContent("test1", "en"),
      MessageContent("test2", "de"),
      MessageContent("test3", "pt_BR"),
      MessageContent("test4", "pt"),
  };
  auto content = SelectMessageContent(contents, "pt");

  EXPECT_TRUE(content.has_value());
  EXPECT_EQ("pt", content.value().GetLanguage());
  EXPECT_EQ("test4", content.value().GetText());
}

TEST(
    SelectMessageContent,
    shouldReturnFirstElementWithMatchingLanguageCodeIfLanguageCodeIsGivenAndNoExactMatchIsPresent) {
  auto contents = {MessageContent("test1", "en"),
                   MessageContent("test2", "de"),
                   MessageContent("test3", "pt_PT"),
                   MessageContent("test4", "pt_BR")};
  auto content = SelectMessageContent(contents, "pt");

  EXPECT_TRUE(content.has_value());
  EXPECT_EQ("pt_PT", content.value().GetLanguage());
  EXPECT_EQ("test3", content.value().GetText());
}
}

#endif
