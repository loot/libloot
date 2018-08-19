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

#include "api/game/load_order_handler.h"

#include "api/helpers/logging.h"
#include "loot/exception/error_categories.h"

using std::string;

namespace loot {
unsigned int mapGameId(GameType gameType) {
  switch (gameType) {
  case GameType::tes4:
    return LIBLO_GAME_TES4;
  case GameType::tes5:
    return LIBLO_GAME_TES5;
  case GameType::tes5se:
    return LIBLO_GAME_TES5SE;
  case GameType::tes5vr:
    return LIBLO_GAME_TES5VR;
  case GameType::fo3:
    return LIBLO_GAME_FO3;
  case GameType::fonv:
    return LIBLO_GAME_FNV;
  case GameType::fo4:
    return LIBLO_GAME_FO4;
  case GameType::fo4vr:
    return LIBLO_GAME_FO4VR;
  default:
    return (unsigned int)-1;
  }
}

LoadOrderHandler::LoadOrderHandler() : gh_(nullptr) {}

LoadOrderHandler::~LoadOrderHandler() { lo_destroy_handle(gh_); }

void LoadOrderHandler::Init(const GameType& gameType,
                            const boost::filesystem::path& gamePath,
                            const boost::filesystem::path& gameLocalAppData) {
  if (gamePath.empty()) {
    throw std::invalid_argument("Game path is not initialised.");
  }

  const char* gameLocalDataPath = nullptr;
  string tempPathString = gameLocalAppData.string();
  if (!tempPathString.empty())
    gameLocalDataPath = tempPathString.c_str();

  // If the handle has already been initialised, close it and open another.
  if (gh_ != nullptr) {
    lo_destroy_handle(gh_);
    gh_ = nullptr;
  }

  int ret = lo_create_handle(
    &gh_, mapGameId(gameType), gamePath.string().c_str(), gameLocalDataPath);

  HandleError("create a game handle", ret);
}

void LoadOrderHandler::LoadCurrentState() {
  auto logger = getLogger();
  if (logger) {
    logger->debug("Loading the current load order state.");
  }

  unsigned int ret = lo_load_current_state(gh_);

  HandleError("load the current load order state", ret);
}

bool LoadOrderHandler::IsPluginActive(const std::string& pluginName) const {
  auto logger = getLogger();
  if (logger) {
    logger->debug("Checking if plugin \"{}\" is active.", pluginName);
  }

  bool result = false;
  unsigned int ret = lo_get_plugin_active(gh_, pluginName.c_str(), &result);

  HandleError("check if a plugin is active", ret);

  return result;
}

std::vector<std::string> LoadOrderHandler::GetLoadOrder() const {
  auto logger = getLogger();
  if (logger) {
    logger->debug("Getting load order.");
  }

  char** pluginArr;
  size_t pluginArrSize;

  unsigned int ret = lo_get_load_order(gh_, &pluginArr, &pluginArrSize);

  HandleError("get the load order", ret);

  std::vector<string> loadOrder(pluginArr, pluginArr + pluginArrSize);
  lo_free_string_array(pluginArr, pluginArrSize);

  return loadOrder;
}

std::vector<std::string> LoadOrderHandler::GetImplicitlyActivePlugins() const {
  auto logger = getLogger();
  if (logger) {
    logger->debug("Getting implicitly active plugins.");
  }

  char** pluginArr;
  size_t pluginArrSize;

  unsigned int ret =
      lo_get_implicitly_active_plugins(gh_, &pluginArr, &pluginArrSize);

  HandleError("get implicitly active plugins", ret);
  
  std::vector<string> loadOrder(pluginArr, pluginArr + pluginArrSize);
  lo_free_string_array(pluginArr, pluginArrSize);

  return loadOrder;
}

void LoadOrderHandler::SetLoadOrder(
    const std::vector<std::string>& loadOrder) const {
  auto logger = getLogger();
  if (logger) {
    logger->info("Setting load order.");
  }

  size_t pluginArrSize = loadOrder.size();
  char** pluginArr = new char*[pluginArrSize];
  int i = 0;
  for (const auto& plugin : loadOrder) {
    if (logger) {
      logger->info("\t\t{}", plugin);
    }

    pluginArr[i] = new char[plugin.length() + 1];
    strcpy(pluginArr[i], plugin.c_str());
    ++i;
  }

  unsigned int ret = lo_set_load_order(gh_, pluginArr, pluginArrSize);

  for (size_t i = 0; i < pluginArrSize; i++) delete[] pluginArr[i];
  delete[] pluginArr;

  HandleError("set the load order", ret);

  if (logger) {
    logger->info("Load order set successfully.");
  }
}

void LoadOrderHandler::HandleError(const std::string& operation,
                                   unsigned int returnCode) const {
  if (returnCode == LIBLO_OK || returnCode == LIBLO_WARN_LO_MISMATCH) {
    return;
  }

  const char* e = nullptr;
  string err;
  lo_get_error_message(&e);
  if (e == nullptr) {
    err = "libloadorder failed to " + operation +
          ". Details could not be fetched.";
  } else {
    err = "libloadorder failed to " + operation + ". Details: " + e;
  }

  auto logger = getLogger();
  if (logger) {
    logger->error(err);
  }

  throw std::system_error(returnCode, libloadorder_category(), err);
}
}
