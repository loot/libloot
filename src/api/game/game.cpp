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
#include <cmath>
#include <thread>

#include <boost/algorithm/string.hpp>

#include "api/api_database.h"
#include "api/helpers/logging.h"
#include "api/sorting/plugin_sorter.h"
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

using std::filesystem::u8path;
using std::list;
using std::string;
using std::thread;
using std::vector;

namespace loot {
Game::Game(const GameType gameType,
           const std::filesystem::path& gamePath,
           const std::filesystem::path& localDataPath) :
    type_(gameType),
    gamePath_(gamePath),
    cache_(std::make_shared<GameCache>()),
    loadOrderHandler_(std::make_shared<LoadOrderHandler>()) {
  auto logger = getLogger();
  if (logger) {
    logger->info("Initialising load order data for game of type {} at: {}",
                 (int)type_,
                 gamePath_.u8string());
  }

  loadOrderHandler_->Init(type_, gamePath_, localDataPath);

  database_ = std::make_shared<ApiDatabase>(
      Type(), DataPath(), GetCache(), GetLoadOrderHandler());
}

GameType Game::Type() const { return type_; }

std::filesystem::path Game::DataPath() const { return gamePath_ / "Data"; }

std::shared_ptr<GameCache> Game::GetCache() { return cache_; }

std::shared_ptr<LoadOrderHandler> Game::GetLoadOrderHandler() {
  return loadOrderHandler_;
}

std::shared_ptr<DatabaseInterface> Game::GetDatabase() { return database_; }

bool Game::IsValidPlugin(const std::string& plugin) const {
  return Plugin::IsValid(Type(), DataPath() / u8path(plugin));
}

void Game::LoadPlugins(const std::vector<std::string>& plugins,
                       bool loadHeadersOnly) {

  auto logger = getLogger();
  uintmax_t meanFileSize = 0;
  std::multimap<uintmax_t, string> sizeMap;

  // First get the plugin sizes.
  for (const auto& plugin : plugins) {
    if (!IsValidPlugin(plugin))
      throw std::invalid_argument("\"" + plugin + "\" is not a valid plugin");

    uintmax_t fileSize = Plugin::GetFileSize(DataPath() / u8path(plugin));
    meanFileSize += fileSize;

    // Trim .ghost extension if present.
    if (boost::iends_with(plugin, ".ghost"))
      sizeMap.emplace(fileSize, plugin.substr(0, plugin.length() - 6));
    else
      sizeMap.emplace(fileSize, plugin);
  }
  meanFileSize /= sizeMap.size();  // Rounding error, but not important.

  // Get the number of threads to use.
  // hardware_concurrency() may be zero, if so then use only one thread.
  size_t threadsToUse =
      ::std::min((size_t)thread::hardware_concurrency(), sizeMap.size());
  threadsToUse = ::std::max(threadsToUse, (size_t)1);

  // Divide the plugins up by thread.
  unsigned int pluginsPerThread = ceil((double)sizeMap.size() / threadsToUse);
  vector<vector<string>> pluginGroups(threadsToUse);
  if (logger) {
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

    if (logger) {
      logger->trace(
          "Adding plugin {} to loading group {}", plugin.second, currentGroup);
    }

    pluginGroups[currentGroup].push_back(plugin.second);
    ++currentGroup;
  }

  // Clear the existing plugin and archive caches.
  cache_->ClearCachedPlugins();
  cache_->ClearCachedArchivePaths();

  // Search for and cache archives.
  CacheArchives();

  // Load the plugins.
  if (logger) {
    logger->trace("Starting plugin loading.");
  }
  vector<thread> threads;
  while (threads.size() < threadsToUse) {
    vector<string>& pluginGroup = pluginGroups[threads.size()];
    threads.push_back(thread([&]() {
      for (auto pluginName : pluginGroup) {
        if (logger) {
          logger->trace("Loading {}", pluginName);
        }
        const bool loadHeader =
            boost::iequals(pluginName, masterFile_) || loadHeadersOnly;
        try {
          cache_->AddPlugin(Plugin(
              Type(),  cache_, loadOrderHandler_, DataPath() / u8path(pluginName), loadHeader));
        } catch (std::exception& e) {
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
}

std::optional<std::shared_ptr<const PluginInterface>> Game::GetPlugin(
    const std::string& pluginName) const {
  return cache_->GetPlugin(pluginName);
}

std::set<std::shared_ptr<const PluginInterface>> Game::GetLoadedPlugins()
    const {
  std::set<std::shared_ptr<const PluginInterface>> interfacePointers;
  for (auto& plugin : cache_->GetPlugins()) {
    interfacePointers.insert(
        std::static_pointer_cast<const PluginInterface>(plugin));
  }

  return interfacePointers;
}

void Game::IdentifyMainMasterFile(const std::string& masterFile) {
  masterFile_ = masterFile;
}

std::vector<std::string> Game::SortPlugins(
    const std::vector<std::string>& plugins) {
  LoadPlugins(plugins, false);

  // Sort plugins into their load order.
  PluginSorter sorter;
  return sorter.Sort(*this);
}

void Game::LoadCurrentLoadOrderState() {
  loadOrderHandler_->LoadCurrentState();
}

bool Game::IsPluginActive(const std::string& pluginName) const {
  auto plugin = cache_->GetPlugin(pluginName);

  if (plugin) {
    return plugin.value()->IsActive();
  }

  return loadOrderHandler_->IsPluginActive(pluginName);
}

std::vector<std::string> Game::GetLoadOrder() const {
  return loadOrderHandler_->GetLoadOrder();
}

void Game::SetLoadOrder(const std::vector<std::string>& loadOrder) {
  loadOrderHandler_->SetLoadOrder(loadOrder);
}

void Game::CacheArchives() {
  const auto archiveFileExtension = GetArchiveFileExtension(Type());

  for (std::filesystem::directory_iterator it(DataPath());
    it != std::filesystem::directory_iterator();
    ++it) {
    // Check if the path is an archive by checking if replacing its
    // file extension with the archive extension resolves to the same file.
    // Could use boost::iends_with, but it's less obvious here that the
    // test string is ASCII-only.
    if (loot::equivalent(it->path(), replaceExtension(it->path(), archiveFileExtension))) {
      cache_->CacheArchivePath(it->path());
    }
  }
}
}
