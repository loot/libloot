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

#ifndef LOOT_API_LOOT_DB
#define LOOT_API_LOOT_DB

#include <list>
#include <string>
#include <vector>

#include "api/game/game_cache.h"
#include "api/game/load_order_handler.h"
#include "api/masterlist.h"
#include "api/metadata/condition_evaluator.h"
#include "api/metadata_list.h"
#include "loot/database_interface.h"
#include "loot/enum/game_type.h"
#include "loot/vertex.h"

namespace loot {
struct ApiDatabase : public DatabaseInterface {
  explicit ApiDatabase(std::shared_ptr<ConditionEvaluator> conditionEvaluator);

  void LoadLists(const std::filesystem::path& masterlist_path,
                 const std::filesystem::path& userlist_path = "");

  void WriteUserMetadata(const std::filesystem::path& outputFile,
                         const bool overwrite) const;

  void WriteMinimalList(const std::filesystem::path& outputFile,
                        const bool overwrite) const;

  bool UpdateMasterlist(const std::filesystem::path& masterlist_path,
                        const std::string& remote_url,
                        const std::string& remote_branch);

  MasterlistInfo GetMasterlistRevision(
      const std::filesystem::path& masterlist_path,
      const bool get_short_id) const;

  bool IsLatestMasterlist(const std::filesystem::path& masterlist_path,
                          const std::string& branch) const;

  std::set<std::string> GetKnownBashTags() const;

  std::vector<Message> GetGeneralMessages(
      bool evaluateConditions = false) const;

  std::vector<Group> GetGroups(bool includeUserMetadata = true) const;
  std::vector<Group> GetUserGroups() const;
  void SetUserGroups(const std::vector<Group>& groups);
  std::vector<Vertex> GetGroupsPath(const std::string& fromGroupName,
                                    const std::string& toGroupName) const;

  std::optional<PluginMetadata> GetPluginMetadata(
      const std::string& plugin,
      bool includeUserMetadata = true,
      bool evaluateConditions = false) const;

  std::optional<PluginMetadata> GetPluginUserMetadata(
      const std::string& plugin,
      bool evaluateConditions = false) const;

  void SetPluginUserMetadata(const PluginMetadata& pluginMetadata);

  void DiscardPluginUserMetadata(const std::string& plugin);

  void DiscardAllUserMetadata();

private:
  std::shared_ptr<ConditionEvaluator> conditionEvaluator_;
  Masterlist masterlist_;
  MetadataList userlist_;
};
}

#endif
