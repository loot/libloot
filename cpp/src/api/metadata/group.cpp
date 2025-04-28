/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2018   WrinklyNinja

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

#include "loot/metadata/group.h"

namespace loot {
Group::Group(std::string_view name,
             const std::vector<std::string>& afterGroups,
             std::string_view description) :
    name_(name), description_(description), afterGroups_(afterGroups) {}

std::string Group::GetName() const { return name_; }

std::string Group::GetDescription() const { return description_; }

std::vector<std::string> Group::GetAfterGroups() const { return afterGroups_; }

bool operator==(const Group& lhs, const Group& rhs) {
  return lhs.GetName() == rhs.GetName() &&
         lhs.GetDescription() == rhs.GetDescription() &&
         lhs.GetAfterGroups() == rhs.GetAfterGroups();
}

bool operator!=(const Group& lhs, const Group& rhs) { return !(lhs == rhs); }

bool operator<(const Group& lhs, const Group& rhs) {
  if (lhs.GetName() < rhs.GetName()) {
    return true;
  }

  if (rhs.GetName() < lhs.GetName()) {
    return false;
  }

  if (lhs.GetDescription() < rhs.GetDescription()) {
    return true;
  }

  if (rhs.GetDescription() < lhs.GetDescription()) {
    return false;
  }

  return lhs.GetAfterGroups() < rhs.GetAfterGroups();
}

bool operator>(const Group& lhs, const Group& rhs) { return rhs < lhs; }

bool operator<=(const Group& lhs, const Group& rhs) { return !(lhs > rhs); }

bool operator>=(const Group& lhs, const Group& rhs) { return !(lhs < rhs); }
}
