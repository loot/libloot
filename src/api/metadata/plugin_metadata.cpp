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

#include <filesystem>
#include <regex>

#include <boost/algorithm/string.hpp>

#include "api/game/game.h"
#include "api/helpers/collections.h"
#include "api/helpers/logging.h"
#include "api/helpers/text.h"

using std::inserter;
using std::regex;
using std::regex_match;
using std::set;
using std::vector;

namespace loot {
PluginMetadata::PluginMetadata() {}

PluginMetadata::PluginMetadata(const std::string& n) : name_(n) {
  // If the name passed ends in '.ghost', that should be trimmed.
  if (boost::iends_with(name_, ".ghost"))
    name_ = name_.substr(0, name_.length() - 6);
}

void PluginMetadata::MergeMetadata(const PluginMetadata& plugin) {
  if (plugin.HasNameOnly())
    return;

  if (!group_.has_value() && plugin.GetGroup()) {
    group_ = plugin.GetGroup();
  }

  loadAfter_ = mergeVectors(loadAfter_, plugin.loadAfter_);
  requirements_ = mergeVectors(requirements_, plugin.requirements_);
  incompatibilities_ = mergeVectors(incompatibilities_, plugin.incompatibilities_);

  // Merge Bash Tags too. Conditions are ignored during comparison, but
  // if a tag is added and removed, both instances will be in the set.
  tags_.insert(begin(plugin.tags_), end(plugin.tags_));

  // Messages are in an ordered list, and should be fully merged.
  messages_.insert(
      end(messages_), begin(plugin.messages_), end(plugin.messages_));

  dirtyInfo_.insert(begin(plugin.dirtyInfo_), end(plugin.dirtyInfo_));
  cleanInfo_.insert(begin(plugin.cleanInfo_), end(plugin.cleanInfo_));
  locations_.insert(begin(plugin.locations_), end(plugin.locations_));

  return;
}

PluginMetadata PluginMetadata::NewMetadata(const PluginMetadata& plugin) const {
  using std::set_difference;

  PluginMetadata p(*this);

  if (p.group_ == plugin.group_) {
    p.group_ = std::nullopt;
  }

  // Compare this plugin against the given plugin.
  p.SetLoadAfterFiles(diffVectors(loadAfter_, plugin.loadAfter_));
  p.SetRequirements(diffVectors(requirements_, plugin.requirements_));
  p.SetIncompatibilities(diffVectors(incompatibilities_, plugin.incompatibilities_));

  vector<Message> msgs1 = plugin.GetMessages();
  vector<Message> msgs2 = messages_;
  std::sort(begin(msgs1), end(msgs1));
  std::sort(begin(msgs2), end(msgs2));
  vector<Message> mDiff;
  set_difference(begin(msgs2),
                 end(msgs2),
                 begin(msgs1),
                 end(msgs1),
                 inserter(mDiff, begin(mDiff)));
  p.SetMessages(mDiff);

  set<Tag> tagDiff;
  set_difference(begin(tags_),
                 end(tags_),
                 begin(plugin.tags_),
                 end(plugin.tags_),
                 inserter(tagDiff, begin(tagDiff)));
  p.SetTags(tagDiff);

  set<PluginCleaningData> dirtDiff;
  set_difference(begin(dirtyInfo_),
                 end(dirtyInfo_),
                 begin(plugin.dirtyInfo_),
                 end(plugin.dirtyInfo_),
                 inserter(dirtDiff, begin(dirtDiff)));
  p.SetDirtyInfo(dirtDiff);

  set<PluginCleaningData> cleanDiff;
  set_difference(begin(cleanInfo_),
                 end(cleanInfo_),
                 begin(plugin.cleanInfo_),
                 end(plugin.cleanInfo_),
                 inserter(cleanDiff, begin(cleanDiff)));
  p.SetCleanInfo(cleanDiff);

  set<Location> locationsDiff;
  set_difference(begin(locations_),
                 end(locations_),
                 begin(plugin.locations_),
                 end(plugin.locations_),
                 inserter(locationsDiff, begin(locationsDiff)));
  p.SetLocations(locationsDiff);

  return p;
}

std::string PluginMetadata::GetName() const { return name_; }

std::optional<std::string> PluginMetadata::GetGroup() const { return group_; }

std::vector<File> PluginMetadata::GetLoadAfterFiles() const {
  return loadAfter_;
}

std::vector<File> PluginMetadata::GetRequirements() const { return requirements_; }

std::vector<File> PluginMetadata::GetIncompatibilities() const {
  return incompatibilities_;
}

std::vector<Message> PluginMetadata::GetMessages() const { return messages_; }

std::set<Tag> PluginMetadata::GetTags() const { return tags_; }

std::set<PluginCleaningData> PluginMetadata::GetDirtyInfo() const {
  return dirtyInfo_;
}

std::set<PluginCleaningData> PluginMetadata::GetCleanInfo() const {
  return cleanInfo_;
}

std::set<Location> PluginMetadata::GetLocations() const { return locations_; }

std::vector<SimpleMessage> PluginMetadata::GetSimpleMessages(
    const std::string& language) const {
  std::vector<SimpleMessage> simpleMessages(messages_.size());
  std::transform(begin(messages_),
                 end(messages_),
                 begin(simpleMessages),
                 [&](const Message& message) {
                   return message.ToSimpleMessage(language);
                 });

  return simpleMessages;
}

void PluginMetadata::SetGroup(const std::string& group) { group_ = group; }

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

void PluginMetadata::SetTags(const std::set<Tag>& t) { tags_ = t; }

void PluginMetadata::SetDirtyInfo(
    const std::set<PluginCleaningData>& dirtyInfo) {
  dirtyInfo_ = dirtyInfo;
}

void PluginMetadata::SetCleanInfo(const std::set<PluginCleaningData>& info) {
  cleanInfo_ = info;
}

void PluginMetadata::SetLocations(const std::set<Location>& locations) {
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

bool PluginMetadata::operator==(const PluginMetadata& rhs) const {
  if (IsRegexPlugin() == rhs.IsRegexPlugin()) {
    return CompareFilenames(name_, rhs.name_) == 0;
  }

  if (IsRegexPlugin())
    return regex_match(rhs.GetName(),
                       regex(name_, regex::ECMAScript | regex::icase));
  else
    return regex_match(name_,
                       regex(rhs.GetName(), regex::ECMAScript | regex::icase));
}

bool PluginMetadata::operator!=(const PluginMetadata& rhs) const {
  return !(*this == rhs);
}

bool PluginMetadata::operator==(const std::string& rhs) const {
  return *this == PluginMetadata(rhs);
}

bool PluginMetadata::operator!=(const std::string& rhs) const {
  return !(*this == rhs);
}
}
