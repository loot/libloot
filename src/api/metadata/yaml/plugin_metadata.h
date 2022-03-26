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
#ifndef LOOT_YAML_PLUGIN_METADATA
#define LOOT_YAML_PLUGIN_METADATA

#define YAML_CPP_SUPPORT_MERGE_KEYS

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <list>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "api/metadata/yaml/file.h"
#include "api/metadata/yaml/location.h"
#include "api/metadata/yaml/message.h"
#include "api/metadata/yaml/message_content.h"
#include "api/metadata/yaml/plugin_cleaning_data.h"
#include "api/metadata/yaml/set.h"
#include "api/metadata/yaml/tag.h"
#include "loot/metadata/plugin_metadata.h"

namespace loot {
template<typename T>
inline ::YAML::EMITTER_MANIP getNodeStyle(const std::vector<T>& objects) {
  if (objects.size() == 1 && emitAsScalar(objects.at(0))) {
    return YAML::Flow;
  }

  return YAML::Block;
}
}

namespace YAML {
template<>
struct convert<loot::PluginMetadata> {
  static Node encode(const loot::PluginMetadata& rhs) {
    Node node;
    node["name"] = rhs.GetName();

    if (rhs.GetGroup())
      node["group"] = rhs.GetGroup().value();

    if (!rhs.GetLoadAfterFiles().empty())
      node["after"] = rhs.GetLoadAfterFiles();
    if (!rhs.GetRequirements().empty())
      node["req"] = rhs.GetRequirements();
    if (!rhs.GetIncompatibilities().empty())
      node["inc"] = rhs.GetIncompatibilities();
    if (!rhs.GetMessages().empty())
      node["msg"] = rhs.GetMessages();
    if (!rhs.GetTags().empty())
      node["tag"] = rhs.GetTags();
    if (!rhs.GetDirtyInfo().empty())
      node["dirty"] = rhs.GetDirtyInfo();
    if (!rhs.GetCleanInfo().empty())
      node["clean"] = rhs.GetCleanInfo();
    if (!rhs.GetLocations().empty())
      node["url"] = rhs.GetLocations();

    return node;
  }

  static bool decode(const Node& node, loot::PluginMetadata& rhs) {
    if (!node.IsMap())
      throw RepresentationException(
          node.Mark(),
          "bad conversion: 'plugin metadata' object must be a map");
    if (!node["name"])
      throw RepresentationException(
          node.Mark(),
          "bad conversion: 'name' key missing from 'plugin metadata' object");

    try {
      rhs = loot::PluginMetadata(node["name"].as<std::string>());
    } catch (const std::regex_error& e) {
      throw RepresentationException(
          node.Mark(),
          std::string("bad conversion: invalid regex in 'name' key: ") +
              e.what());
    }

    if (node["group"])
      rhs.SetGroup(node["group"].as<std::string>());

    if (node["after"])
      rhs.SetLoadAfterFiles(node["after"].as<std::vector<loot::File>>());
    if (node["req"])
      rhs.SetRequirements(node["req"].as<std::vector<loot::File>>());
    if (node["inc"])
      rhs.SetIncompatibilities(node["inc"].as<std::vector<loot::File>>());
    if (node["msg"])
      rhs.SetMessages(node["msg"].as<std::vector<loot::Message>>());
    if (node["tag"])
      rhs.SetTags(node["tag"].as<std::vector<loot::Tag>>());
    if (node["dirty"]) {
      rhs.SetDirtyInfo(
          node["dirty"].as<std::vector<loot::PluginCleaningData>>());
    }
    if (node["clean"]) {
      rhs.SetCleanInfo(
          node["clean"].as<std::vector<loot::PluginCleaningData>>());
    }
    if (node["url"])
      rhs.SetLocations(node["url"].as<std::vector<loot::Location>>());

    return true;
  }
};

inline Emitter& operator<<(Emitter& out, const loot::PluginMetadata& rhs) {
  if (!rhs.HasNameOnly()) {
    out << BeginMap << Key << "name" << Value << YAML::SingleQuoted
        << rhs.GetName();

    const auto locations = rhs.GetLocations();
    if (!locations.empty()) {
      out << Key << "url" << Value << loot::getNodeStyle(locations)
          << locations;
    }

    if (rhs.GetGroup()) {
      out << Key << "group" << Value << YAML::SingleQuoted
          << rhs.GetGroup().value();
    }

    const auto after = rhs.GetLoadAfterFiles();
    if (!after.empty()) {
      out << Key << "after" << Value << loot::getNodeStyle(after) << after;
    }

    const auto req = rhs.GetRequirements();
    if (!req.empty()) {
      out << Key << "req" << Value << loot::getNodeStyle(req) << req;
    }

    const auto inc = rhs.GetIncompatibilities();
    if (!inc.empty()) {
      out << Key << "inc" << Value << loot::getNodeStyle(inc) << inc;
    }

    if (!rhs.GetMessages().empty())
      out << Key << "msg" << Value << rhs.GetMessages();

    const auto tags = rhs.GetTags();
    if (!tags.empty()) {
      out << Key << "tag" << Value << loot::getNodeStyle(tags) << tags;
    }

    if (!rhs.GetDirtyInfo().empty())
      out << Key << "dirty" << Value << rhs.GetDirtyInfo();

    if (!rhs.GetCleanInfo().empty())
      out << Key << "clean" << Value << rhs.GetCleanInfo();

    out << EndMap;
  }

  return out;
}
}

#endif
