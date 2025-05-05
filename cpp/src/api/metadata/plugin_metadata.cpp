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

#include "loot/metadata/plugin_metadata.h"

#include <cstring>
#include <regex>
#include <stdexcept>

#include "api/convert.h"
#include "libloot-cpp/src/lib.rs.h"

namespace {
// Append second to first, skipping any elements that are already present in
// first. Although this is O(U * M), both input vectors are expected to be
// small (with tens of elements being an unusually large number).
template<typename T>
std::vector<T> mergeVectors(std::vector<T> first,
                            const std::vector<T>& second) {
  const auto initialSizeOfFirst = first.size();
  for (const auto& element : second) {
    const auto end = first.cbegin() + initialSizeOfFirst;

    if (std::find(first.cbegin(), end, element) == end) {
      first.push_back(element);
    }
  }

  return first;
}

std::string TrimDotGhostExtension(std::string&& filename) {
  using std::string_view_literals::operator""sv;
  // If the name passed ends in '.ghost', that should be trimmed.
  constexpr std::string_view GHOST_FILE_EXTENSION = ".ghost"sv;

  if (filename.length() < GHOST_FILE_EXTENSION.length()) {
    return filename;
  }

  auto view = std::string_view(filename);
  view.remove_prefix(filename.length() - GHOST_FILE_EXTENSION.length());

  bool areEqual = std::equal(
      view.begin(),
      view.end(),
      GHOST_FILE_EXTENSION.begin(),
      [](unsigned char a, unsigned char b) { return std::tolower(a) == b; });

  if (areEqual) {
    return filename.substr(0,
                           filename.length() - GHOST_FILE_EXTENSION.length());
  }

  return filename;
}
}

namespace loot {
// If the name passed ends in '.ghost', that should be trimmed.
PluginMetadata::PluginMetadata(std::string_view n) :
    name_(TrimDotGhostExtension(std::string(n))) {}

void PluginMetadata::MergeMetadata(const PluginMetadata& plugin) {
  if (plugin.HasNameOnly())
    return;

  if (!group_.has_value() && plugin.GetGroup()) {
    group_ = plugin.GetGroup();
  }

  loadAfter_ = mergeVectors(loadAfter_, plugin.loadAfter_);
  requirements_ = mergeVectors(requirements_, plugin.requirements_);
  incompatibilities_ =
      mergeVectors(incompatibilities_, plugin.incompatibilities_);

  tags_ = mergeVectors(tags_, plugin.tags_);

  // Messages are in an ordered list, and should be fully merged.
  messages_.insert(
      end(messages_), begin(plugin.messages_), end(plugin.messages_));

  dirtyInfo_ = mergeVectors(dirtyInfo_, plugin.dirtyInfo_);
  cleanInfo_ = mergeVectors(cleanInfo_, plugin.cleanInfo_);
  locations_ = mergeVectors(locations_, plugin.locations_);

  return;
}

std::string PluginMetadata::GetName() const { return name_; }

std::optional<std::string> PluginMetadata::GetGroup() const { return group_; }

std::vector<File> PluginMetadata::GetLoadAfterFiles() const {
  return loadAfter_;
}

std::vector<File> PluginMetadata::GetRequirements() const {
  return requirements_;
}

std::vector<File> PluginMetadata::GetIncompatibilities() const {
  return incompatibilities_;
}

std::vector<Message> PluginMetadata::GetMessages() const { return messages_; }

std::vector<Tag> PluginMetadata::GetTags() const { return tags_; }

std::vector<PluginCleaningData> PluginMetadata::GetDirtyInfo() const {
  return dirtyInfo_;
}

std::vector<PluginCleaningData> PluginMetadata::GetCleanInfo() const {
  return cleanInfo_;
}

std::vector<Location> PluginMetadata::GetLocations() const {
  return locations_;
}

void PluginMetadata::SetGroup(std::string_view group) { group_ = group; }

void PluginMetadata::UnsetGroup() { group_ = std::nullopt; }

void PluginMetadata::SetLoadAfterFiles(const std::vector<File>& l) {
  loadAfter_ = l;
}

void PluginMetadata::SetRequirements(const std::vector<File>& r) {
  requirements_ = r;
}

void PluginMetadata::SetIncompatibilities(const std::vector<File>& i) {
  incompatibilities_ = i;
}

void PluginMetadata::SetMessages(const std::vector<Message>& m) {
  messages_ = m;
}

void PluginMetadata::SetTags(const std::vector<Tag>& t) { tags_ = t; }

void PluginMetadata::SetDirtyInfo(
    const std::vector<PluginCleaningData>& dirtyInfo) {
  dirtyInfo_ = dirtyInfo;
}

void PluginMetadata::SetCleanInfo(const std::vector<PluginCleaningData>& info) {
  cleanInfo_ = info;
}

void PluginMetadata::SetLocations(const std::vector<Location>& locations) {
  locations_ = locations;
}

bool PluginMetadata::HasNameOnly() const {
  return !group_.has_value() && loadAfter_.empty() && requirements_.empty() &&
         incompatibilities_.empty() && messages_.empty() && tags_.empty() &&
         dirtyInfo_.empty() && cleanInfo_.empty() && locations_.empty();
}

bool PluginMetadata::IsRegexPlugin() const {
  // Treat as regex if the plugin filename contains any of ":\*?|" as
  // they are not valid Windows filename characters, but have meaning
  // in regexes.
  return strpbrk(name_.c_str(), ":\\*?|") != nullptr;
}

bool PluginMetadata::NameMatches(std::string_view pluginName) const {
  const auto metadata = convert(*this);
  return metadata->name_matches(convert(pluginName));
}

std::string PluginMetadata::AsYaml() const {
  const auto metadata = convert(*this);
  return std::string(metadata->as_yaml());
}
}
