/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2018    WrinklyNinja

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

#ifndef LOOT_API_SORTING_PLUGIN_SORTING_DATA
#define LOOT_API_SORTING_PLUGIN_SORTING_DATA

#include "api/plugin.h"
#include "loot/metadata/plugin_metadata.h"

namespace loot {
class PluginSortingData : private PluginMetadata {
public:
  PluginSortingData(const Plugin& plugin, const PluginMetadata&& metadata);

  std::string GetName() const;
  bool IsMaster() const;
  bool LoadsArchive() const;
  std::vector<std::string> GetMasters() const;
  size_t NumOverrideFormIDs() const;
  bool DoFormIDsOverlap(const PluginSortingData& plugin) const;

  std::unordered_set<std::string> GetAfterGroupPlugins() const;
  void SetAfterGroupPlugins(std::unordered_set<std::string> plugins);

  using PluginMetadata::GetGroup;
  using PluginMetadata::GetLoadAfterFiles;
  using PluginMetadata::GetRequirements;

private:
  const Plugin& plugin_;
  std::unordered_set<std::string> afterGroupPlugins_;
};
}

#endif
