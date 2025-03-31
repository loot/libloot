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

#include "api/game/game_cache.h"

#include "api/helpers/text.h"

namespace loot {
std::vector<std::shared_ptr<const Plugin>> GameCache::GetPlugins() const {
  std::vector<std::shared_ptr<const Plugin>> output(plugins_.size());
  std::transform(
      begin(plugins_), end(plugins_), begin(output), [](const auto& pair) {
        return pair.second;
      });
  return output;
}

std::shared_ptr<const Plugin> GameCache::GetPlugin(
    std::string_view pluginName) const {
  const auto it = plugins_.find(NormalizeFilename(pluginName));
  if (it != end(plugins_))
    return it->second;

  return nullptr;
}

void GameCache::AddPlugin(Plugin&& plugin) {
  auto normalizedName = NormalizeFilename(plugin.GetName());
  auto pluginPointer = std::make_shared<Plugin>(std::move(plugin));

  const auto it = plugins_.find(normalizedName);
  if (it != end(plugins_)) {
    it->second = pluginPointer;
  } else {
    plugins_.emplace(normalizedName, pluginPointer);
  }
}

std::vector<const Plugin*> GameCache::GetPluginsWithReplacements(
    const std::vector<Plugin>& newPlugins) const {
  std::unordered_map<std::string, const Plugin*> pluginsMap;
  for (const auto& plugin : newPlugins) {
    pluginsMap.emplace(NormalizeFilename(plugin.GetName()), &plugin);
  }
  for (const auto& [key, plugin] : plugins_) {
    pluginsMap.emplace(key, plugin.get());
  }

  std::vector<const Plugin*> loadedPlugins;
  for (const auto& [key, plugin] : pluginsMap) {
    loadedPlugins.push_back(plugin);
  }

  return loadedPlugins;
}

std::set<std::filesystem::path> GameCache::GetArchivePaths() const {
  return archivePaths_;
}

void GameCache::CacheArchivePaths(std::set<std::filesystem::path>&& paths) {
  archivePaths_ = std::move(paths);
}

void GameCache::ClearCachedPlugins() { plugins_.clear(); }
}
