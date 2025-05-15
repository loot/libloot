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
    case GameType::starfield:
      return LIBLO_GAME_STARFIELD;
    case GameType::openmw:
      return LIBLO_GAME_OPENMW;
    case GameType::oblivionRemastered:
      return LIBLO_GAME_OBLIVION_REMASTERED;
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
  std::string tempPathString = gameLocalAppData.u8string();
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
    logger->trace("Loading the current load order state.");
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
  std::vector<std::string> loadOrder(pluginArr, pluginArr + pluginArrSize);
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
  std::vector<std::string> loadOrder(pluginArr, pluginArr + pluginArrSize);
  lo_free_string_array(pluginArr, pluginArrSize);

  return loadOrder;
}

std::vector<std::string> LoadOrderHandler::GetEarlyLoadingPlugins() const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Getting early loading plugins.");
  }

  char** pluginArr = nullptr;
  size_t pluginArrSize = 0;

  const unsigned int ret =
      lo_get_early_loading_plugins(gh_.get(), &pluginArr, &pluginArrSize);

  HandleError("get early loading plugins", ret);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<std::string> loadOrder(pluginArr, pluginArr + pluginArrSize);
  lo_free_string_array(pluginArr, pluginArrSize);

  return loadOrder;
}

std::filesystem::path LoadOrderHandler::GetActivePluginsFilePath() const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Getting active plugins file path.");
  }

  char* filePathCString = nullptr;

  const unsigned int ret =
      lo_get_active_plugins_file_path(gh_.get(), &filePathCString);

  HandleError("get active plugins file path", ret);

  const auto filePath = std::filesystem::u8path(std::string_view(filePathCString));

  lo_free_string(filePathCString);

  return filePath;
}

std::vector<std::filesystem::path> LoadOrderHandler::GetAdditionalDataPaths()
    const {
  const auto logger = getLogger();
  if (logger) {
    logger->trace("Getting additional data paths.");
  }

  char** pathArr = nullptr;
  size_t pathArrSize = 0;

  const unsigned int ret =
      lo_get_additional_plugins_directories(gh_.get(), &pathArr, &pathArrSize);

  HandleError("get additional data paths", ret);

  std::vector<std::filesystem::path> loadOrder;
  for (size_t i = 0; i < pathArrSize; i += 1) {
    loadOrder.push_back(std::filesystem::u8path(std::string_view(pathArr[i])));
  }
  lo_free_string_array(pathArr, pathArrSize);

  return loadOrder;
}

void LoadOrderHandler::SetLoadOrder(
    const std::vector<std::string>& loadOrder) const {
  auto logger = getLogger();
  if (logger) {
    logger->debug("Setting load order:");
    for (const auto& plugin : loadOrder) {
      logger->debug("\t{}", plugin);
    }
  }

  std::vector<const char*> plugins;
  plugins.reserve(loadOrder.size());
  for (const auto& plugin : loadOrder) {
    plugins.push_back(plugin.c_str());
  }

  const unsigned int ret =
      lo_set_load_order(gh_.get(), plugins.data(), plugins.size());

  HandleError("set the load order", ret);

  if (logger) {
    logger->debug("Load order set successfully.");
  }
}

void LoadOrderHandler::SetAdditionalDataPaths(
    const std::vector<std::filesystem::path>& dataPaths) const {
  auto logger = getLogger();
  if (logger) {
    logger->debug("Setting additional data paths:");
    for (const auto& dataPath : dataPaths) {
      logger->debug("\t{}", dataPath.u8string());
    }
  }

  std::vector<std::string> dataPathStrings;
  for (const auto& dataPath : dataPaths) {
    dataPathStrings.push_back(dataPath.u8string());
  }

  std::vector<const char*> dataPathCStrings;
  for (const auto& dataPath : dataPathStrings) {
    dataPathCStrings.push_back(dataPath.c_str());
  }

  const unsigned int ret = lo_set_additional_plugins_directories(
      gh_.get(), dataPathCStrings.data(), dataPathCStrings.size());

  HandleError("set additional data paths", ret);

  if (logger) {
    logger->debug("Additional data paths set successfully.");
  }
}

void LoadOrderHandler::HandleError(std::string_view operation,
                                   unsigned int returnCode) const {
  if (returnCode == LIBLO_OK || returnCode == LIBLO_WARN_LO_MISMATCH) {
    return;
  }

  const char* message = nullptr;
  std::string err;
  lo_get_error_message(&message);
  if (message == nullptr) {
    err = fmt::format(
        "Failed to {}. libloadorder error code: {}", operation, returnCode);
  } else {
    err = fmt::format("Failed to {}. Details: {}", operation, message);
  }

  auto logger = getLogger();
  if (logger) {
    logger->error(err);
  }

  throw std::runtime_error(err);
}
}
