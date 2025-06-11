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

#include "loot/metadata/tag.h"

namespace loot {
Tag::Tag(std::string_view tag,
         const bool isAddition,
         std::string_view condition) :
    name_(tag), addTag_(isAddition), condition_(condition) {}

bool Tag::IsAddition() const { return addTag_; }

std::string Tag::GetName() const { return name_; }

std::string Tag::GetCondition() const { return condition_; }

std::strong_ordering operator<=>(const Tag& lhs, const Tag& rhs) {
  if (lhs.IsAddition() != rhs.IsAddition()) {
    if (lhs.IsAddition()) {
      return std::strong_ordering::less;
    }

    return std::strong_ordering::greater;
  }

  auto nameOrder = lhs.GetName() <=> rhs.GetName();
  if (nameOrder != std::strong_ordering::equal) {
    return nameOrder;
  }

  return lhs.GetCondition() <=> rhs.GetCondition();
}
}
