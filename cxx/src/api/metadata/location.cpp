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

#include "loot/metadata/location.h"

namespace loot {
Location::Location(std::string_view url, std::string_view name) :
    url_(url), name_(name) {}

std::string Location::GetURL() const { return url_; }

std::string Location::GetName() const { return name_; }

bool operator==(const Location& lhs, const Location& rhs) {
  return lhs.GetURL() == rhs.GetURL() && lhs.GetName() == rhs.GetName();
}

bool operator!=(const Location& lhs, const Location& rhs) {
  return !(lhs == rhs);
}

bool operator<(const Location& lhs, const Location& rhs) {
  if (lhs.GetURL() < rhs.GetURL()) {
    return true;
  }

  if (rhs.GetURL() < lhs.GetURL()) {
    return false;
  }

  return lhs.GetName() < rhs.GetName();
}

bool operator>(const Location& lhs, const Location& rhs) { return rhs < lhs; }

bool operator<=(const Location& lhs, const Location& rhs) {
  return !(lhs > rhs);
}

bool operator>=(const Location& lhs, const Location& rhs) {
  return !(lhs < rhs);
}
}
