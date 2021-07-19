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

#ifndef LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORTER_TEST
#define LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_SORTER_TEST

#include "api/sorting/plugin_graph.h"

#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginGraphTest : public CommonGameTestFixture {
protected:
  PluginGraphTest() :
      game_(GetParam(), dataPath.parent_path(), localPath),
      masterlistPath_(metadataFilesPath / "userlist.yaml"),
      cccPath_(dataPath.parent_path() / getCCCFilename()),
      blankEslEsp("Blank.esl.esp") {}

  void loadInstalledPlugins(Game &game_, bool headersOnly) {
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
    });

    if (GetParam() == GameType::fo4 || GetParam() == GameType::tes5se) {
      plugins.push_back(blankEsl);

      if (std::filesystem::exists(dataPath / blankEslEsp)) {
        plugins.push_back(blankEslEsp);
      }
    }

    game_.IdentifyMainMasterFile(masterFile);
    game_.LoadCurrentLoadOrderState();
    game_.LoadPlugins(plugins, headersOnly);
  }

  void GenerateMasterlist() {
    using std::endl;

    std::ofstream masterlist(masterlistPath_);
    masterlist << "groups:" << endl
               << "  - name: earliest" << endl
               << "  - name: earlier" << endl
               << "    after:" << endl
               << "      - earliest" << endl
               << "  - name: default" << endl
               << "    after:" << endl
               << "      - earlier" << endl
               << "  - name: group1" << endl
               << "  - name: group2" << endl
               << "    after:" << endl
               << "      - group1" << endl
               << "  - name: group3" << endl
               << "    after:" << endl
               << "      - group2" << endl
               << "  - name: group4" << endl
               << "    after:" << endl
               << "      - default" << endl;

    masterlist.close();
  }

  std::string getCCCFilename() {
    if (GetParam() == GameType::fo4) {
      return "Fallout4.ccc";
    } else {
      // Not every game has a .ccc file, but Skyrim SE does, so just assume
      // that.
      return "Skyrim.ccc";
    }
  }

  void GenerateCCCFile() {
    using std::endl;

    if (GetParam() == GameType::fo4) {
      std::ofstream ccc(cccPath_);
      ccc << blankDifferentEsm << endl
          << blankDifferentMasterDependentEsm << endl;

      ccc.close();
    }
  }

  Game game_;
  const std::string blankEslEsp;
  const std::filesystem::path masterlistPath_;
  const std::filesystem::path cccPath_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_SUITE_P(,
                        PluginGraphTest,
                        ::testing::Values(GameType::tes4,
                                          GameType::tes5,
                                          GameType::tes5se));

TEST_P(PluginGraphTest, topologicalSortWithNoLoadedPluginsShouldReturnAnEmptyList) {
  PluginGraph graph;
  std::vector<std::string> sorted = graph.TopologicalSort();

  EXPECT_TRUE(sorted.empty());
}
}
}

#endif
