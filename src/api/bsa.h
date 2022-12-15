/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2022 Oliver Hamlet

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
#ifndef LOOT_API_BSA
#define LOOT_API_BSA

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace loot {
std::map<uint64_t, std::set<uint64_t>> GetAssetsInBethesdaArchive(
    const std::filesystem::path& archivePath);

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBethesdaArchives(
    const std::vector<std::filesystem::path>& archivePaths);

bool DoAssetsIntersect(const std::map<uint64_t, std::set<uint64_t>>& left,
                       const std::map<uint64_t, std::set<uint64_t>>& right);
}

#endif
