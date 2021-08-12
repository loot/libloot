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

#include "loot/api_decorator.h"
#include "loot/enum/game_type.h"
#include "loot/enum/log_level.h"
#include "loot/exception/condition_syntax_error.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/error_categories.h"
#include "loot/exception/file_access_error.h"
#include "loot/exception/git_state_error.h"
#include "loot/exception/undefined_group_error.h"
#include "loot/game_interface.h"
#include "loot/loot_version.h"

namespace loot {
/**@}*/
/**********************************************************************//**
 *  @name Logging Functions
 *************************************************************************/
/**@{*/

/**
 * @brief Set the callback function that is called when logging.
 * @details If this function is not called, the default behaviour is to
 *          print messages to the console.
 * @param callback
 *        The function called when logging. The first parameter is the
 *        level of the message being logged, and the second is the message.
 */
LOOT_API void SetLoggingCallback(
    std::function<void(LogLevel, const char*)> callback);

/**@}*/
/**********************************************************************//**
 *  @name Version Functions
 *************************************************************************/
/**@{*/

/**
 *  @brief Checks for API compatibility.
 *  @details Checks whether the loaded API is compatible with the given
 *           version of the API, abstracting API stability policy away from
 *           clients. The version numbering used is major.minor.patch.
 *  @param major
 *         The major version number to check.
 *  @param minor
 *         The minor version number to check.
 *  @param patch
 *         The patch version number to check.
 *  @returns True if the API versions are compatible, false otherwise.
 */
LOOT_API bool IsCompatible(const unsigned int major,
                           const unsigned int minor,
                           const unsigned int patch);

/**@}*/
/**********************************************************************//**
 *  @name Lifecycle Management Functions
 *************************************************************************/
/**@{*/

/**
 *  @brief Initialise a new game handle.
 *  @details Creates a handle for a game, which is then used by all
 *           game-specific functions.
 *  @param game
 *         A game code for which to create the handle.
 *  @param game_path
 *         The relative or absolute path to the directory containing the
 *         game's executable.
 *  @param game_local_path
 *         The relative or absolute path to the game's folder in
 *         `%%LOCALAPPDATA%` or an empty path. If an empty path, the API will
 *         attempt to look up the path that `%%LOCALAPPDATA%` corresponds to.
 *         This parameter is provided so that systems lacking that environmental
 *         variable (eg. Linux) can still use the API.
 *  @returns The new game handle.
 */
LOOT_API std::shared_ptr<GameInterface> CreateGameHandle(
    const GameType game,
    const std::filesystem::path& game_path,
    const std::filesystem::path& game_local_path = "");


/**@}*/
/**********************************************************************/ /**
  *  @name File Versioning Functions
  *************************************************************************/
/**@{*/

/**
 *  @brief Update the given masterlist or masterlist prelude file.
 *  @details Uses Git to update the given file using a given remote.
 *           If the file doesn't exist, this will create it. This
 *           function also initialises a Git repository in the given
 *           file's parent folder.
 *
 *           If a Git repository is already present, it will be used to
 *           perform a diff-only update, but if for any reason a
 *           fast-forward merge update is not possible, the existing
 *           repository will be deleted and a new repository cloned from
 *           the given remote.
 *  @param file_path
 *         The relative or absolute path to the file that should be
 *         updated. The filename must match the filename of the file in the
 *         given remote repository, otherwise it will not be updated
 *         correctly. The file must be present in the repository's root
 *         directory.
 *  @param remote_url
 *         The URL of the remote from which to fetch updates. This can also be
 *         a relative or absolute path to a local repository.
 *  @param remote_branch
 *         The branch of the remote from which to apply updates.
 *  @returns `true` if the file was updated. `false` if no update was
 *           necessary, ie. it was already up-to-date.
 */
LOOT_API bool UpdateFile(const std::filesystem::path& file_path,
                               const std::string& remote_url,
                               const std::string& remote_branch);

/**
 *  @brief Get the given masterlist or masterlist prelude file's revision.
 *  @details Getting a file's revision is only possible if it is found
 *           in the root of a local Git repository.
 *  @param file_path
 *         The relative or absolute path to the file that should be queried.
 *  @param get_short_id
 *         If `true`, the shortest unique hexadecimal revision hash that is at
 *         least 7 characters long will be outputted. Otherwise, the full 40
 *         character hash will be outputted.
 *  @returns The revision data.
 */
LOOT_API FileRevision GetFileRevision(const std::filesystem::path& file_path,
                      const bool get_short_id);

/**
 * Check if the given masterlist or masterlist prelude file is the latest
 * available for a given branch.
 * @param  file_path
 *         The relative or absolute path to the file for which the latest
 *         revision should be obtained. It needs to be in the root of a local
 *         Git repository.
 * @param  branch
 *         The branch to check against.
 * @return True if the file's current revision matches its latest revision
 *         for the given branch, and false otherwise.
 */
LOOT_API bool IsLatestFile(const std::filesystem::path& file_path,
                                 const std::string& branch);

}

#endif
