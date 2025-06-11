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

#ifndef LOOT_METADATA_PLUGIN_CLEANING_DATA
#define LOOT_METADATA_PLUGIN_CLEANING_DATA

#include <cstdint>
#include <string>
#include <string_view>

#include "loot/api_decorator.h"
#include "loot/metadata/message.h"

namespace loot {
/**
 * Represents data identifying the plugin under which it is stored as dirty or
 * clean.
 */
class PluginCleaningData {
public:
  /**
   * Construct a PluginCleaningData object with zero CRC, ITM count, deleted
   * reference count and deleted navmesh count values, an empty utility string
   * and no detail.
   */
  LOOT_API PluginCleaningData() = default;

  /**
   * Construct a PluginCleaningData object with the given CRC and utility,
   * zero ITM count, deleted reference count and deleted navmesh count
   * values and no detail.
   * @param  crc
   *         The CRC of a plugin.
   * @param  utility
   *         The utility that the plugin cleanliness was checked with.
   */
  LOOT_API explicit PluginCleaningData(uint32_t crc,
                                       std::string_view utility);

  /**
   * Construct a PluginCleaningData object with the given values.
   * @param  crc
   *         A clean or dirty plugin's CRC.
   * @param  utility
   *         The utility that the plugin cleanliness was checked with.
   * @param  detail
   *         A vector of localised information message strings about the plugin
   *         cleanliness.
   * @param  itm
   *         The number of Identical To Master records found in the plugin.
   * @param  ref
   *         The number of deleted references found in the plugin.
   * @param  nav
   *         The number of deleted navmeshes found in the plugin.
   */
  LOOT_API explicit PluginCleaningData(
      uint32_t crc,
      std::string_view utility,
      const std::vector<MessageContent>& detail,
      unsigned int itm,
      unsigned int ref,
      unsigned int nav);

  /**
   * Get the CRC that identifies the plugin that the cleaning data is for.
   * @return A CRC-32 checksum.
   */
  LOOT_API uint32_t GetCRC() const;

  /**
   * Get the number of Identical To Master records in the plugin.
   * @return The number of Identical To Master records in the plugin.
   */
  LOOT_API unsigned int GetITMCount() const;

  /**
   * Get the number of deleted references in the plugin.
   * @return The number of deleted references in the plugin.
   */
  LOOT_API unsigned int GetDeletedReferenceCount() const;

  /**
   * Get the number of deleted navmeshes in the plugin.
   * @return The number of deleted navmeshes in the plugin.
   */
  LOOT_API unsigned int GetDeletedNavmeshCount() const;

  /**
   * Get the name of the cleaning utility that was used to check the plugin.
   * @return A cleaning utility name, possibly related information such as
   *         a version number and/or a CommonMark-formatted URL to the utility's
   *         download location.
   */
  LOOT_API std::string GetCleaningUtility() const;

  /**
   * Get any additional informative message content supplied with the cleaning
   * data, eg. a link to a cleaning guide or information on wild edits or manual
   * cleaning steps.
   * @return A vector of localised MessageContent objects.
   */
  LOOT_API std::vector<MessageContent> GetDetail() const;

  /**
   * Compares two PluginCleaningData objects.
   */
  LOOT_API auto operator<=>(const PluginCleaningData& rhs) const = default;

private:
  uint32_t crc_{0};
  unsigned int itm_{0};
  unsigned int ref_{0};
  unsigned int nav_{0};
  std::string utility_;
  std::vector<MessageContent> detail_;
};
}

#endif
