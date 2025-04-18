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
#include <string_view>
#include <unordered_map>
#include <vector>

#include "api/helpers/text.h"
#include "api/metadata/condition_evaluator.h"
#include "loot/metadata/group.h"
#include "loot/metadata/plugin_metadata.h"

namespace std {
template<>
struct hash<loot::Filename> {
  size_t operator()(const loot::Filename& filename) const {
    return hash<string>()(loot::NormalizeFilename(std::string(filename)));
  }
};
}

namespace loot {

// This assumes that the prelude and masterlist files both use
// YAML's block style (at least up to the end of the prelude in the
// latter). This is true for all official files.
std::string ReplaceMetadataListPrelude(const std::string& prelude,
                                       std::string&& masterlist);

class MetadataList {
public:
  void Load(const std::filesystem::path& filepath);
  void LoadWithPrelude(const std::filesystem::path& filePath,
                       const std::filesystem::path& preludePath);
  void Save(const std::filesystem::path& filepath) const;
  void Clear();

  std::vector<PluginMetadata> Plugins() const;
  std::vector<Message> Messages() const;
  std::vector<std::string> BashTags() const;
  std::vector<Group> Groups() const;

  void SetGroups(const std::vector<Group>& groups);

  // Merges multiple matching regex entries if any are found.
  std::optional<PluginMetadata> FindPlugin(std::string_view pluginName) const;
  void AddPlugin(const PluginMetadata& plugin);

  // Doesn't erase matching regex entries, because they might also
  // be required for other plugins.
  void ErasePlugin(std::string_view pluginName);

  void AppendMessage(const Message& message);

private:
  std::vector<Group> groups_;
  std::vector<std::string> bashTags_;
  std::unordered_map<Filename, PluginMetadata> plugins_;
  std::vector<PluginMetadata> regexPlugins_;
  std::vector<Message> messages_;

  void Load(std::istream& istream, const std::filesystem::path& source_path);
};
}

#endif
