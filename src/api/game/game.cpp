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
#include <execution>
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
    database_(ApiDatabase(conditionEvaluator_)) {}

GameType Game::Type() const { return type_; }

std::filesystem::path Game::DataPath() const {
  if (type_ == GameType::tes3) {
    return gamePath_ / "Data Files";
  } else {
    return gamePath_ / "Data";
  }
}

GameCache& Game::GetCache() { return cache_; }

const GameCache& Game::GetCache() const { return cache_; }

LoadOrderHandler& Game::GetLoadOrderHandler() { return loadOrderHandler_; }

const LoadOrderHandler& Game::GetLoadOrderHandler() const {
  return loadOrderHandler_;
}

const DatabaseInterface& Game::GetDatabase() const { return database_; }

DatabaseInterface& Game::GetDatabase() { return database_; }

bool Game::IsValidPlugin(const std::string& plugin) const {
  return Plugin::IsValid(Type(), DataPath() / u8path(plugin));
}

void Game::LoadPlugins(const std::vector<std::string>& plugins,
                       bool loadHeadersOnly) {
  const auto logger = getLogger();

  // First validate the plugins (the validity check is done in parallel because
  // it's relatively slow).
  const auto invalidPluginIt =
      std::find_if(std::execution::par_unseq,
                   plugins.cbegin(),
                   plugins.cend(),
                   [this](const std::string& pluginName) {
                     try {
                       return !IsValidPlugin(pluginName);
                     } catch (...) {
                       return true;
                     }
                   });

  if (invalidPluginIt != plugins.end()) {
    throw std::invalid_argument("\"" + *invalidPluginIt +
                                "\" is not a valid plugin");
  }

  // Clear the existing plugin and archive caches.
  cache_.ClearCachedPlugins();

  // Search for and cache archives.
  CacheArchives();

  // Load the plugins.
  if (logger) {
    logger->trace("Starting plugin loading.");
  }

  const auto masterPath = DataPath() / u8path(masterFilename_);
  std::for_each(
      std::execution::par_unseq,
      plugins.begin(),
      plugins.end(),
      [&](const std::string& pluginName) {
        try {
          const auto endIt =
              boost::iends_with(pluginName, GHOST_FILE_EXTENSION)
                  ? pluginName.end() - GHOST_FILE_EXTENSION_LENGTH
                  : pluginName.end();

          auto pluginPath = DataPath() / u8path(pluginName.begin(), endIt);
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
      });

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

bool Game::IsLoadOrderAmbiguous() const {
  return loadOrderHandler_.IsAmbiguous();
}

std::filesystem::path Game::GetActivePluginsFilePath() const {
  return loadOrderHandler_.GetActivePluginsFilePath();
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
