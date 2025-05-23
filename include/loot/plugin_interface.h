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
#ifndef LOOT_PLUGIN_INTERFACE
#define LOOT_PLUGIN_INTERFACE

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "loot/metadata/message.h"
#include "loot/metadata/tag.h"

namespace loot {
/**
 * Represents a plugin file that has been parsed by LOOT.
 */
class PluginInterface {
public:
  virtual ~PluginInterface() = default;

  /**
   * Get the plugin's filename.
   * @return The plugin filename. If the plugin was ghosted when it was loaded,
   *         this filename will be without the .ghost suffix, unless the game is
   *         OpenMW, in which case ghosted plugins are not supported.
   */
  virtual std::string GetName() const = 0;

  /**
   * Get the value of the version field in the HEDR subrecord of the plugin's
   * TES4 record.
   * @return The value of the version field, or an empty optional if that value
   *         is NaN or could not be found.
   */
  virtual std::optional<float> GetHeaderVersion() const = 0;

  /**
   * Get the plugin's version number from its description field.
   *
   * The description field may not contain a version number, or LOOT may be
   * unable to detect it. The description field parsing may fail to extract the
   * version number correctly, though it functions correctly in all known cases.
   * @return An optional containing a version string if one is found, otherwise
   *         an optional containing no value.
   */
  virtual std::optional<std::string> GetVersion() const = 0;

  /**
   * Get the plugin's masters.
   * @return The plugin's masters in the same order they are listed in the file.
   */
  virtual std::vector<std::string> GetMasters() const = 0;

  /**
   * Get any Bash Tags found in the plugin's description field.
   * @return A set of Bash Tags. The order of elements in the set holds no
   *         semantics.
   */
  virtual std::vector<std::string> GetBashTags() const = 0;

  /**
   * Get the plugin's CRC-32 checksum.
   * @return An optional containing the plugin's CRC-32 checksum if the plugin
   *         has been fully loaded, otherwise an optional containing no value.
   */
  virtual std::optional<uint32_t> GetCRC() const = 0;

  /**
   * Check if the plugin is a master plugin.
   *
   * What causes a plugin to be a master plugin varies by game, but is usually
   * indicated by the plugin having its master flag set and/or by its file
   * extension. However, OpenMW uses neither for determining plugins' load order
   * so all OpenMW plugins are treated as non-masters.
   *
   * The term "master" is potentially confusing: a plugin A may not be a *master
   * plugin*, but may still be a *master of* another plugin by being listed as
   * such in that plugin's header record. Master plugins are sometimes referred
   * to as *master files* or simply *masters*, while the other meaning is always
   * referenced in relation to another plugin.
   * @return True if the plugin is a master plugin, false otherwise.
   */
  virtual bool IsMaster() const = 0;

  /**
   * Check if the plugin is a light plugin.
   * @return True if plugin is a light plugin, false otherwise.
   */
  virtual bool IsLightPlugin() const = 0;

  /**
   * Check if the plugin is a medium plugin.
   * @return True if plugin is a medium plugin, false otherwise.
   */
  virtual bool IsMediumPlugin() const = 0;

  /**
   * Check if the plugin is an update plugin.
   * @return True if plugin is an update plugin, false otherwise.
   */
  virtual bool IsUpdatePlugin() const = 0;

  /**
   * Check if the plugin is a blueprint plugin.
   * @return True if plugin is a blueprint plugin, false otherwise.
   */
  virtual bool IsBlueprintPlugin() const = 0;

  /**
   * Check if the plugin is or would be valid as a light plugin.
   * @return True if the plugin is a valid light plugin or would be a valid
   *         light plugin, false otherwise.
   */
  virtual bool IsValidAsLightPlugin() const = 0;

  /**
   * Check if the plugin is or would be valid as a medium plugin.
   * @return True if the plugin is a valid medium plugin or would be a valid
   *         medium plugin, false otherwise.
   */
  virtual bool IsValidAsMediumPlugin() const = 0;

  /**
   * Check if the plugin is or would be valid as an update plugin.
   * @return True if the plugin is a valid update plugin or would be a valid
   *         update plugin, false otherwise.
   */
  virtual bool IsValidAsUpdatePlugin() const = 0;

  /**
   * Check if the plugin contains any records other than its TES4 header.
   * @return True if the plugin only contains a TES4 header, false otherwise.
   */
  virtual bool IsEmpty() const = 0;

  /**
   * Check if the plugin loads an archive (BSA/BA2 depending on the game).
   * @return True if the plugin loads an archive, false otherwise.
   */
  virtual bool LoadsArchive() const = 0;

  /**
   * Check if two plugins contain a record with the same ID.
   * @param  plugin
   *         The other plugin to check for overlap with.
   * @return True if the plugins both contain at least one record with the same
   *         ID, false otherwise. FormIDs are compared for all games apart from
   *         Morrowind, which doesn't have FormIDs and so has other identifying
   *         data compared.
   */
  virtual bool DoRecordsOverlap(const PluginInterface& plugin) const = 0;
};
}

#endif
