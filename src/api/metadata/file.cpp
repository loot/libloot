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

#include "api/helpers/text.h"
#include "api/metadata/yaml/file.h"

namespace loot {
File::File() {}

File::File(const std::string& name,
           const std::string& display,
           const std::string& condition) :
    name_(Filename(name)),
    display_(display),
    ConditionalMetadata(condition) {}

bool File::operator<(const File& rhs) const {
  if (display_ < rhs.display_) {
    return true;
  }

  if (rhs.display_ < display_) {
    return false;
  }

  if (this->GetCondition() < rhs.GetCondition()) {
    return true;
  }

  if (rhs.GetCondition() < this->GetCondition()) {
    return false;
  }

  return name_ < rhs.name_;
}

bool File::operator==(const File& rhs) const {
  return display_ == rhs.display_ && GetCondition() == rhs.GetCondition() &&
         name_ == rhs.name_;
}

Filename File::GetName() const { return name_; }

std::string File::GetDisplayName() const {
  if (display_.empty())
    return std::string(name_);
  else
    return display_;
}

bool operator!=(const File& lhs, const File& rhs) { return !(lhs == rhs); }

bool operator>(const File& lhs, const File& rhs) { return rhs < lhs; }

bool operator<=(const File& lhs, const File& rhs) { return !(lhs > rhs); }

bool operator>=(const File& lhs, const File& rhs) { return !(lhs < rhs); }
}
