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

#include <stdexcept>

#include "libloot-cpp/src/lib.rs.h"

namespace loot {
Filename::Filename(std::string_view filename) : filename_(filename) {}

Filename::operator std::string() const { return filename_; }

std::weak_ordering operator<=>(const Filename& lhs, const Filename& rhs) {
  auto result = loot::rust::new_filename(lhs.filename_)
                    ->cmp(*loot::rust::new_filename(rhs.filename_));

  if (result > 0) {
    return std::weak_ordering::greater;
  }

  if (result == 0) {
    return std::weak_ordering::equivalent;
  }

  return std::weak_ordering::less;
}

bool operator==(const Filename& lhs, const Filename& rhs) {
  return (lhs <=> rhs) == std::weak_ordering::equivalent;
}
}
