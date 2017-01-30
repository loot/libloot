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

#include "gui/state/game.h"

#include <algorithm>
#include <thread>
#include <cmath>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/locale.hpp>
#include <boost/log/trivial.hpp>

#include "loot/exception/file_access_error.h"
#include "loot/exception/game_detection_error.h"
#include "loot/windows_encoding_converters.h"

#ifdef _WIN32
#   ifndef UNICODE
#       define UNICODE
#   endif
#   ifndef _UNICODE
#      define _UNICODE
#   endif
#   define NOMINMAX
#   include "windows.h"
#   include "shlobj.h"
#   include "shlwapi.h"
#endif

using std::list;
using std::string;
using std::thread;
using std::vector;

namespace fs = boost::filesystem;

namespace loot {
namespace gui {
Game::Game(const GameSettings& gameSettings,
           const boost::filesystem::path& lootDataPath,
           const boost::filesystem::path& localDataPath) :
  GameSettings(gameSettings),
  lootDataPath_(lootDataPath),
  gameHandle_(CreateGameHandle(gameSettings.Type(), gameSettings.GamePath().string(), localDataPath.string())),
  pluginsFullyLoaded_(false) {
  gameHandle_->IdentifyMainMasterFile(gameSettings.Master());
}

bool Game::IsInstalled(const GameSettings& gameSettings) {
  auto gamePath = DetectGamePath(gameSettings);

  return !gamePath.empty();
}

void Game::Init() {
  BOOST_LOG_TRIVIAL(info) << "Initialising filesystem-related data for game: " << Name();

  SetGamePath(DetectGamePath(*this));

  if (GamePath().empty()) {
    throw GameDetectionError("Game path could not be detected.");
  }

  if (!lootDataPath_.empty()) {
      //Make sure that the LOOT game path exists.
    try {
      if (!fs::exists(lootDataPath_ / FolderName()))
        fs::create_directories(lootDataPath_ / FolderName());
    } catch (fs::filesystem_error& e) {
      throw FileAccessError((boost::format("Could not create LOOT folder for game. Details: %1%") % e.what()).str());
    }
  }
}

std::shared_ptr<const PluginInterface> Game::GetPlugin(const std::string & name) const {
  return gameHandle_->GetPlugin(name);
}

std::set<std::shared_ptr<const PluginInterface>> Game::GetPlugins() const {
  return gameHandle_->GetLoadedPlugins();
}

void Game::RedatePlugins() {
  if (Type() != GameType::tes5 && Type() != GameType::tes5se) {
    BOOST_LOG_TRIVIAL(warning) << "Cannot redate plugins for game " << Name();
    return;
  }

  vector<string> loadorder = gameHandle_->GetLoadOrder();
  if (!loadorder.empty()) {
    time_t lastTime = 0;
    for (const auto &pluginName : loadorder) {
      fs::path filepath = DataPath() / pluginName;
      if (!fs::exists(filepath)) {
        if (fs::exists(filepath.string() + ".ghost"))
          filepath += ".ghost";
        else
          continue;
      }

      time_t thisTime = fs::last_write_time(filepath);
      BOOST_LOG_TRIVIAL(info) << "Current timestamp for \"" << filepath.filename().string() << "\": " << thisTime;
      if (thisTime >= lastTime) {
        lastTime = thisTime;
        BOOST_LOG_TRIVIAL(trace) << "No need to redate \"" << filepath.filename().string() << "\".";
      } else {
        lastTime += 60;
        fs::last_write_time(filepath, lastTime);  //Space timestamps by a minute.
        BOOST_LOG_TRIVIAL(info) << "Redated \"" << filepath.filename().string() << "\" to: " << lastTime;
      }
    }
  }
}

void Game::LoadAllInstalledPlugins(bool headersOnly) {
  std::vector<std::string> plugins;

  BOOST_LOG_TRIVIAL(trace) << "Scanning for plugins in " << this->DataPath();
  for (fs::directory_iterator it(this->DataPath()); it != fs::directory_iterator(); ++it) {
    if (fs::is_regular_file(it->status()) && gameHandle_->IsValidPlugin(it->path().filename().string())) {
      string name = it->path().filename().string();
      BOOST_LOG_TRIVIAL(info) << "Found plugin: " << name;

      plugins.push_back(name);
    }
  }

  gameHandle_->LoadPlugins(plugins, headersOnly);

  pluginsFullyLoaded_ = !headersOnly;
}

bool Game::ArePluginsFullyLoaded() const {
  return pluginsFullyLoaded_;
}

boost::filesystem::path Game::DataPath() const {
  if (GamePath().empty())
    throw std::logic_error("Cannot get data path from empty game path");
  return GamePath() / "Data";
}

fs::path Game::MasterlistPath() const {
  return lootDataPath_ / FolderName() / "masterlist.yaml";
}

fs::path Game::UserlistPath() const {
  return lootDataPath_ / FolderName() / "userlist.yaml";
}

std::vector<std::string> Game::GetLoadOrder() const {
  return gameHandle_->GetLoadOrder();
}

void Game::SetLoadOrder(const std::vector<std::string>& loadOrder) {
  BackupLoadOrder(GetLoadOrder(), lootDataPath_ / FolderName());
  gameHandle_->SetLoadOrder(loadOrder);
}

short Game::GetActiveLoadOrderIndex(const std::string & pluginName) const {
  return GetActiveLoadOrderIndex(pluginName, gameHandle_->GetLoadOrder());
}

short Game::GetActiveLoadOrderIndex(const std::string & pluginName, const std::vector<std::string>& loadOrder) const {
  // Get the full load order, then count the number of active plugins until the
  // given plugin is encountered. If the plugin isn't active or in the load
  // order, return -1.

  if (!gameHandle_->IsPluginActive(pluginName))
    return -1;

  short numberOfActivePlugins = 0;
  for (const std::string& plugin : loadOrder) {
    if (boost::iequals(plugin, pluginName))
      return numberOfActivePlugins;

    if (gameHandle_->IsPluginActive(plugin))
      ++numberOfActivePlugins;
  }

  return -1;
}

boost::filesystem::path Game::DetectGamePath(const GameSettings & gameSettings) {
  try {
    BOOST_LOG_TRIVIAL(trace) << "Checking if game \"" << gameSettings.Name() << "\" is installed.";
    if (!gameSettings.GamePath().empty() && fs::exists(gameSettings.GamePath() / "Data" / gameSettings.Master()))
      return gameSettings.GamePath();

    if (fs::exists(fs::path("..") / "Data" / gameSettings.Master())) {
      return "..";
    }

#ifdef _WIN32
    std::string path;
    std::string key_parent = fs::path(gameSettings.RegistryKey()).parent_path().string();
    std::string key_name = fs::path(gameSettings.RegistryKey()).filename().string();
    path = RegKeyStringValue("HKEY_LOCAL_MACHINE", key_parent, key_name);
    if (!path.empty() && fs::exists(fs::path(path) / "Data" / gameSettings.Master())) {
      return path;
    }
#endif
  } catch (std::exception &e) {
    BOOST_LOG_TRIVIAL(error) << "Error while checking if game \"" << gameSettings.Name() << "\" is installed: " << e.what();
  }

  return boost::filesystem::path();
}

void Game::BackupLoadOrder(const std::vector<std::string>& loadOrder,
                           const boost::filesystem::path& backupDirectory) {
  const int maxBackupIndex = 2;
  boost::format filenameFormat = boost::format("loadorder.bak.%1%");

  boost::filesystem::path backupFilePath = backupDirectory / (filenameFormat % 2).str();
  if (boost::filesystem::exists(backupFilePath))
    boost::filesystem::remove(backupFilePath);

  for (int i = maxBackupIndex - 1; i > -1; --i) {
    const boost::filesystem::path backupFilePath = backupDirectory / (filenameFormat % i).str();
    if (boost::filesystem::exists(backupFilePath))
      boost::filesystem::rename(backupFilePath, backupDirectory / (filenameFormat % (i + 1)).str());
  }

  boost::filesystem::ofstream out(backupDirectory / (filenameFormat % 0).str());
  for (const auto &plugin : loadOrder)
    out << plugin << std::endl;
}

#ifdef _WIN32
std::string Game::RegKeyStringValue(const std::string& keyStr, const std::string& subkey, const std::string& value) {
  HKEY hKey = NULL;
  DWORD len = MAX_PATH;
  std::wstring wstr(MAX_PATH, 0);

  if (keyStr == "HKEY_CLASSES_ROOT")
    hKey = HKEY_CLASSES_ROOT;
  else if (keyStr == "HKEY_CURRENT_CONFIG")
    hKey = HKEY_CURRENT_CONFIG;
  else if (keyStr == "HKEY_CURRENT_USER")
    hKey = HKEY_CURRENT_USER;
  else if (keyStr == "HKEY_LOCAL_MACHINE")
    hKey = HKEY_LOCAL_MACHINE;
  else if (keyStr == "HKEY_USERS")
    hKey = HKEY_USERS;
  else
    throw std::invalid_argument("Invalid registry key given.");

  BOOST_LOG_TRIVIAL(trace) << "Getting string for registry key, subkey and value: " << keyStr << " + " << subkey << " + " << value;
  LONG ret = RegGetValue(hKey,
                         ToWinWide(subkey).c_str(),
                         ToWinWide(value).c_str(),
                         RRF_RT_REG_SZ | KEY_WOW64_32KEY,
                         NULL,
                         &wstr[0],
                         &len);

  if (ret == ERROR_SUCCESS) {
    BOOST_LOG_TRIVIAL(info) << "Found string: " << wstr.c_str();
    return FromWinWide(wstr.c_str());  // Passing c_str() cuts off any unused buffer.
  } else {
    BOOST_LOG_TRIVIAL(info) << "Failed to get string value.";
    return "";
  }
}
#endif
}
}