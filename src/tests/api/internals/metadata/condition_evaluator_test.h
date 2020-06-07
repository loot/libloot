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

#include "api/metadata/condition_evaluator.h"

#include "loot/exception/condition_syntax_error.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class ConditionEvaluatorTest : public CommonGameTestFixture {
protected:
  ConditionEvaluatorTest() :
      info_(std::vector<MessageContent>({
          MessageContent("info"),
      })),
      game_(GetParam(), dataPath.parent_path(), localPath),
      evaluator_(game_.Type(),
                 game_.DataPath()),
      nonAsciiEsm(u8"non\u00C1scii.esm"),
      nonAsciiNestedFile(u8"non\u00C1scii/test.txt") {
    // Make sure the plugin with a non-ASCII filename exists.
    std::filesystem::copy_file(dataPath / blankEsm,
      dataPath / std::filesystem::u8path(nonAsciiEsm));

    auto nonAsciiPath = dataPath / std::filesystem::u8path(nonAsciiNestedFile);
    std::filesystem::create_directory(nonAsciiPath.parent_path());

    std::ofstream out(nonAsciiPath);
    out.close();

    loadInstalledPlugins();
    evaluator_.RefreshState(game_.GetCache());
    evaluator_.RefreshState(game_.GetLoadOrderHandler());
  }

  std::string IntToHexString(const uint32_t value) {
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
  }

  void loadInstalledPlugins() {
    std::vector<std::string> plugins({
        masterFile,
        blankEsm,
        blankDifferentEsm,
        blankMasterDependentEsm,
        blankDifferentMasterDependentEsm,
        blankEsp,
        blankDifferentEsp,
        blankMasterDependentEsp,
        blankDifferentMasterDependentEsp,
        blankPluginDependentEsp,
        blankDifferentPluginDependentEsp,
        nonAsciiEsm,
      });

    if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
      plugins.push_back(blankEsl);
    }

    game_.IdentifyMainMasterFile(masterFile);
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
INSTANTIATE_TEST_CASE_P(,
                        ConditionEvaluatorTest,
                        ::testing::Values(GameType::tes3,
                                          GameType::tes4,
                                          GameType::tes5,
                                          GameType::fo3,
                                          GameType::fonv,
                                          GameType::fo4,
                                          GameType::tes5se));

TEST_P(ConditionEvaluatorTest,
       evaluateShouldReturnTrueForAnEmptyConditionString) {
  EXPECT_TRUE(evaluator_.Evaluate(""));
}

TEST_P(ConditionEvaluatorTest, evaluateShouldThrowForAnInvalidConditionString) {
  EXPECT_THROW(evaluator_.Evaluate("condition"), ConditionSyntaxError);
}

TEST_P(ConditionEvaluatorTest,
       evaluateShouldReturnTrueForAConditionThatIsTrue) {
  EXPECT_TRUE(evaluator_.Evaluate("file(\"" + blankEsm + "\")"));
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

TEST_P(ConditionEvaluatorTest,
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

  EXPECT_NO_THROW(plugin = evaluator_.EvaluateAll(plugin));

  std::vector<File> expectedFiles({file1});
  std::set<File> expectedFileSet({file1});
  EXPECT_EQ("group1", plugin.GetGroup().value());
  EXPECT_EQ(expectedFiles, plugin.GetLoadAfterFiles());
  EXPECT_EQ(expectedFiles, plugin.GetRequirements());
  EXPECT_EQ(expectedFileSet, plugin.GetIncompatibilities());
  EXPECT_EQ(std::vector<Message>({message1}), plugin.GetMessages());
  EXPECT_EQ(std::set<Tag>({tag1}), plugin.GetTags());
  EXPECT_EQ(std::set<PluginCleaningData>({info1}), plugin.GetDirtyInfo());
  EXPECT_EQ(std::set<PluginCleaningData>({info1}), plugin.GetCleanInfo());
}

TEST_P(ConditionEvaluatorTest, evaluateAllShouldPreserveGroupExplicitness) {
  PluginMetadata plugin(blankEsm);

  EXPECT_NO_THROW(plugin = evaluator_.EvaluateAll(plugin));
  EXPECT_FALSE(plugin.GetGroup());
}
}
}

#endif
