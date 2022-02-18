/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2013-2016    WrinklyNinja

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

#include "api/game/game.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <cmath>
#include <map>
#include <thread>

#include "api/api_database.h"
#include "api/helpers/logging.h"
#include "api/sorting/plugin_sort.h"
#include "loot/exception/file_access_error.h"

#ifdef _WIN32
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#include "shlobj.h"
#include "shlwapi.h"
#include "windows.h"
#endif

using std::string;
using std::thread;
using std::vector;
using std::filesystem::u8path;

namespace loot {
Game::Game(const GameType gameType,
           const std::filesystem::path& gamePath,
           const std::filesystem::path& localDataPath) :
    type_(gameType),
    gamePath_(gamePath),
    loadOrderHandler_(type_, gamePath_, localDataPath),
    conditionEvaluator_(
        std::make_shared<ConditionEvaluator>(Type(), DataPath())),
    database_(ApiDatabase(conditionEvaluator_)) {
  auto logger = getLogger();
  if (logger) {
    logger->info("Initialising load order data for game of type {} at: {}",
                 (int)type_,
                 gamePath_.u8string());
  }
}

GameType Game::Type() const { return type_; }

std::filesystem::path Game::DataPath() const {
  if (type_ == GameType::tes3) {
    return gamePath_ / "Data Files";
  } else {
    return gamePath_ / "Data";
  }
}

GameCache& Game::GetCache() { return cache_; }

LoadOrderHandler& Game::GetLoadOrderHandler() { return loadOrderHandler_; }

DatabaseInterface& Game::GetDatabase() { return database_; }

bool Game::IsValidPlugin(const std::string& plugin) const {
  return Plugin::IsValid(Type(), DataPath() / u8path(plugin));
}

void Game::LoadPlugins(const std::vector<std::string>& plugins,
                       bool loadHeadersOnly) {
  auto logger = getLogger();
  std::multimap<uintmax_t, string> sizeMap;

  // First get the plugin sizes.
  for (const auto& plugin : plugins) {
    if (!IsValidPlugin(plugin))
      throw std::invalid_argument("\"" + plugin + "\" is not a valid plugin");

    uintmax_t fileSize = Plugin::GetFileSize(DataPath() / u8path(plugin));

    // Trim .ghost extension if present.
    if (boost::iends_with(plugin, GHOST_FILE_EXTENSION))
      sizeMap.emplace(
          fileSize,
          plugin.substr(0, plugin.length() - GHOST_FILE_EXTENSION_LENGTH));
    else
      sizeMap.emplace(fileSize, plugin);
  }

  // Get the number of threads to use.
  // hardware_concurrency() may be zero, if so then use only one thread.
  size_t threadsToUse =
      ::std::min((size_t)thread::hardware_concurrency(), sizeMap.size());
  threadsToUse = ::std::max(threadsToUse, (size_t)1);

  // Divide the plugins up by thread.
  vector<vector<string>> pluginGroups(threadsToUse);
  if (logger) {
    auto pluginsPerThread = sizeMap.size() / threadsToUse;
    logger->info(
        "Loading {} plugins using {} threads, with up to {} plugins per "
        "thread.",
        sizeMap.size(),
        threadsToUse,
        pluginsPerThread);
  }

  // The plugins should be split between the threads so that the data
  // load is as evenly spread as possible.
  size_t currentGroup = 0;
  for (const auto& plugin : sizeMap) {
    if (currentGroup == threadsToUse) {
      currentGroup = 0;
    }

    pluginGroups.at(currentGroup).push_back(plugin.second);
    ++currentGroup;
  }

  // Clear the existing plugin and archive caches.
  cache_.ClearCachedPlugins();

  // Search for and cache archives.
  CacheArchives();

  // Load the plugins.
  if (logger) {
    logger->trace("Starting plugin loading.");
  }
  auto masterPath = DataPath() / u8path(masterFilename_);
  vector<thread> threads;
  while (threads.size() < threadsToUse) {
    vector<string>& pluginGroup = pluginGroups.at(threads.size());
    threads.push_back(thread([&]() {
      for (auto pluginName : pluginGroup) {
        try {
          auto pluginPath = DataPath() / u8path(pluginName);
          const bool loadHeader =
              loadHeadersOnly || loot::equivalent(pluginPath, masterPath);

          cache_.AddPlugin(Plugin(Type(), cache_, pluginPath, loadHeader));
        } catch (const std::exception& e) {
          if (logger) {
            logger->error(
                "Caught exception while trying to add {} to the cache: {}",
                pluginName,
                e.what());
          }
        }
      }
    }));
  }

  // Join all threads.
  for (auto& thread : threads) {
    if (thread.joinable())
      thread.join();
  }

  conditionEvaluator_->RefreshLoadedPluginsState(GetLoadedPlugins());
}

const PluginInterface* Game::GetPlugin(const std::string& pluginName) const {
  return cache_.GetPlugin(pluginName);
}

std::vector<const PluginInterface*> Game::GetLoadedPlugins() const {
  std::vector<const PluginInterface*> interfacePointers;
  for (const auto plugin : cache_.GetPlugins()) {
    interfacePointers.push_back(plugin);
  }

  return interfacePointers;
}

void Game::IdentifyMainMasterFile(const std::string& masterFile) {
  masterFilename_ = masterFile;
}

std::vector<std::string> Game::SortPlugins(
    const std::vector<std::string>& plugins) {
  LoadPlugins(plugins, false);

  // Sort plugins into their load order.
  return loot::SortPlugins(*this, plugins);
}

void Game::LoadCurrentLoadOrderState() {
  loadOrderHandler_.LoadCurrentState();
  conditionEvaluator_->RefreshActivePluginsState(
      loadOrderHandler_.GetActivePlugins());
}

bool Game::IsPluginActive(const std::string& pluginName) const {
  return loadOrderHandler_.IsPluginActive(pluginName);
}

std::vector<std::string> Game::GetLoadOrder() const {
  return loadOrderHandler_.GetLoadOrder();
}

void Game::SetLoadOrder(const std::vector<std::string>& loadOrder) {
  loadOrderHandler_.SetLoadOrder(loadOrder);
}

void Game::CacheArchives() {
  const auto archiveFileExtension = GetArchiveFileExtension(Type());

  std::set<std::filesystem::path> archivePaths;
  for (std::filesystem::directory_iterator it(DataPath());
       it != std::filesystem::directory_iterator();
       ++it) {
    // This is only correct for ASCII strings, but that's all that
    // GetArchiveFileExtension() can return. It's a lot faster than the more
    // generally-correct approach of testing file path equivalence when
    // there are a lot of entries in DataPath().
    if (it->is_regular_file() &&
        boost::iends_with(it->path().u8string(), archiveFileExtension)) {
      archivePaths.insert(it->path());
    }
  }

  cache_.CacheArchivePaths(std::move(archivePaths));
}
}
