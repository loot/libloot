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
#include "shlobj.h"
#include "shlwapi.h"
#include "windows.h"
#endif

using std::filesystem::u8path;

namespace {
using loot::GameType;

// The Microsoft Store installs Fallout 4 DLCs to directories outside of the
// game's install path. These directories have fixed paths relative to the
// game install path (renaming them causes the game launch to fail, or not
// find the DLC files).
constexpr const char* MS_FO4_AUTOMATRON_DATA_PATH =
    "../../Fallout 4- Automatron (PC)/Content/Data";
constexpr const char* MS_FO4_CONTRAPTIONS_DATA_PATH =
    "../../Fallout 4- Contraptions Workshop (PC)/Content/Data";
constexpr const char* MS_FO4_FAR_HARBOR_DATA_PATH =
    "../../Fallout 4- Far Harbor (PC)/Content/Data";
constexpr const char* MS_FO4_TEXTURE_PACK_DATA_PATH =
    "../../Fallout 4- High Resolution Texture Pack/Content/Data";
constexpr const char* MS_FO4_NUKA_WORLD_DATA_PATH =
    "../../Fallout 4- Nuka-World (PC)/Content/Data";
constexpr const char* MS_FO4_VAULT_TEC_DATA_PATH =
    "../../Fallout 4- Vault-Tec Workshop (PC)/Content/Data";
constexpr const char* MS_FO4_WASTELAND_DATA_PATH =
    "../../Fallout 4- Wasteland Workshop (PC)/Content/Data";

bool IsMicrosoftStoreInstall(const GameType gameType,
                             const std::filesystem::path& gamePath) {
  switch (gameType) {
    case GameType::tes3:
    case GameType::tes4:
    case GameType::fo3:
    case GameType::fonv:
      // tes3, tes4, fo3 and fonv install paths are localised, with the
      // appxmanifest.xml file sitting in the parent directory.
      return std::filesystem::exists(gamePath.parent_path() /
                                     "appxmanifest.xml");
    case GameType::tes5se:
    case GameType::fo4:
      return std::filesystem::exists(gamePath / "appxmanifest.xml");
    default:
      return false;
  }
}

std::vector<std::filesystem::path> GetAdditionalDataPaths(
    const GameType gameType,
    const std::filesystem::path& dataPath) {
  const auto gamePath = dataPath.parent_path();

  if (gameType == GameType::fo4 &&
      IsMicrosoftStoreInstall(gameType, gamePath)) {
    // All DLC directories are listed before the main data path because DLC
    // plugins in those directories override any in the main data path.
    return {gamePath / MS_FO4_AUTOMATRON_DATA_PATH,
            gamePath / MS_FO4_NUKA_WORLD_DATA_PATH,
            gamePath / MS_FO4_WASTELAND_DATA_PATH,
            gamePath / MS_FO4_TEXTURE_PACK_DATA_PATH,
            gamePath / MS_FO4_VAULT_TEC_DATA_PATH,
            gamePath / MS_FO4_FAR_HARBOR_DATA_PATH,
            gamePath / MS_FO4_CONTRAPTIONS_DATA_PATH,
            dataPath};
  }

  return {};
}

std::filesystem::path ResolvePluginPath(
    const std::filesystem::path& dataPath,
    const std::filesystem::path& pluginPath) {
  return pluginPath.is_absolute() ? pluginPath : dataPath / pluginPath;
}

std::vector<std::filesystem::path> FindArchives(
    const std::filesystem::path& parentPath,
    const std::string& archiveFileExtension) {
  if (!std::filesystem::is_directory(parentPath)) {
    return {};
  }

  std::vector<std::filesystem::path> archivePaths;

  for (std::filesystem::directory_iterator it(parentPath);
       it != std::filesystem::directory_iterator();
       ++it) {
    // This is only correct for ASCII strings, but that's all that
    // GetArchiveFileExtension() can return. It's a lot faster than the more
    // generally-correct approach of testing file path equivalence when
    // there are a lot of entries in DataPath().
    if (it->is_regular_file() &&
        boost::iends_with(it->path().u8string(), archiveFileExtension)) {
      archivePaths.push_back(it->path());
    }
  }

  return archivePaths;
}

std::vector<std::filesystem::path> StringsToPaths(
    const std::vector<std::string>& pluginPathStrings) {
  std::vector<std::filesystem::path> pluginPaths;
  for (const auto& pluginPathString : pluginPathStrings) {
    pluginPaths.push_back(u8path(pluginPathString));
  }

  return pluginPaths;
}
}

namespace loot {
Game::Game(const GameType gameType,
           const std::filesystem::path& gamePath,
           const std::filesystem::path& localDataPath) :
    type_(gameType),
    gamePath_(gamePath),
    loadOrderHandler_(type_, gamePath_, localDataPath),
    conditionEvaluator_(
        std::make_shared<ConditionEvaluator>(Type(), DataPath())),
    database_(ApiDatabase(conditionEvaluator_)),
    additionalDataPaths_(GetAdditionalDataPaths(Type(), DataPath())) {
  conditionEvaluator_->SetAdditionalDataPaths(additionalDataPaths_);
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

const GameCache& Game::GetCache() const { return cache_; }

LoadOrderHandler& Game::GetLoadOrderHandler() { return loadOrderHandler_; }

const LoadOrderHandler& Game::GetLoadOrderHandler() const {
  return loadOrderHandler_;
}

const DatabaseInterface& Game::GetDatabase() const { return database_; }

DatabaseInterface& Game::GetDatabase() { return database_; }

void Game::SetAdditionalDataPaths(
    const std::vector<std::filesystem::path>& additionalDataPaths) {
  additionalDataPaths_ = additionalDataPaths;

  conditionEvaluator_->SetAdditionalDataPaths(additionalDataPaths_);
  conditionEvaluator_->ClearConditionCache();
  loadOrderHandler_.SetAdditionalDataPaths(additionalDataPaths_);
}

bool Game::IsValidPlugin(const std::string& pluginPath) const {
  return IsValidPlugin(u8path(pluginPath));
}

bool Game::IsValidPlugin(const std::filesystem::path& pluginPath) const {
  return Plugin::IsValid(Type(), ResolvePluginPath(DataPath(), pluginPath));
}

void Game::LoadPlugins(const std::vector<std::string>& pluginPathStrings,
                       bool loadHeadersOnly) {
  return LoadPlugins(StringsToPaths(pluginPathStrings), loadHeadersOnly);
}

void Game::LoadPlugins(const std::vector<std::filesystem::path>& pluginPaths,
                       bool loadHeadersOnly) {
  const auto logger = getLogger();

  // Check that all plugin filenames are unique.
  std::unordered_set<std::string> filenames;
  for (const auto& pluginPath : pluginPaths) {
    const auto filename = NormalizeFilename(pluginPath.filename().u8string());
    const auto inserted = filenames.insert(filename).second;
    if (!inserted) {
      throw std::invalid_argument("The filename \"" + filename +
                                  "\" is not unique.");
    }
  }

  // Validate the plugins (the validity check is done in parallel because
  // it's relatively slow).
  const auto invalidPluginIt =
      std::find_if(std::execution::par_unseq,
                   pluginPaths.cbegin(),
                   pluginPaths.cend(),
                   [this](const std::filesystem::path& pluginPath) {
                     try {
                       return !IsValidPlugin(pluginPath);
                     } catch (...) {
                       return true;
                     }
                   });

  if (invalidPluginIt != pluginPaths.end()) {
    throw std::invalid_argument("\"" + invalidPluginIt->u8string() +
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
      pluginPaths.begin(),
      pluginPaths.end(),
      [&](const std::filesystem::path& pluginPath) {
        try {
          const auto resolvedPluginPath =
              boost::iequals(pluginPath.extension().u8string(),
                             GHOST_FILE_EXTENSION)
                  ? ResolvePluginPath(
                        DataPath(),
                        std::filesystem::path(pluginPath).replace_extension())
                  : ResolvePluginPath(DataPath(), pluginPath);

          const bool loadHeader =
              loadHeadersOnly ||
              loot::equivalent(resolvedPluginPath, masterPath);

          cache_.AddPlugin(
              Plugin(Type(), cache_, resolvedPluginPath, loadHeader));
        } catch (const std::exception& e) {
          if (logger) {
            logger->error(
                "Caught exception while trying to add {} to the cache: {}",
                pluginPath.u8string(),
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
    const std::vector<std::string>& pluginPathStrings) {
  return SortPlugins(StringsToPaths(pluginPathStrings));
}

std::vector<std::string> Game::SortPlugins(
    const std::vector<std::filesystem::path>& pluginPaths) {
  LoadPlugins(pluginPaths, false);

  std::vector<std::string> loadOrder;
  for (const auto& pluginPath : pluginPaths) {
    loadOrder.push_back(pluginPath.filename().u8string());
  }

  // Sort plugins into their load order.
  return loot::SortPlugins(*this, loadOrder);
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
  for (const auto& parentPath : additionalDataPaths_) {
    const auto archives = FindArchives(parentPath, archiveFileExtension);
    archivePaths.insert(archives.begin(), archives.end());
  }

  const auto archives = FindArchives(DataPath(), archiveFileExtension);
  archivePaths.insert(archives.begin(), archives.end());

  cache_.CacheArchivePaths(std::move(archivePaths));
}
}
