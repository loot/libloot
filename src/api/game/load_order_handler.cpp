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
    case GameType::tes3:
      return LIBLO_GAME_TES3;
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
      throw std::logic_error("Unexpected game type");
  }
}

LoadOrderHandler::LoadOrderHandler(
    const GameType& gameType,
    const std::filesystem::path& gamePath,
    const std::filesystem::path& gameLocalAppData) :
    gh_(std::unique_ptr<std::remove_pointer<lo_game_handle>::type,
                        decltype(&lo_destroy_handle)>(nullptr,
                                                      lo_destroy_handle)) {
  if (gamePath.empty()) {
    throw std::invalid_argument("Game path is not initialised.");
  }

  const char* gameLocalDataPath = nullptr;
  string tempPathString = gameLocalAppData.u8string();
  if (!tempPathString.empty())
    gameLocalDataPath = tempPathString.c_str();

  lo_game_handle handle = nullptr;

  int ret = lo_create_handle(&handle,
                             mapGameId(gameType),
                             gamePath.u8string().c_str(),
                             gameLocalDataPath);

  HandleError("create a game handle", ret);

  gh_ =
      std::unique_ptr<std::remove_pointer<lo_game_handle>::type,
                      decltype(&lo_destroy_handle)>(handle, lo_destroy_handle);
}

void LoadOrderHandler::LoadCurrentState() {
  auto logger = getLogger();
  if (logger) {
    logger->info("Loading the current load order state.");
  }

  const unsigned int ret = lo_load_current_state(gh_.get());

  HandleError("load the current load order state", ret);
}

bool LoadOrderHandler::IsAmbiguous() const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Checking if the load order is ambiguous.");
  }

  bool result = false;
  const unsigned int ret = lo_is_ambiguous(gh_.get(), &result);

  HandleError("check if the load order is ambiguous", ret);

  return result;
}

bool LoadOrderHandler::IsPluginActive(const std::string& pluginName) const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Checking if plugin \"{}\" is active.", pluginName);
  }

  bool result = false;
  const unsigned int ret =
      lo_get_plugin_active(gh_.get(), pluginName.c_str(), &result);

  HandleError("check if a plugin is active", ret);

  return result;
}

std::vector<std::string> LoadOrderHandler::GetLoadOrder() const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Getting load order.");
  }

  char** pluginArr = nullptr;
  size_t pluginArrSize = 0;

  const unsigned int ret =
      lo_get_load_order(gh_.get(), &pluginArr, &pluginArrSize);

  HandleError("get the load order", ret);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<string> loadOrder(pluginArr, pluginArr + pluginArrSize);
  lo_free_string_array(pluginArr, pluginArrSize);

  return loadOrder;
}

std::vector<std::string> LoadOrderHandler::GetActivePlugins() const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Getting active plugins.");
  }

  char** pluginArr = nullptr;
  size_t pluginArrSize = 0;

  const unsigned int ret =
      lo_get_active_plugins(gh_.get(), &pluginArr, &pluginArrSize);

  HandleError("get active plugins", ret);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<string> loadOrder(pluginArr, pluginArr + pluginArrSize);
  lo_free_string_array(pluginArr, pluginArrSize);

  return loadOrder;
}

std::vector<std::string> LoadOrderHandler::GetImplicitlyActivePlugins() const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Getting implicitly active plugins.");
  }

  char** pluginArr = nullptr;
  size_t pluginArrSize = 0;

  const unsigned int ret =
      lo_get_implicitly_active_plugins(gh_.get(), &pluginArr, &pluginArrSize);

  HandleError("get implicitly active plugins", ret);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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

  std::vector<const char*> plugins;
  plugins.reserve(loadOrder.size());
  for (const auto& plugin : loadOrder) {
    if (logger) {
      logger->info("\t\t{}", plugin);
    }

    plugins.push_back(plugin.c_str());
  }

  const unsigned int ret =
      lo_set_load_order(gh_.get(), plugins.data(), plugins.size());

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
