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

#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "api/game/game.h"
#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "api/metadata/condition_evaluator.h"
#include "api/metadata/yaml/group.h"
#include "api/metadata/yaml/plugin_metadata.h"
#include "loot/exception/file_access_error.h"

namespace loot {
constexpr std::string_view PRELUDE_ON_FIRST_LINE = "prelude:";
constexpr std::string_view PRELUDE_ON_NEW_LINE = "\nprelude:";

std::string read_to_string(const std::filesystem::path& filePath) {
  std::ifstream in(filePath);
  if (!in.good()) {
    throw FileAccessError("Cannot open " + filePath.u8string());
  }

  auto content = std::string(std::istreambuf_iterator<char>(in),
                             std::istreambuf_iterator<char>());

  in.close();

  return content;
}

std::optional<std::pair<size_t, size_t>> FindPreludeBounds(
    const std::string& masterlist) {
  size_t startOfPrelude = std::string::npos;
  size_t endOfPrelude = std::string::npos;

  // This assumes that the metadata file is using block style at
  // the top level, that ? indicators and tags are not used, and
  // that key strings are unquoted.
  if (boost::starts_with(masterlist, PRELUDE_ON_FIRST_LINE)) {
    startOfPrelude = PRELUDE_ON_FIRST_LINE.size();
  } else {
    startOfPrelude = masterlist.find(PRELUDE_ON_NEW_LINE);

    if (startOfPrelude != std::string::npos) {
      // Skip the leading line break.
      startOfPrelude += PRELUDE_ON_NEW_LINE.size();
    }
  }

  if (startOfPrelude == std::string::npos) {
    // No prelude to replace.
    return std::nullopt;
  }

  // The end of the prelude is marked by a line break followed by a
  // non-space, non-hash (#) character, as this means what follows is
  // unindented content.
  auto pos = startOfPrelude;
  const auto lastIndex = masterlist.size() - 1;
  while (endOfPrelude == std::string::npos) {
    const auto nextLineBreakPos = masterlist.find("\n", pos);
    if (nextLineBreakPos == std::string::npos ||
        nextLineBreakPos == lastIndex) {
      break;
    }

    pos = nextLineBreakPos + 1;

    const auto nextChar = masterlist.at(pos);
    if (nextChar != ' ' && nextChar != '#' && nextChar != '\n') {
      endOfPrelude = nextLineBreakPos;
      break;
    }
  }

  return std::make_pair(startOfPrelude, endOfPrelude);
}

// Indent all prelude content by two spaces to ensure it's parsed as part
// of the prelude.
std::string IndentPrelude(const std::string& prelude) {
  auto newPrelude = "\n  " + boost::replace_all_copy(prelude, "\n", "\n  ");

  boost::replace_all(newPrelude, "  \n", "\n");

  if (boost::ends_with(newPrelude, "\n  ")) {
    return newPrelude.substr(0, newPrelude.size() - 2);
  }

  return newPrelude;
}

std::string ReplaceMetadataListPrelude(const std::string& prelude,
                                       const std::string& masterlist) {
  auto preludeBounds = FindPreludeBounds(masterlist);

  if (!preludeBounds.has_value()) {
    // No prelude to replace.
    return masterlist;
  }

  auto newPrelude = IndentPrelude(prelude);

  const auto [startOfPrelude, endOfPrelude] = preludeBounds.value();

  if (endOfPrelude == std::string::npos) {
    return masterlist.substr(0, startOfPrelude) + newPrelude;
  }

  return masterlist.substr(0, startOfPrelude) + newPrelude +
         masterlist.substr(endOfPrelude);
}

void MetadataList::Load(const std::filesystem::path& filepath) {
  Clear();

  auto logger = getLogger();
  if (logger) {
    logger->debug("Loading file: {}", filepath.u8string());
  }

  std::ifstream in(filepath);
  if (!in.good())
    throw FileAccessError("Cannot open " + filepath.u8string());

  this->Load(in, filepath);

  in.close();
}

void MetadataList::LoadWithPrelude(const std::filesystem::path& filePath,
                                   const std::filesystem::path& preludePath) {
  // Parsing YAML resolves references such that replacing the
  // referenced keys entirely (rather than just replacing their values)
  // does not cause aliases to be re-resolved, so the old values are
  // retained.
  // As such, replacing the prelude needs to happen before parsing,
  // which means reading the files and performing string manipulation.
  auto prelude_content = read_to_string(preludePath);
  auto masterlist_content = read_to_string(filePath);

  masterlist_content =
      ReplaceMetadataListPrelude(prelude_content, masterlist_content);

  auto stream = std::istringstream(masterlist_content);
  this->Load(stream, filePath);
}

void MetadataList::Load(std::istream& istream,
                        const std::filesystem::path& source_path) {
  YAML::Node metadataList = YAML::Load(istream);

  if (!metadataList.IsMap())
    throw FileAccessError("The root of the metadata file " +
                          source_path.u8string() + " is not a YAML map.");

  if (metadataList["plugins"]) {
    for (const auto& node : metadataList["plugins"]) {
      PluginMetadata plugin(node.as<PluginMetadata>());
      if (plugin.IsRegexPlugin())
        regexPlugins_.push_back(plugin);
      else if (!plugins_.emplace(Filename(plugin.GetName()), plugin).second)
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

  auto logger = getLogger();
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
  for (const auto& pluginPair : plugins_) {
    plugins.push_back(pluginPair.second);
  }
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
  const auto defaultGroupsExists =
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

  const auto it = plugins_.find(Filename(pluginName));

  if (it != plugins_.end())
    match = it->second;

  // Now we want to also match possibly multiple regex entries.
  const auto nameMatches = [&](const PluginMetadata& pluginMetadata) {
    return pluginMetadata.NameMatches(pluginName);
  };
  auto regIt = find_if(regexPlugins_.begin(), regexPlugins_.end(), nameMatches);
  while (regIt != regexPlugins_.end()) {
    match.MergeMetadata(*regIt);

    regIt = find_if(++regIt, regexPlugins_.end(), nameMatches);
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
    if (!plugins_.emplace(Filename(plugin.GetName()), plugin).second)
      throw std::invalid_argument(
          "Cannot add \"" + plugin.GetName() +
          "\" to the metadata list as another entry already exists.");
  }
}

// Doesn't erase matching regex entries, because they might also
// be required for other plugins.
void MetadataList::ErasePlugin(const std::string& pluginName) {
  const auto it = plugins_.find(Filename(pluginName));

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
    plugins_.emplace(plugin.first,
                     conditionEvaluator.EvaluateAll(plugin.second));
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
