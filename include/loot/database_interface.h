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
#ifndef LOOT_DATABASE_INTERFACE
#define LOOT_DATABASE_INTERFACE

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "loot/exception/cyclic_interaction_error.h"
#include "loot/metadata/group.h"
#include "loot/metadata/message.h"
#include "loot/metadata/plugin_metadata.h"

namespace loot {
/** @brief The interface provided by API's database handle. */
class DatabaseInterface {
public:
  virtual ~DatabaseInterface() = default;

  /**
   * @name Data Reading & Writing
   * @{
   */

  /**
   * @brief Loads the masterlist, userlist and masterlist prelude from the
   *        paths specified.
   * @details Can be called multiple times, each time replacing the
   *          previously-loaded data.
   * @param masterlist_path
   *        The relative or absolute path to the masterlist file that should be
   *        loaded.
   * @param userlist_path
   *        The relative or absolute path to the userlist file that should be
   *        loaded, or an empty path. If an empty path, no userlist will be
   *        loaded.
   * @param masterlist_prelude_path
   *        The relative or absolute path to the masterlist prelude file that
   *        should be loaded. If an empty path, no masterlist prelude will be
   *        loaded.
   */
  virtual void LoadLists(
      const std::filesystem::path& masterlist_path,
      const std::filesystem::path& userlist_path = "",
      const std::filesystem::path& masterlist_prelude_path = "") = 0;

  /**
   * Writes a metadata file containing all loaded user-added metadata.
   * @param outputFile
   *        The path to which the file shall be written.
   * @param overwrite
   *        If `false` and `outputFile` already exists, no data will be
   *        written. Otherwise, data will be written.
   */
  virtual void WriteUserMetadata(const std::filesystem::path& outputFile,
                                 const bool overwrite) const = 0;

  /**
   * @brief Writes a minimal metadata file that only contains plugins with
   *        Bash Tag suggestions and/or dirty info, plus the suggestions and
   *        info themselves.
   * @param outputFile
   *        The path to which the file shall be written.
   * @param overwrite
   *        If `false` and `outputFile` already exists, no data will be
   *        written. Otherwise, data will be written.
   */
  virtual void WriteMinimalList(const std::filesystem::path& outputFile,
                                const bool overwrite) const = 0;

  /**
   * @}
   * @name Non-plugin Data Access
   * @{
   */

  /**
   * @brief Gets the Bash Tags that are listed in the loaded metadata lists.
   * @details Bash Tag suggestions can include plugins not in this list.
   * @returns A set of Bash Tag names.
   */
  virtual std::vector<std::string> GetKnownBashTags() const = 0;

  /**
   * @brief Get all general messages listen in the loaded metadata lists.
   * @param evaluateConditions
   *        If true, any metadata conditions are evaluated before the metadata
   *        is returned, otherwise unevaluated metadata is returned. Evaluating
   *        general message conditions also clears the condition cache before
   *        evaluating conditions.
   * @returns A vector of messages supplied in the metadata lists but not
   *          attached to any particular plugin.
   */
  virtual std::vector<Message> GetGeneralMessages(
      bool evaluateConditions = false) const = 0;

  /**
   * @brief Gets the groups that are defined in the loaded metadata lists.
   * @param includeUserMetadata
   *        If true, any group metadata present in the userlist is included in
   *        the returned metadata, otherwise the metadata returned only includes
   *        metadata from the masterlist.
   * @returns An vector of Group objects. Each Group's name is unique, if a
   *          group has masterlist and user metadata the two are merged into a
   *          single group object.
   */
  virtual std::vector<Group> GetGroups(
      bool includeUserMetadata = true) const = 0;

  /**
   * @brief Gets the groups that are defined or extended in the loaded userlist.
   * @returns An unordered set of Group objects.
   */
  virtual std::vector<Group> GetUserGroups() const = 0;

  /**
   * @brief Sets the group definitions to store in the userlist, overwriting any
   *        existing definitions there.
   * @param groups
   *        The unordered set of Group objects to set.
   */
  virtual void SetUserGroups(const std::vector<Group>& groups) = 0;

  /**
   * @brief Get the "shortest" path between the two given groups according to
   *        their load after metadata.
   * @details The "shortest" path is defined as the path that maximises the
   *          amount of user metadata involved while minimising the amount of
   *          masterlist metadata involved. It's not the path involving the
   *          fewest groups.
   * @param fromGroupName
   *        The name of the source group, that loads earlier.
   * @param toGroupName
   *        The name of the destination group, that loads later.
   * @returns A vector of Vertex elements representing the path from the source
   *          group to the destination group, or an empty vector if no path
   *          exists.
   */
  virtual std::vector<Vertex> GetGroupsPath(
      const std::string& fromGroupName,
      const std::string& toGroupName) const = 0;

  /**
   * @}
   * @name Plugin Data Access
   * @{
   */

  /**
   * @brief Get all a plugin's loaded metadata.
   * @param plugin
   *        The filename of the plugin to look up metadata for.
   * @param includeUserMetadata
   *        If true, any user metadata the plugin has is included in the
   *        returned metadata, otherwise the metadata returned only includes
   *        metadata from the masterlist.
   * @param evaluateConditions
   *        If true, any metadata conditions are evaluated before the metadata
   *        is returned, otherwise unevaluated metadata is returned. Evaluating
   *        plugin metadata conditions does not clear the condition cache.
   * @returns If the plugin has metadata, an optional containing that metadata,
   *          otherwise an optional containing no value.
   */
  virtual std::optional<PluginMetadata> GetPluginMetadata(
      const std::string& plugin,
      bool includeUserMetadata = true,
      bool evaluateConditions = false) const = 0;

  /**
   * @brief Get a plugin's metadata loaded from the given userlist.
   * @param plugin
   *        The filename of the plugin to look up user-added metadata for.
   * @param evaluateConditions
   *        If true, any metadata conditions are evaluated before the metadata
   *        is returned, otherwise unevaluated metadata is returned. Evaluating
   *        plugin metadata conditions does not clear the condition cache.
   * @returns If the plugin has user-added metadata, an optional containing
   *          that metadata, otherwise an optional containing no value.
   */
  virtual std::optional<PluginMetadata> GetPluginUserMetadata(
      const std::string& plugin,
      bool evaluateConditions = false) const = 0;

  /**
   * @brief Sets a plugin's user metadata, overwriting any existing user
   *        metadata.
   * @param pluginMetadata
   *        The user metadata you want to set, with plugin.Name() being the
   *        filename of the plugin the metadata is for.
   */
  virtual void SetPluginUserMetadata(const PluginMetadata& pluginMetadata) = 0;

  /**
   * @brief Discards all loaded user metadata for the plugin with the given
   *        filename.
   * @param plugin
   *        The filename of the plugin for which all user-added metadata
   *        should be deleted.
   */
  virtual void DiscardPluginUserMetadata(const std::string& plugin) = 0;

  /**
   * @brief Discards all loaded user metadata for all plugins, and any
   * user-added general messages and known bash tags.
   */
  virtual void DiscardAllUserMetadata() = 0;

  /** @} */
};
}

#endif
