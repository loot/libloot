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

#ifndef LOOT_API_HELPERS_TEXT
#define LOOT_API_HELPERS_TEXT

#include <optional>
#include <string>
#include <vector>

#include "loot/metadata/tag.h"

namespace loot {
std::vector<Tag> ExtractBashTags(const std::string& description);

std::optional<std::string> ExtractVersion(const std::string& text);

// Compare strings as if they're filenames, respecting filesystem case
// insensitivity on Windows. Returns -1 if lhs < rhs, 0 if lhs == rhs, and 1 if
// lhs > rhs. The comparison may give different results on Linux, but is still
// locale-invariant.
int CompareFilenames(const std::string& lhs, const std::string& rhs);

// Normalize the given filename in a way that is locale-invariant. On Windows,
// this uppercases the filename according to the same case mapping rules as used
// by the filesystem. On Linux, case folding is used and gives results that are
// different but hopefully still consistent enough with the behaviour on Windows
// that the normalized filenames distinguish characters in a similar way to the
// Windows filesystem.
std::string NormalizeFilename(const std::string& filename);
}

#endif
