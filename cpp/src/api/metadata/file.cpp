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
    name_(Filename(name)),
    display_(display),
    detail_(detail),
    condition_(condition),
    constraint_(constraint) {}

Filename File::GetName() const { return name_; }

std::string File::GetDisplayName() const { return display_; }

std::vector<MessageContent> File::GetDetail() const { return detail_; }

std::string File::GetCondition() const { return condition_; }

std::string File::GetConstraint() const { return constraint_; }
}
