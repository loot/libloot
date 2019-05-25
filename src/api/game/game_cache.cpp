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

#include <thread>

#include "api/helpers/text.h"

using std::lock_guard;
using std::mutex;
using std::pair;
using std::string;

namespace loot {
GameCache::GameCache() {}

GameCache::GameCache(const GameCache& cache) :
    plugins_(cache.plugins_) {}

GameCache& GameCache::operator=(const GameCache& cache) {
  if (&cache != this) {
    plugins_ = cache.plugins_;
  }

  return *this;
}

std::set<std::shared_ptr<const Plugin>> GameCache::GetPlugins() const {
  std::set<std::shared_ptr<const Plugin>> output;
  std::transform(
      begin(plugins_),
      end(plugins_),
      std::inserter<std::set<std::shared_ptr<const Plugin>>>(output,
                                                             begin(output)),
      [](const pair<string, std::shared_ptr<const Plugin>>& pluginPair) {
        return pluginPair.second;
      });
  return output;
}

std::shared_ptr<const Plugin> GameCache::GetPlugin(
    const std::string& pluginName) const {
  auto it = plugins_.find(NormalizeFilename(pluginName));
  if (it != end(plugins_))
    return it->second;

  return nullptr;
}

void GameCache::AddPlugin(const Plugin&& plugin) {
  lock_guard<mutex> lock(mutex_);

  auto normalizedName = NormalizeFilename(plugin.GetName());

  auto it = plugins_.find(normalizedName);
  if (it != end(plugins_))
    plugins_.erase(it);

  plugins_.emplace(normalizedName,
                   std::make_shared<Plugin>(std::move(plugin)));
}

std::set<std::filesystem::path> GameCache::GetArchivePaths() const
{
  return archivePaths_;
}

void GameCache::CacheArchivePath(const std::filesystem::path& path)
{
  lock_guard<mutex> lock(mutex_);

  archivePaths_.insert(path);
}

void GameCache::ClearCachedPlugins() {
  lock_guard<mutex> guard(mutex_);

  plugins_.clear();
}

void GameCache::ClearCachedArchivePaths() {
  lock_guard<mutex> guard(mutex_);

  archivePaths_.clear();
}
}
