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

#include "loot/api.h"

#include <filesystem>

#include "api/game/game.h"
#include "api/helpers/logging.h"

namespace fs = std::filesystem;

namespace loot {
const char* DescribeGameType(GameType gameType) {
  switch (gameType) {
    case GameType::tes4:
      return "The Elder Scrolls IV: Oblivion";
    case GameType::tes5:
      return "The Elder Scrolls V: Skyrim";
    case GameType::fo3:
      return "Fallout 3";
    case GameType::fonv:
      return "Fallout: New Vegas";
    case GameType::fo4:
      return "Fallout 4";
    case GameType::tes5se:
      return "The Elder Scrolls V: Skyrim Special Edition";
    case GameType::fo4vr:
      return "Fallout 4 VR";
    case GameType::tes5vr:
      return "The Elder Scrolls V: Skyrim VR";
    case GameType::tes3:
      return "The Elder Scrolls III: Morrowind";
    case GameType::starfield:
      return "Starfield";
    case GameType::openmw:
      return "OpenMW";
    default:
      return "Unknown";
  }
}

std::filesystem::path ResolvePath(const std::filesystem::path& path) {
  // is_symlink can throw on MSVC with the message
  // "symlink_status: The parameter is incorrect."
  // even though a perfectly valid (non-symlink) path is given. This has been
  // seen with a non C: drive path, but not reproduced, so just catch the
  // exception and log it.
  try {
    if (fs::is_symlink(path))
      return fs::read_symlink(path);
  } catch (const std::exception& e) {
    auto logger = getLogger();
    if (logger) {
      logger->error("Could not check or read potential symlink path \"{}\": {}",
                    path.u8string(),
                    e.what());
    }
  }

  return path;
}

LOOT_API void SetLoggingCallback(
    std::function<void(LogLevel, std::string_view)> callback) {
  const auto logger = createLogger(callback);

  spdlog::drop(logger->name());
  spdlog::register_logger(logger);
}

LOOT_API void SetLogLevel(LogLevel level) { setLoggerLevel(level); }

LOOT_API bool IsCompatible(const unsigned int versionMajor,
                           const unsigned int versionMinor,
                           const unsigned int) {
  if (versionMajor > 0)
    return versionMajor == LIBLOOT_VERSION_MAJOR;
  else
    return versionMinor == LIBLOOT_VERSION_MINOR;
}

LOOT_API std::unique_ptr<GameInterface> CreateGameHandle(
    const GameType game,
    const std::filesystem::path& gamePath,
    const std::filesystem::path& gameLocalPath) {
  auto logger = getLogger();
  if (logger) {
    logger->info(
        "Attempting to create a game handle for game type \"{}\" with game "
        "path \"{}\" and game local path \"{}\"",
        DescribeGameType(game),
        gamePath.u8string(),
        gameLocalPath.u8string());
  }

  auto resolvedGamePath = ResolvePath(gamePath);
  if (!fs::is_directory(resolvedGamePath)) {
    throw std::invalid_argument("Given game path \"" + gamePath.u8string() +
                                "\" does not resolve to a valid directory.");
  }

  auto resolvedGameLocalPath = ResolvePath(gameLocalPath);
  if (!gameLocalPath.empty() && fs::exists(resolvedGameLocalPath) &&
      !fs::is_directory(resolvedGameLocalPath)) {
    throw std::invalid_argument(
        "Given game local path \"" + gameLocalPath.u8string() +
        "\" resolves to a path that exists but is not a valid directory.");
  }

  return std::make_unique<Game>(game, resolvedGamePath, resolvedGameLocalPath);
}
}
