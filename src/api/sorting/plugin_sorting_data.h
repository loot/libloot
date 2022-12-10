/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2018    WrinklyNinja

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

#ifndef LOOT_API_SORTING_PLUGIN_SORTING_DATA
#define LOOT_API_SORTING_PLUGIN_SORTING_DATA

#include "api/plugin.h"
#include "loot/metadata/plugin_metadata.h"

namespace loot {
class PluginSortingData {
public:
  explicit PluginSortingData();

  /**
   * This stores a copy of the plugin pointer that is passed to it, so
   * PluginSortingData objects must not live longer than the Plugin objects
   * that they are constructed from.
   */
  explicit PluginSortingData(
      const PluginSortingInterface* plugin,
      const PluginMetadata& masterlistMetadata,
      const PluginMetadata& userMetadata,
      const std::vector<std::string>& loadOrder,
      const GameType gameType,
      const std::vector<const PluginInterface*>& loadedPlugins);

  std::string GetName() const;
  bool IsMaster() const;
  bool LoadsArchive() const;
  std::vector<std::string> GetMasters() const;
  size_t NumOverrideFormIDs() const;
  bool DoFormIDsOverlap(const PluginSortingData& plugin) const;

  std::string GetGroup() const;

  std::unordered_set<std::string> GetAfterGroupPlugins() const;
  void SetAfterGroupPlugins(std::unordered_set<std::string> plugins);

  const std::vector<File>& GetMasterlistLoadAfterFiles() const;
  const std::vector<File>& GetUserLoadAfterFiles() const;
  const std::vector<File>& GetMasterlistRequirements() const;
  const std::vector<File>& GetUserRequirements() const;

  const std::optional<size_t>& GetLoadOrderIndex() const;

private:
  const PluginSortingInterface* plugin_;
  std::string group_;
  std::unordered_set<std::string> afterGroupPlugins_;

  std::vector<File> masterlistLoadAfter_;
  std::vector<File> userLoadAfter_;
  std::vector<File> masterlistReq_;
  std::vector<File> userReq_;

  std::optional<size_t> loadOrderIndex_;
  size_t numOverrideFormIDs;
};
}

#endif
