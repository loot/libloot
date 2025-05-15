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

#ifndef LOOT_API_H
#define LOOT_API_H

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "loot/api_decorator.h"
#include "loot/enum/game_type.h"
#include "loot/enum/log_level.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/plugin_not_loaded_error.h"
#include "loot/exception/undefined_group_error.h"
#include "loot/game_interface.h"
#include "loot/loot_version.h"

namespace loot {
/**
 * @name Logging Functions
 * @{
 */

/**
 * @brief Set the callback function that is called when logging.
 * @param callback
 *        The function called when logging. The first parameter is the
 *        level of the message being logged, and the second is the message.
 */
LOOT_API void SetLoggingCallback(
    std::function<void(LogLevel, std::string_view)> callback);

/**
 * @brief Set the log severity level.
 * @details The default level setting is trace. This function has no effect if
 *          no logging callback has been set.
 * @param level
 *        Messages of this severity level and higher will be logged.
 */
LOOT_API void SetLogLevel(LogLevel level);

/**
 * @}
 * @name Version Functions
 * @{
 */

/**
 * @brief Checks for API compatibility.
 * @details Checks whether the loaded API is compatible with the given
 *          version of the API, abstracting API stability policy away from
 *          clients. The version numbering used is major.minor.patch.
 * @param major
 *        The major version number to check.
 * @param minor
 *        The minor version number to check.
 * @param patch
 *        The patch version number to check.
 * @returns True if the API versions are compatible, false otherwise.
 */
LOOT_API bool IsCompatible(const unsigned int major,
                           const unsigned int minor,
                           const unsigned int patch);

/**
 * @}
 * @name Lifecycle Management Functions
 * @{
 */

/**
 * @brief Initialise a new game handle.
 * @details Creates a handle for a game, which is then used by all
 *          game-specific functions.
 * @param game
 *        A game code for which to create the handle.
 * @param game_path
 *        The relative or absolute path to the directory containing the
 *        game's executable.
 * @param game_local_path
 *        The relative or absolute path to the game's local data folder, or an
 *        empty path. The local data folder is usually in `%%LOCALAPPDATA%`, but
 *        Morrowind has no local data folder and OpenMW's is in the user's
 *        My Games folder on Windows and in `$HOME/.config` on Linux. If an
 *        empty path is provided, the API will attempt to look up the relevant
 *        local data path, which may fail in some situations (e.g. when running
 *        libloot natively on Linux for a game other than Morrowind or OpenMW).
 * @returns The new game handle.
 */
LOOT_API std::unique_ptr<GameInterface> CreateGameHandle(
    const GameType game,
    const std::filesystem::path& game_path,
    const std::filesystem::path& game_local_path = "");
}

#endif
