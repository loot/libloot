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

#include "loot/metadata/file.h"

namespace loot {
File::File(std::string_view name,
           std::string_view display,
           std::string_view condition,
           const std::vector<MessageContent>& detail,
           std::string_view constraint) :
    ConditionalMetadata(condition),
    name_(Filename(name)),
    display_(display),
    detail_(detail),
    constraint_(constraint) {}

Filename File::GetName() const { return name_; }

std::string File::GetDisplayName() const { return display_; }

std::vector<MessageContent> File::GetDetail() const { return detail_; }

std::string File::GetConstraint() const { return constraint_; }

bool operator==(const File& lhs, const File& rhs) {
  return lhs.GetDisplayName() == rhs.GetDisplayName() &&
         lhs.GetCondition() == rhs.GetCondition() &&
         lhs.GetConstraint() == rhs.GetConstraint() &&
         lhs.GetName() == rhs.GetName() && lhs.GetDetail() == rhs.GetDetail();
}

bool operator!=(const File& lhs, const File& rhs) { return !(lhs == rhs); }

bool operator<(const File& lhs, const File& rhs) {
  if (lhs.GetDisplayName() < rhs.GetDisplayName()) {
    return true;
  }

  if (rhs.GetDisplayName() < lhs.GetDisplayName()) {
    return false;
  }

  if (lhs.GetCondition() < rhs.GetCondition()) {
    return true;
  }

  if (rhs.GetCondition() < lhs.GetCondition()) {
    return false;
  }

  if (lhs.GetConstraint() < rhs.GetConstraint()) {
    return true;
  }

  if (rhs.GetConstraint() < lhs.GetConstraint()) {
    return false;
  }

  if (lhs.GetName() < rhs.GetName()) {
    return true;
  }

  if (rhs.GetName() < lhs.GetName()) {
    return false;
  }

  return lhs.GetDetail() < rhs.GetDetail();
}

bool operator>(const File& lhs, const File& rhs) { return rhs < lhs; }

bool operator<=(const File& lhs, const File& rhs) { return !(lhs > rhs); }

bool operator>=(const File& lhs, const File& rhs) { return !(lhs < rhs); }
}
