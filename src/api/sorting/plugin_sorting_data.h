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
  explicit PluginSortingData(const Plugin& plugin,
                    const PluginMetadata& masterlistMetadata,
                    const PluginMetadata& userMetadata,
                    const std::vector<std::string>& loadOrder,
                    const GameType gameType,
                    const std::vector<std::shared_ptr<const Plugin>>& loadedPlugins);

  std::string GetName() const;
  bool IsMaster() const;
  bool LoadsArchive() const;
  std::vector<std::string> GetMasters() const;
  size_t NumOverrideFormIDs() const;
  bool DoFormIDsOverlap(const PluginSortingData& plugin) const;

  std::string GetGroup() const;

  std::unordered_set<std::string> GetAfterGroupPlugins() const;
  void SetAfterGroupPlugins(std::unordered_set<std::string> plugins);

  const std::set<File>& GetMasterlistLoadAfterFiles() const;
  const std::set<File>& GetUserLoadAfterFiles() const;
  const std::set<File>& GetMasterlistRequirements() const;
  const std::set<File>& GetUserRequirements() const;

  const std::optional<size_t>& GetLoadOrderIndex() const;

private:
  const Plugin& plugin_;
  std::string group_;
  std::unordered_set<std::string> afterGroupPlugins_;

  std::set<File> masterlistLoadAfter_;
  std::set<File> userLoadAfter_;
  std::set<File> masterlistReq_;
  std::set<File> userReq_;

  std::optional<size_t> loadOrderIndex_;
  size_t numOverrideFormIDs;
};
}

#endif
