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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_CONDITION_EVALUATOR_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_CONDITION_EVALUATOR_TEST

#include <stdexcept>

#include "api/metadata/condition_evaluator.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class ConditionEvaluatorTest : public CommonGameTestFixture,
                               public testing::WithParamInterface<GameType> {
protected:
  ConditionEvaluatorTest() :
      CommonGameTestFixture(GetParam()),
      info_(std::vector<MessageContent>({
          MessageContent("info"),
      })),
      nonAsciiEsm(u8"non\u00C1scii.esm"),
      nonAsciiNestedFile(u8"non\u00C1scii/test.txt"),
      game_(GetParam(), gamePath, localPath),
      evaluator_(game_.GetType(), game_.DataPath()) {
    // Make sure the plugin with a non-ASCII filename exists.
    std::filesystem::copy_file(dataPath / blankEsm,
                               dataPath / std::filesystem::u8path(nonAsciiEsm));

    touch(dataPath / std::filesystem::u8path(nonAsciiNestedFile));

    loadInstalledPlugins();
    evaluator_.RefreshLoadedPluginsState(game_.GetLoadedPlugins());
    evaluator_.RefreshActivePluginsState(
        game_.GetLoadOrderHandler().GetActivePlugins());
  }

  std::string IntToHexString(const uint32_t value) {
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
  }

  void loadInstalledPlugins() {
    auto plugins = GetInstalledPlugins();

    plugins.push_back(std::filesystem::u8path(nonAsciiEsm));

    game_.LoadCurrentLoadOrderState();
    game_.LoadPlugins(plugins, true);
  }

  const std::vector<MessageContent> info_;
  const std::string nonAsciiEsm;
  const std::string nonAsciiNestedFile;

  Game game_;
  ConditionEvaluator evaluator_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                         ConditionEvaluatorTest,
                         ::testing::ValuesIn(ALL_GAME_TYPES));

TEST_P(ConditionEvaluatorTest,
       evaluateShouldReturnTrueForAnEmptyConditionString) {
  EXPECT_TRUE(evaluator_.Evaluate(""));
}

TEST_P(ConditionEvaluatorTest, evaluateShouldThrowForAnInvalidConditionString) {
  EXPECT_THROW(evaluator_.Evaluate("condition"), std::runtime_error);
}

TEST_P(ConditionEvaluatorTest,
       evaluateShouldReturnTrueForAConditionThatIsTrue) {
  EXPECT_TRUE(evaluator_.Evaluate("file(\"" + blankEsm + "\")"));
}

TEST_P(ConditionEvaluatorTest, evaluateShouldUseAllGivenDataPaths) {
  ASSERT_FALSE(
      evaluator_.Evaluate("file(\"" + localPath.filename().u8string() + "\")"));

  evaluator_.ClearConditionCache();
  evaluator_.SetAdditionalDataPaths({localPath.parent_path()});

  EXPECT_TRUE(
      evaluator_.Evaluate("file(\"" + localPath.filename().u8string() + "\")"));
}

TEST_P(ConditionEvaluatorTest,
       evaluateFileConditionShouldReturnTrueForANonAsciiFileThatExists) {
  EXPECT_TRUE(evaluator_.Evaluate("file(\"" + nonAsciiEsm + "\")"));
}

TEST_P(ConditionEvaluatorTest,
       evaluateChecksumConditionShouldBeAbleToGetTheCrcOfANonAsciiFile) {
  std::string condition("checksum(\"" + nonAsciiEsm + "\", " +
                        IntToHexString(blankEsmCrc) + ")");
  EXPECT_TRUE(evaluator_.Evaluate(condition));
}

TEST_P(ConditionEvaluatorTest,
       evaluateVersionConditionShouldBeAbleToGetTheVersionOfANonAsciiFile) {
  std::string condition("version(\"" + nonAsciiEsm + "\", \"5.0\", ==)");
  EXPECT_TRUE(evaluator_.Evaluate(condition));
}

TEST_P(ConditionEvaluatorTest,
       evaluateActiveConditionShouldReturnTrueForAnActivePlugin) {
  std::string condition("active(\"" + blankEsm + "\")");
  EXPECT_TRUE(evaluator_.Evaluate(condition));
}

TEST_P(ConditionEvaluatorTest,
       evaluateRegexFileConditionShouldReturnTrueForANonAsciiFileThatExists) {
  std::string condition(u8"file(\"non\u00C1scii.*\\.esm\")");
  EXPECT_TRUE(evaluator_.Evaluate(condition));
}

TEST_P(
    ConditionEvaluatorTest,
    evaluateRegexFileConditionShouldReturnTrueForANonAsciiNestedFileThatExists) {
  std::string condition(u8"file(\"non\u00C1scii/.+\\.txt\")");
  EXPECT_TRUE(evaluator_.Evaluate(condition));
}

TEST_P(ConditionEvaluatorTest,
       evaluateShouldReturnFalseForAConditionThatIsFalse) {
  EXPECT_FALSE(evaluator_.Evaluate("file(\"" + missingEsp + "\")"));
}

TEST_P(ConditionEvaluatorTest, evaluateAllShouldEvaluateAllMetadataConditions) {
  PluginMetadata plugin(nonAsciiEsm);
  plugin.SetGroup("group1");

  File file1(blankEsp);
  File file2(blankDifferentEsm, "", "file(\"" + missingEsp + "\")");
  plugin.SetLoadAfterFiles({file1, file2});
  plugin.SetRequirements({file1, file2});
  plugin.SetIncompatibilities({file1, file2});

  Message message1(MessageType::say, "content");
  Message message2(MessageType::say, "content", "file(\"" + missingEsp + "\")");
  plugin.SetMessages({message1, message2});

  Tag tag1("Relev");
  Tag tag2("Relev", true, "file(\"" + missingEsp + "\")");
  plugin.SetTags({tag1, tag2});

  PluginCleaningData info1(blankEsmCrc, "utility", info_, 1, 2, 3);
  PluginCleaningData info2(0xDEADBEEF, "utility", info_, 1, 2, 3);
  plugin.SetDirtyInfo({info1, info2});
  plugin.SetCleanInfo({info1, info2});

  EXPECT_NO_THROW(plugin = evaluator_.EvaluateAll(plugin).value());

  std::vector<File> expectedFiles({file1});
  EXPECT_EQ("group1", plugin.GetGroup().value());
  EXPECT_EQ(expectedFiles, plugin.GetLoadAfterFiles());
  EXPECT_EQ(expectedFiles, plugin.GetRequirements());
  EXPECT_EQ(expectedFiles, plugin.GetIncompatibilities());
  EXPECT_EQ(std::vector<Message>({message1}), plugin.GetMessages());
  EXPECT_EQ(std::vector<Tag>({tag1}), plugin.GetTags());
  EXPECT_EQ(std::vector<PluginCleaningData>({info1}), plugin.GetDirtyInfo());
  EXPECT_EQ(std::vector<PluginCleaningData>({info1}), plugin.GetCleanInfo());
}

TEST_P(ConditionEvaluatorTest, evaluateAllShouldPreserveGroupExplicitness) {
  PluginMetadata plugin(blankEsm);

  EXPECT_FALSE(evaluator_.EvaluateAll(plugin).has_value());
}

TEST_P(ConditionEvaluatorTest,
       refreshActivePluginsStateShouldClearTheConditionCache) {
  std::string condition("active(\"" + blankEsm + "\")");
  ASSERT_TRUE(evaluator_.Evaluate(condition));

  evaluator_.RefreshActivePluginsState({blankEsp});

  EXPECT_FALSE(evaluator_.Evaluate(condition));
}

TEST_P(
    ConditionEvaluatorTest,
    refreshActivePluginsStateShouldClearTheActivePluginsCacheIfGivenAnEmptyVector) {
  std::string condition("active(\"" + blankEsm + "\")");
  ASSERT_TRUE(evaluator_.Evaluate(condition));

  evaluator_.RefreshActivePluginsState({});

  EXPECT_FALSE(evaluator_.Evaluate(condition));
}

TEST_P(ConditionEvaluatorTest,
       refreshLoadedPluginsStateShouldClearTheConditionCache) {
  std::string condition("version(\"" + blankEsm + "\", \"5.0\", ==)");

  ASSERT_TRUE(evaluator_.Evaluate(condition));

  auto plugins = game_.GetLoadedPlugins();
  auto pluginsIt =
      std::find_if(plugins.cbegin(), plugins.cend(), [&](auto plugin) {
        return plugin->GetName() == blankEsm;
      });
  plugins.erase(pluginsIt);
  evaluator_.RefreshLoadedPluginsState(plugins);

  EXPECT_FALSE(evaluator_.Evaluate(condition));
}

TEST_P(
    ConditionEvaluatorTest,
    refreshLoadedPluginsStateShouldClearTheVersionsCacheIfGivenAnEmptyVector) {
  std::string condition("version(\"" + blankEsm + "\", \"5.0\", ==)");

  ASSERT_TRUE(evaluator_.Evaluate(condition));

  evaluator_.RefreshLoadedPluginsState({});

  EXPECT_FALSE(evaluator_.Evaluate(condition));
}

TEST_P(ConditionEvaluatorTest,
       setAdditionalDataPathsShouldAcceptAnEmptyVector) {
  EXPECT_NO_THROW(evaluator_.SetAdditionalDataPaths({}));
}

TEST_P(ConditionEvaluatorTest,
       setAdditionalDataPathsShouldAcceptANonEmptyVector) {
  EXPECT_NO_THROW(evaluator_.SetAdditionalDataPaths(
      {std::filesystem::u8path("a"), std::filesystem::u8path("b")}));
}
}
}

#endif
