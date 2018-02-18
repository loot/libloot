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

#include <boost/filesystem.hpp>
#include <boost/locale.hpp>

#include "api/game/game.h"
#include "api/helpers/logging.h"

namespace fs = boost::filesystem;

namespace loot {
std::string ResolvePath(const std::string& path) {
  // NTFS junction links show up as symlinks and directories, but resolving
  // them just appends their target path.
  if (path.empty() || !fs::is_symlink(path) || fs::is_directory(path))
    return path;

  return fs::read_symlink(path).string();
}

LOOT_API void SetLoggingCallback(
    std::function<void(LogLevel, const char*)> callback) {
  auto sink = std::make_shared<SpdLoggingSink>(callback);
  auto logger = std::make_shared<spdlog::logger>(LOGGER_NAME, sink);
  logger->set_level(spdlog::level::level_enum::trace);

  spdlog::drop(LOGGER_NAME);
  spdlog::register_logger(logger);
}

LOOT_API bool IsCompatible(const unsigned int versionMajor,
                           const unsigned int versionMinor,
                           const unsigned int versionPatch) {
  if (versionMajor > 0)
    return versionMajor == loot::LootVersion::major;
  else
    return versionMinor == loot::LootVersion::minor;
}

LOOT_API void InitialiseLocale(const std::string& id) {
  std::locale::global(boost::locale::generator().generate(id));
  boost::filesystem::path::imbue(std::locale());
}

LOOT_API std::shared_ptr<GameInterface> CreateGameHandle(
    const GameType game,
    const std::string& gamePath,
    const std::string& gameLocalPath) {
  auto logger = getLogger();
  if (logger) {
    logger->info(
        "Attempting to create a game handle with game path \"{}\" "
        "and local path \"{}\"",
        gamePath,
        gameLocalPath);
  }

  const std::string resolvedGamePath = ResolvePath(gamePath);
  if (!fs::is_directory(resolvedGamePath))
    throw std::invalid_argument("Given game path \"" + gamePath +
                                "\" does not resolve to a valid directory.");

  const std::string resolvedGameLocalPath = ResolvePath(gameLocalPath);
  if (!gameLocalPath.empty() && !fs::is_directory(resolvedGameLocalPath))
    throw std::invalid_argument("Given game path \"" + gameLocalPath +
                                "\" does not resolve to a valid directory.");

  return std::make_shared<Game>(game, resolvedGamePath, resolvedGameLocalPath);
}
}
