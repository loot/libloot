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
#include <vector>

#include "loot/metadata/message.h"
#include "loot/metadata/tag.h"

namespace loot {
/**
 * Represents a plugin file that has been parsed by LOOT.
 */
class PluginInterface {
public:
  /**
   * Get the plugin's filename.
   * @return The plugin filename.
   */
  virtual std::string GetName() const = 0;

  /**
   * Get the value of the version field in the HEDR subrecord of the plugin's
   * TES4 record.
   * @return The value of the version field, or NaN if the field could not be
   *         found.
   */
  virtual float GetHeaderVersion() const = 0;

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
  virtual std::vector<Tag> GetBashTags() const = 0;

  /**
   * Get the plugin's CRC-32 checksum.
   * @return An optional containing the plugin's CRC-32 checksum if the plugin
   *         has been fully loaded, otherwise an optional containing no value.
   */
  virtual std::optional<uint32_t> GetCRC() const = 0;

  /**
   * Check if the plugin's master flag is set.
   * @return True if the master flag is set, false otherwise.
   */
  virtual bool IsMaster() const = 0;

  /**
   * Check if the plugin is a light master.
   * @return True if plugin is a light master, false otherwise.
   */
  virtual bool IsLightMaster() const = 0;

  /**
   * Check if the plugin is or would be valid as a light master.
   * @return True if the plugin is a valid light master or would be a valid
   *         light master, false otherwise.
   */
  virtual bool IsValidAsLightMaster() const = 0;

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
  virtual bool DoFormIDsOverlap(const PluginInterface& plugin) const = 0;
};
}

#endif
