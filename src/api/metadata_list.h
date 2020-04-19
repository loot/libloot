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

#ifndef LOOT_API_METADATA_LIST
#define LOOT_API_METADATA_LIST

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "api/helpers/text.h"
#include "api/metadata/condition_evaluator.h"
#include "loot/metadata/group.h"
#include "loot/metadata/plugin_metadata.h"

namespace std {
  /**
    * A specialisation of std::hash for loot::PluginMetadata.
    */
  template<>
  struct hash<loot::PluginMetadata> {
    /**
      * Calculate a hash value for an object of a class that implements
      * loot::PluginMetadata.
      * @return The hash generated from the plugin's normalized filename.
      */
    size_t operator()(const loot::PluginMetadata& plugin) const {
      return hash<string>()(loot::NormalizeFilename(plugin.GetName()));
    }
  };
}

namespace loot {
class MetadataList {
public:
  void Load(const std::filesystem::path& filepath);
  void Save(const std::filesystem::path& filepath) const;
  void Clear();

  std::vector<PluginMetadata> Plugins() const;
  std::vector<Message> Messages() const;
  std::set<std::string> BashTags() const;
  std::vector<Group> Groups() const;

  void SetGroups(const std::vector<Group>& groups);

  // Merges multiple matching regex entries if any are found.
  std::optional<PluginMetadata> FindPlugin(const std::string& pluginName) const;
  void AddPlugin(const PluginMetadata& plugin);

  // Doesn't erase matching regex entries, because they might also
  // be required for other plugins.
  void ErasePlugin(const std::string& pluginName);

  void AppendMessage(const Message& message);

  // Eval plugin conditions.
  void EvalAllConditions(ConditionEvaluator& conditionEvaluator);

protected:
  std::vector<Group> groups_;
  std::set<std::string> bashTags_;
  std::unordered_set<PluginMetadata> plugins_;
  std::vector<PluginMetadata> regexPlugins_;
  std::vector<Message> messages_;

  std::unordered_set<PluginMetadata> unevaluatedPlugins_;
  std::vector<PluginMetadata> unevaluatedRegexPlugins_;
  std::vector<Message> unevaluatedMessages_;
};
}

#endif
