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

#include "loot/metadata/filename.h"

#include "api/helpers/text.h"

namespace loot {
Filename::Filename() {}

Filename::Filename(const std::string& filename) :
    filename_(filename) {}

Filename::operator std::string() const { return filename_; }

bool operator==(const Filename& lhs, const Filename& rhs) {
  return CompareFilenames(std::string(lhs), std::string(rhs)) == 0;
}

bool operator!=(const Filename& lhs, const Filename& rhs) { return !(lhs == rhs); }

bool operator<(const Filename& lhs, const Filename& rhs) {
  return CompareFilenames(std::string(lhs), std::string(rhs)) < 0;
}

bool operator>(const Filename& lhs, const Filename& rhs) { return rhs < lhs; }

bool operator<=(const Filename& lhs, const Filename& rhs) { return !(lhs > rhs); }

bool operator>=(const Filename& lhs, const Filename& rhs) { return !(lhs < rhs); }
}
