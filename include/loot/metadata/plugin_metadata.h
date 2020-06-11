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
#ifndef LOOT_METADATA_PLUGIN_METADATA
#define LOOT_METADATA_PLUGIN_METADATA

#include <cstdint>
#include <list>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "loot/api_decorator.h"
#include "loot/metadata/file.h"
#include "loot/metadata/location.h"
#include "loot/metadata/message.h"
#include "loot/metadata/plugin_cleaning_data.h"
#include "loot/metadata/tag.h"

namespace loot {
/**
 * Represents a plugin's metadata.
 */
class PluginMetadata {
public:
  /**
   * Construct a PluginMetadata object with a blank plugin name and no metadata.
   * @return A PluginMetadata object.
   */
  LOOT_API explicit PluginMetadata();

  /**
   * Construct a PluginMetadata object with no metadata for a plugin with the
   * given filename.
   * @param  name
   *         The filename of the plugin that the object is constructed for.
   * @return A PluginMetadata object.
   */
  LOOT_API explicit PluginMetadata(const std::string& name);

  /**
   * Merge metadata from the given PluginMetadata object into this object.
   *
   * If an equal metadata object already exists in this PluginMetadata object,
   * it is not duplicated. This object's group is replaced by the given object's
   * group if the latter is explicit.
   * @param plugin
   *        The plugin metadata to merge.
   */
  LOOT_API void MergeMetadata(const PluginMetadata& plugin);

  /**
   * Get metadata in this object that isn't present in the given PluginMetadata
   * object.
   * @param  plugin
   *         The PluginMetadata object to compare against.
   * @return A PluginMetadata object containing the metadata in this object that
   *         is not in the given object. The returned object inherits this
   *         object's group.
   */
  LOOT_API PluginMetadata NewMetadata(const PluginMetadata& plugin) const;

  /**
   * Get the plugin name.
   * @return The plugin name.
   */
  LOOT_API std::string GetName() const;

  /**
   * Get the plugin's group.
   * @return An optional containing the name of the group this plugin belongs to
   *         if it was explicitly set, otherwise an optional containing no
   *         value.
   */
  LOOT_API std::optional<std::string> GetGroup() const;

  /**
   * Get the plugins that the plugin must load after.
   * @return The plugins that the plugin must load after.
   */
  LOOT_API std::vector<File> GetLoadAfterFiles() const;

  /**
   * Get the files that the plugin requires to be installed.
   * @return The files that the plugin requires to be installed.
   */
  LOOT_API std::vector<File> GetRequirements() const;

  /**
   * Get the files that the plugin is incompatible with.
   * @return The files that the plugin is incompatible with.
   */
  LOOT_API std::vector<File> GetIncompatibilities() const;

  /**
   * Get the plugin's messages.
   * @return The plugin's messages.
   */
  LOOT_API std::vector<Message> GetMessages() const;

  /**
   * Get the plugin's Bash Tag suggestions.
   * @return The plugin's Bash Tag suggestions.
   */
  LOOT_API std::vector<Tag> GetTags() const;

  /**
   * Get the plugin's dirty plugin information.
   * @return The PluginCleaningData objects that identify the plugin as dirty.
   */
  LOOT_API std::vector<PluginCleaningData> GetDirtyInfo() const;

  /**
   * Get the plugin's clean plugin information.
   * @return The PluginCleaningData objects that identify the plugin as clean.
   */
  LOOT_API std::vector<PluginCleaningData> GetCleanInfo() const;

  /**
   * Get the locations at which this plugin can be found.
   * @return The locations at which this plugin can be found.
   */
  LOOT_API std::vector<Location> GetLocations() const;

  /**
   * Get the plugin's messages as SimpleMessage objects for the given language.
   * @param language
   *        The language to create the SimpleMessage objects for.
   * @return The plugin's messages as SimpleMessage objects.
   */
  LOOT_API std::vector<SimpleMessage> GetSimpleMessages(
      const std::string& language) const;

  /**
   * Set the plugin's group.
   * @param group
   *        The name of the group this plugin belongs to.
   */
  LOOT_API void SetGroup(const std::string& group);

  /**
   * Unsets the plugin's group.
   */
  LOOT_API void UnsetGroup();

  /**
   * Set the files that the plugin must load after.
   * @param after
   *        The files to set.
   */
  LOOT_API void SetLoadAfterFiles(const std::vector<File>& after);

  /**
   * Set the files that the plugin requires to be installed.
   * @param requirements
   *        The files to set.
   */
  LOOT_API void SetRequirements(const std::vector<File>& requirements);

  /**
   * Set the files that the plugin must load after.
   * @param incompatibilities
   *        The files to set.
   */
  LOOT_API void SetIncompatibilities(const std::vector<File>& incompatibilities);

  /**
   * Set the plugin's messages.
   * @param messages
   *        The messages to set.
   */
  LOOT_API void SetMessages(const std::vector<Message>& messages);

  /**
   * Set the plugin's Bash Tag suggestions.
   * @param tags
   *        The Bash Tag suggestions to set.
   */
  LOOT_API void SetTags(const std::vector<Tag>& tags);

  /**
   * Set the plugin's dirty information.
   * @param info
   *        The dirty information to set.
   */
  LOOT_API void SetDirtyInfo(const std::vector<PluginCleaningData>& info);

  /**
   * Set the plugin's clean information.
   * @param info
   *        The clean information to set.
   */
  LOOT_API void SetCleanInfo(const std::vector<PluginCleaningData>& info);

  /**
   * Set the plugin's locations.
   * @param locations
   *        The locations to set.
   */
  LOOT_API void SetLocations(const std::vector<Location>& locations);

  /**
   * Check if no plugin metadata is set.
   * @return True if the group is implicit and the metadata containers are all
   *         empty, false otherwise.
   */
  LOOT_API bool HasNameOnly() const;

  /**
   * Check if the plugin name is a regular expression.
   * @return True if the plugin name contains any of the characters ``:\*?|``,
   *         false otherwise.
   */
  LOOT_API bool IsRegexPlugin() const;

  /**
   * Check if two PluginMetadata objects are equal by comparing their name
   * values.
   * @returns True if the plugin names are case-insensitively equal, false
   *          otherwise.
   */
  LOOT_API bool operator==(const PluginMetadata& rhs) const;

  /**
   * Check if two PluginMetadata objects are not equal by comparing their name
   * values.
   * @returns True if the plugin names are not case-insensitively equal, false
   *          otherwise.
   */
  LOOT_API bool operator!=(const PluginMetadata& rhs) const;
private:
  std::string name_;
  std::optional<std::string> group_;
  std::vector<File> loadAfter_;
  std::vector<File> requirements_;
  std::vector<File> incompatibilities_;
  std::vector<Message> messages_;
  std::vector<Tag> tags_;
  std::vector<PluginCleaningData> dirtyInfo_;
  std::vector<PluginCleaningData> cleanInfo_;
  std::vector<Location> locations_;
};
}

#endif
