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

#ifndef LOOT_API_SORTING_PLUGIN_SORT
#define LOOT_API_SORTING_PLUGIN_SORT

#include <string>
#include <vector>

#include "api/game/game.h"
#include "api/sorting/plugin_sorting_data.h"

namespace loot {
std::vector<std::string> SortPlugins(
    std::vector<PluginSortingData>&& pluginsSortingData,
    const GameType gameType,
    const std::vector<Group> masterlistGroups,
    const std::vector<Group> userGroups,
    const std::vector<std::string>& implicitlyActivePlugins);

std::vector<std::string> SortPlugins(Game& game,
                                     const std::vector<std::string>& loadOrder);
}

#endif
