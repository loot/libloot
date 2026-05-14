/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2013-2026 Oliver Hamlet

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

#ifndef LOOT_TESTS_API_INTERFACE_API_GAME_OPERATIONS_TEST
#define LOOT_TESTS_API_INTERFACE_API_GAME_OPERATIONS_TEST

#include <fstream>

#include "loot/api.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class ApiGameOperationsTest : public CommonGameTestFixture,
                              public testing::WithParamInterface<GameType> {
protected:
  static constexpr std::string_view MASTER_FILE = "master.esm";
  static constexpr std::string_view GENERAL_MASTERLIST_MESSAGE =
      "A general masterlist message.";

  ApiGameOperationsTest() :
      CommonGameTestFixture(GetParam()),
      handle_(nullptr),
      masterlistPath(localPath / "masterlist.yaml") {}

  void SetUp() override {
    CommonGameTestFixture::SetUp();

    handle_ = CreateGameHandle(GetParam(), gamePath, localPath);
  }

  void GenerateMasterlist() {
    using std::endl;

    static constexpr std::string_view NOTE_MESSAGE =
        "Do not clean ITM records, they are intentional and required for the "
        "mod to function.";
    static constexpr std::string_view WARNING_MESSAGE =
        "Check you are using v2+. If not, Update. v1 has a severe bug with the "
        "Mystic Emporium disappearing.";
    static constexpr std::string_view ERROR_MESSAGE =
        "Obsolete. Remove this and install Enhanced Weather.";

    std::ofstream masterlist(masterlistPath);
    masterlist << "bash_tags:" << endl
               << "  - Actors.ACBS" << endl
               << "  - C.Climate" << endl
               << "globals:" << endl
               << "  - type: say" << endl
               << "    content: '" << GENERAL_MASTERLIST_MESSAGE << "'" << endl
               << "    condition: 'file(\"" << MISSING_ESP << "\")'" << endl
               << "groups:" << endl
               << "  - name: group1" << endl
               << "  - name: group2" << endl
               << "    after:" << endl
               << "      - group1" << endl
               << "plugins:" << endl
               << "  - name: " << BLANK_ESM << endl
               << "    after:" << endl
               << "      - " << MASTER_FILE << endl
               << "    msg:" << endl
               << "      - type: say" << endl
               << "        content: '" << NOTE_MESSAGE << "'" << endl
               << "        condition: 'file(\"" << MISSING_ESP << "\")'" << endl
               << "    tag:" << endl
               << "      - Actors.ACBS" << endl
               << "      - Actors.AIData" << endl
               << "      - '-C.Water'" << endl
               << "  - name: " << BLANK_DIFFERENT_ESM << endl
               << "    after:" << endl
               << "      - " << BLANK_MASTER_DEPENDENT_ESM << endl
               << "    msg:" << endl
               << "      - type: warn" << endl
               << "        content: '" << WARNING_MESSAGE << "'" << endl
               << "    dirty:" << endl
               << "      - crc: 0x7d22f9df" << endl
               << "        util: TES4Edit" << endl
               << "        udr: 4" << endl
               << "  - name: " << BLANK_DIFFERENT_ESP << endl
               << "    after:" << endl
               << "      - " << BLANK_PLUGIN_DEPENDENT_ESP << endl
               << "    msg:" << endl
               << "      - type: error" << endl
               << "        content: '" << ERROR_MESSAGE << "'" << endl
               << "  - name: " << BLANK_ESP << endl
               << "    after:" << endl
               << "      - " << BLANK_DIFFERENT_MASTER_DEPENDENT_ESP << endl
               << "      - " << BLANK_OVERRIDE_ESP << endl
               << "  - name: " << BLANK_DIFFERENT_MASTER_DEPENDENT_ESP << endl
               << "    after:" << endl
               << "      - " << BLANK_MASTER_DEPENDENT_ESP << endl
               << "    msg:" << endl
               << "      - type: say" << endl
               << "        content: '" << NOTE_MESSAGE << "'" << endl
               << "      - type: warn" << endl
               << "        content: '" << WARNING_MESSAGE << "'" << endl
               << "      - type: error" << endl
               << "        content: '" << ERROR_MESSAGE << "'" << endl;

    masterlist.close();
  }

  std::unique_ptr<GameInterface> handle_;

  std::filesystem::path masterlistPath;
};
}
}

#endif
