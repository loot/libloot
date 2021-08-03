/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

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
#ifndef LOOT_FILE_REVISION
#define LOOT_FILE_REVISION

#include <string>

namespace loot {
/**
 * @brief A structure that holds data about a file's source control revision.
 */
struct FileRevision {
  inline explicit FileRevision() : is_modified(false) {}

  /**
   * @brief The revision hash for the file.
   */
  std::string id;

  /**
   * @brief A string containing the ISO 8601 formatted revision date, ie.
   *        YYYY-MM-DD.
   */
  std::string date;

  /**
   * @brief `true` if the file has been edited since the revision identified by
   *        `id`, or `false` if it is at exactly the revision given.
   */
  bool is_modified;
};
}

#endif
