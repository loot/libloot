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

#include "api/metadata/condition_evaluator.h"
#include "api/metadata_list.h"
#include "loot/database_interface.h"
#include "loot/enum/game_type.h"
#include "loot/vertex.h"

namespace loot {
struct ApiDatabase final : public DatabaseInterface {
  explicit ApiDatabase(std::shared_ptr<ConditionEvaluator> conditionEvaluator);

  void LoadMasterlist(
      const std::filesystem::path& masterlistPath) override;

  void LoadMasterlistWithPrelude(
      const std::filesystem::path& masterlistPath,
      const std::filesystem::path& masterlistPreludePath) override;

  void LoadUserlist(
      const std::filesystem::path& userlistPath) override;

  void WriteUserMetadata(const std::filesystem::path& outputFile,
                         const bool overwrite) const override;

  void WriteMinimalList(const std::filesystem::path& outputFile,
                        const bool overwrite) const override;

  bool Evaluate(const std::string& condition) const override;

  std::vector<std::string> GetKnownBashTags() const override;

  std::vector<Message> GetGeneralMessages(
      bool evaluateConditions = false) const override;

  std::vector<Group> GetGroups(bool includeUserMetadata = true) const override;
  std::vector<Group> GetUserGroups() const override;
  void SetUserGroups(const std::vector<Group>& groups) override;
  std::vector<Vertex> GetGroupsPath(
      std::string_view fromGroupName,
      std::string_view toGroupName) const override;

  std::optional<PluginMetadata> GetPluginMetadata(
      std::string_view plugin,
      bool includeUserMetadata = true,
      bool evaluateConditions = false) const override;

  std::optional<PluginMetadata> GetPluginUserMetadata(
      std::string_view plugin,
      bool evaluateConditions = false) const override;

  void SetPluginUserMetadata(const PluginMetadata& pluginMetadata) override;

  void DiscardPluginUserMetadata(std::string_view plugin) override;

  void DiscardAllUserMetadata() override;

private:
  std::shared_ptr<ConditionEvaluator> conditionEvaluator_;
  MetadataList masterlist_;
  MetadataList userlist_;
};
}

#endif
