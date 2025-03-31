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

#ifndef LOOT_API_GAME_GAME_CACHE
#define LOOT_API_GAME_GAME_CACHE

#include <string>
#include <string_view>
#include <unordered_map>

#include "api/plugin.h"

namespace loot {
class GameCache {
public:
  std::vector<std::shared_ptr<const Plugin>> GetPlugins() const;
  std::shared_ptr<const Plugin> GetPlugin(std::string_view pluginName) const;
  void AddPlugin(Plugin&& plugin);

  std::vector<const Plugin*> GetPluginsWithReplacements(
      const std::vector<Plugin>& newPlugins) const;

  std::set<std::filesystem::path> GetArchivePaths() const;
  void CacheArchivePaths(std::set<std::filesystem::path>&& paths);

  void ClearCachedPlugins();

private:
  std::unordered_map<std::string, std::shared_ptr<const Plugin>> plugins_;
  std::set<std::filesystem::path> archivePaths_;
};
}

#endif
