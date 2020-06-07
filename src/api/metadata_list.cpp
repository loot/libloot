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

#include "api/metadata_list.h"

#include <filesystem>
#include <fstream>

#include "api/game/game.h"
#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "api/metadata/condition_evaluator.h"
#include "api/metadata/yaml/group.h"
#include "api/metadata/yaml/plugin_metadata.h"
#include "loot/exception/file_access_error.h"

namespace loot {
void MetadataList::Load(const std::filesystem::path& filepath) {
  Clear();

  auto logger = getLogger();
  if (logger) {
    logger->debug("Loading file: {}", filepath.u8string());
  }

  std::ifstream in(filepath);
  if (!in.good())
    throw FileAccessError("Cannot open " + filepath.u8string());

  YAML::Node metadataList = YAML::Load(in);
  in.close();

  if (!metadataList.IsMap())
    throw FileAccessError("The root of the metadata file " +
                          filepath.u8string() + " is not a YAML map.");

  if (metadataList["plugins"]) {
    for (const auto& node : metadataList["plugins"]) {
      PluginMetadata plugin(node.as<PluginMetadata>());
      if (plugin.IsRegexPlugin())
        regexPlugins_.push_back(plugin);
      else if (!plugins_.insert(plugin).second)
        throw FileAccessError("More than one entry exists for plugin \"" +
                              plugin.GetName() + "\"");
    }
  }
  if (metadataList["globals"])
    messages_ = metadataList["globals"].as<std::vector<Message>>();

  std::unordered_set<std::string> bashTags;
  if (metadataList["bash_tags"]) {
    for (const auto& node : metadataList["bash_tags"]) {
      auto bashTag = node.as<std::string>();
      if (bashTags.count(bashTag) != 0) {
        throw FileAccessError("More than one entry exists for Bash Tag \"" +
                              bashTag + "\"");
      }
      bashTags_.push_back(bashTag);
      bashTags.insert(bashTag);
    }
  }

  std::unordered_set<std::string> groupNames;
  if (metadataList["groups"]) {
    for (const auto& node : metadataList["groups"]) {
      auto group = node.as<Group>();
      if (groupNames.count(group.GetName()) != 0) {
        throw FileAccessError("More than one entry exists for group \"" +
                              group.GetName() + "\"");
      }
      groups_.push_back(group);
      groupNames.insert(group.GetName());
    }
  }

  auto defaultGroup = Group();
  if (groupNames.count(defaultGroup.GetName()) == 0) {
    groups_.insert(groups_.cbegin(), Group());
  }

  if (logger) {
    logger->debug("File loaded successfully.");
  }
}

void MetadataList::Save(const std::filesystem::path& filepath) const {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Saving metadata list to: {}", filepath.u8string());
  }
  YAML::Emitter emitter;
  emitter.SetIndent(2);
  emitter << YAML::BeginMap;

  if (!bashTags_.empty())
    emitter << YAML::Key << "bash_tags" << YAML::Value << bashTags_;

  if (!groups_.empty()) {
    emitter << YAML::Key << "groups" << YAML::Value << groups_;
  }

  if (!messages_.empty())
    emitter << YAML::Key << "globals" << YAML::Value << messages_;

  auto plugins = Plugins();
  std::sort(plugins.begin(),
            plugins.end(),
            [](const PluginMetadata& p1, const PluginMetadata& p2) {
              return CompareFilenames(p1.GetName(), p2.GetName()) < 0;
            });

  if (!plugins.empty())
    emitter << YAML::Key << "plugins" << YAML::Value << plugins;

  emitter << YAML::EndMap;

  std::ofstream out(filepath);
  if (out.fail())
    throw FileAccessError("Couldn't open output file.");

  out << emitter.c_str();
  out.close();
}

void MetadataList::Clear() {
  groups_.clear();
  bashTags_.clear();
  plugins_.clear();
  regexPlugins_.clear();
  messages_.clear();

  unevaluatedPlugins_.clear();
  unevaluatedRegexPlugins_.clear();
  unevaluatedMessages_.clear();
}

std::vector<PluginMetadata> MetadataList::Plugins() const {
  std::vector<PluginMetadata> plugins;
  plugins.reserve(plugins_.size() + regexPlugins_.size());
  plugins.insert(plugins.end(), plugins_.begin(), plugins_.end());
  plugins.insert(plugins.end(), regexPlugins_.begin(), regexPlugins_.end());

  return plugins;
}

std::vector<Message> MetadataList::Messages() const { return messages_; }

std::vector<std::string> MetadataList::BashTags() const { return bashTags_; }

std::vector<Group> MetadataList::Groups() const {
  if (groups_.empty()) {
    return {Group()};
  }

  return groups_;
}

void MetadataList::SetGroups(const std::vector<Group>& groups) {
  // In case the default group is missing.
  auto defaultGroupName = Group().GetName();
  auto defaultGroupsExists =
      std::any_of(groups.cbegin(), groups.cend(), [&](const Group& group) {
        return group.GetName() == defaultGroupName;
      });

  if (!defaultGroupsExists) {
    groups_.clear();
    groups_.push_back(Group());
    groups_.insert(groups_.end(), groups.begin(), groups.end());
  } else {
    groups_ = groups;
  }
}

// Merges multiple matching regex entries if any are found.
std::optional<PluginMetadata> MetadataList::FindPlugin(
    const std::string& pluginName) const {
  PluginMetadata match(pluginName);

  auto it = plugins_.find(match);

  if (it != plugins_.end())
    match = *it;

  // Now we want to also match possibly multiple regex entries.
  auto regIt = find(regexPlugins_.begin(), regexPlugins_.end(), match);
  while (regIt != regexPlugins_.end()) {
    match.MergeMetadata(*regIt);

    regIt = find(++regIt, regexPlugins_.end(), match);
  }

  if (match.HasNameOnly()) {
    return std::nullopt;
  }

  return match;
}

void MetadataList::AddPlugin(const PluginMetadata& plugin) {
  if (plugin.IsRegexPlugin())
    regexPlugins_.push_back(plugin);
  else {
    if (!plugins_.insert(plugin).second)
      throw std::invalid_argument(
          "Cannot add \"" + plugin.GetName() +
          "\" to the metadata list as another entry already exists.");
  }
}

// Doesn't erase matching regex entries, because they might also
// be required for other plugins.
void MetadataList::ErasePlugin(const std::string& pluginName) {
  auto it = plugins_.find(PluginMetadata(pluginName));

  if (it != plugins_.end()) {
    plugins_.erase(it);
    return;
  }
}

void MetadataList::AppendMessage(const Message& message) {
  messages_.push_back(message);
}

void MetadataList::EvalAllConditions(ConditionEvaluator& conditionEvaluator) {
  if (unevaluatedPlugins_.empty())
    unevaluatedPlugins_.swap(plugins_);
  else
    plugins_.clear();

  for (const auto& plugin : unevaluatedPlugins_) {
    plugins_.insert(conditionEvaluator.EvaluateAll(plugin));
  }

  if (unevaluatedRegexPlugins_.empty())
    unevaluatedRegexPlugins_ = regexPlugins_;
  else
    regexPlugins_ = unevaluatedRegexPlugins_;

  for (auto& plugin : regexPlugins_) {
    plugin = conditionEvaluator.EvaluateAll(plugin);
  }

  if (unevaluatedMessages_.empty())
    unevaluatedMessages_.swap(messages_);
  else
    messages_.clear();

  for (const auto& message : unevaluatedMessages_) {
    if (conditionEvaluator.Evaluate(message.GetCondition()))
      messages_.push_back(message);
  }
}
}
