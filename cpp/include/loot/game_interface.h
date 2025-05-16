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
#ifndef LOOT_GAME_INTERFACE
#define LOOT_GAME_INTERFACE

#include "loot/database_interface.h"
#include "loot/enum/game_type.h"
#include "loot/plugin_interface.h"

namespace loot {
/** @brief The interface provided for accessing game-specific functionality. */
class GameInterface {
public:
  virtual ~GameInterface() = default;

  /**
   * @brief Get the game's type.
   * @returns The game's type.
   */
  virtual GameType GetType() const = 0;

  /**
   * @brief   Gets the currently-set additional data paths.
   * @details The following games are configured with additional data paths by
   *          default:
   *          - Fallout 4, when installed from the Microsoft Store
   *          - Starfield
   *          - OpenMW
   */
  virtual std::vector<std::filesystem::path> GetAdditionalDataPaths() const = 0;

  /**
   * @brief   Set additional data paths.
   * @details The additional data paths are used when interacting with the load
   *          order, evaluating conditions and scanning for archives (BSA/BA2
   *          depending on the game). Additional data paths are used in the
   *          order they are given (except with OpenMW, which checks them in
   *          reverse order), and take precedence over the game's main data
   *          path.
   */
  virtual void SetAdditionalDataPaths(
      const std::vector<std::filesystem::path>& additionalDataPaths) = 0;

  /**
   *  @name Metadata Access
   *  @{
   */

  /**
   * @brief Get the database interface used for accessing metadata-related
   *        functionality.
   * @returns A reference to the game's DatabaseInterface. The reference remains
   *          valid for the lifetime of the GameInterface instance.
   */
  virtual DatabaseInterface& GetDatabase() = 0;

  /**
   * @brief Get the database interface used for accessing metadata-related
   *        functionality.
   * @returns A reference to the game's DatabaseInterface. The reference remains
   *          valid for the lifetime of the GameInterface instance.
   */
  virtual const DatabaseInterface& GetDatabase() const = 0;

  /**
   * @}
   * @name Plugin Data Access
   * @{
   */

  /**
   * @brief Check if a file is a valid plugin.
   * @details The validity check is not exhaustive: it generally checks that the
   *          file is a valid plugin file extension for the game and that its
   *          header (if applicable) can be parsed.
   * @param  pluginPath
   *         The path to the file to check. Relative paths are resolved relative
   *         to the game's plugins directory, while absolute paths are used
   *         as given.
   * @returns True if the file is a valid plugin, false otherwise.
   */
  virtual bool IsValidPlugin(const std::filesystem::path& pluginPath) const = 0;

  /**
   * @brief Parses plugins and loads their data.
   * @details If a given plugin filename (or one that is case-insensitively
   *          equal) has already been loaded, its previously-loaded data
   *          data is discarded, invalidating any existing shared pointers to
   *          that plugin's PluginInterface object.
   *
   *          If the game is Morrowind, OpenMW or Starfield, it's only valid to
   *          fully load a plugin if its masters are already loaded or included
   *          in the same input vector.
   * @param pluginPaths
   *        The plugin paths to load. Relative paths are resolved relative to
   *        the game's plugins directory, while absolute paths are used as
   *        given. Each plugin filename must be unique within the vector.
   * @param loadHeadersOnly
   *        If true, only the plugins' headers are loaded. If false, all records
   *        in the plugins are parsed.
   */
  virtual void LoadPlugins(
      const std::vector<std::filesystem::path>& pluginPaths,
      bool loadHeadersOnly) = 0;

  /**
   * @brief Clears the plugins loaded by previous calls to `LoadPlugins()`.
   * @details This invalidates any PluginInterface pointers retrieved using
   *          `GetPlugin()` or `GetLoadedPlugins()`.
   */
  virtual void ClearLoadedPlugins() = 0;

  /**
   * @brief Get data for a loaded plugin.
   * @param  pluginName
   *         The filename of the plugin to get data for.
   * @returns A shared pointer to a const PluginInterface implementation. The
   *          pointer is null if the given plugin has not been loaded. The
   *          pointer remains valid until the `ClearLoadedPlugins()` function
   *          is called, this GameInterface is destroyed, or until a plugin with
   *          a case-insensitively equal filename is loaded.
   */
  virtual std::shared_ptr<const PluginInterface> GetPlugin(
      std::string_view pluginName) const = 0;

  /**
   * @brief Get a set of const references to all loaded plugins' PluginInterface
   *        objects.
   * @returns A set of shared pointers to const PluginInterface. The pointers
   *          remain valid until the `ClearLoadedPlugins()` function is called,
   *          this GameInterface is destroyed, or until a plugin with a
   *          case-insensitively equal filename is loaded.
   */
  virtual std::vector<std::shared_ptr<const PluginInterface>> GetLoadedPlugins()
      const = 0;

  /**
   *  @}
   *  @name Sorting
   *  @{
   */

  /**
   *  @brief Calculates a new load order for the game's installed plugins
   *         (including inactive plugins) and outputs the sorted order.
   *  @details Pulls metadata from the masterlist and userlist if they are
   *           loaded, and reads the contents of each plugin. No changes are
   *           applied to the load order used by the game. This function does
   *           not load or evaluate the masterlist or userlist.
   *  @param pluginFilenames
   *         The plugins to sort, in their current load order. All given plugins
   *         must have been loaded using `LoadPlugins()`.
   *  @returns A vector of the given plugin filenames in their sorted load
   *           order.
   */
  virtual std::vector<std::string> SortPlugins(
      const std::vector<std::string>& pluginFilenames) = 0;

  /**
   *  @}
   *  @name Load Order Interaction
   *  @{
   */

  /**
   *
   * @brief Load the current load order state, discarding any previously held
   *        state.
   * @details This function should be called whenever the load order or active
   *          state of plugins "on disk" changes, so that the cached state is
   *          updated to reflect the changes.
   */
  virtual void LoadCurrentLoadOrderState() = 0;

  /**
   * @brief Check if the load order is ambiguous.
   * @details This checks that all plugins in the current load order state have
   *          a well-defined position in the "on disk" state, and that all data
   *          sources are consistent. If the load order is ambiguous, different
   *          applications may read different load orders from the same source
   *          data.
   * @returns True if the load order is ambiguous, false otherwise.
   */
  virtual bool IsLoadOrderAmbiguous() const = 0;

  /**
   * @brief Gets the path to the file that holds the list of active plugins.
   * @details The active plugins file path is often within the game's local
              path, but its name and location varies by game and game
              configuration, so this function exposes the path that libloot
              uses.
   * @returns The file path.
   */
  virtual std::filesystem::path GetActivePluginsFilePath() const = 0;

  /**
   * @brief Check if a plugin is active.
   * @param  plugin
   *         The filename of the plugin for which to check the active state.
   * @returns True if the plugin is active, false otherwise.
   */
  virtual bool IsPluginActive(const std::string& plugin) const = 0;

  /**
   * @brief Get the current load order.
   * @returns A vector of plugin filenames in their load order.
   */
  virtual std::vector<std::string> GetLoadOrder() const = 0;

  /**
   * @brief Set the game's load order.
   * @details There is no way to persist the load order of inactive OpenMW
   *          plugins, so setting an OpenMW load order will have no effect if
   *          the relative order of active plugins is unchanged.
   * @param loadOrder
   *        A vector of plugin filenames sorted in the load order to set.
   */
  virtual void SetLoadOrder(const std::vector<std::string>& loadOrder) = 0;
};
}

#endif
