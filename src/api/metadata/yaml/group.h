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
#ifndef LOOT_YAML_GROUP
#define LOOT_YAML_GROUP

#define YAML_CPP_SUPPORT_MERGE_KEYS

#include <set>
#include <string>

#include <yaml-cpp/yaml.h>

#include "api/metadata/yaml/set.h"
#include "loot/metadata/group.h"

namespace YAML {
template<>
struct convert<loot::Group> {
  static Node encode(const loot::Group& rhs) {
    Node node;
    node["name"] = rhs.GetName();

    if (!rhs.GetDescription().empty()) {
      node["description"] = rhs.GetDescription();
    }

    auto afterGroups = rhs.GetAfterGroups();
    if (!afterGroups.empty())
      node["after"] = afterGroups;

    return node;
  }

  static bool decode(const Node& node, loot::Group& rhs) {
    if (!node.IsMap())
      throw RepresentationException(
          node.Mark(), "bad conversion: 'group' object must be a map");

    if (!node["name"])
      throw RepresentationException(
          node.Mark(),
          "bad conversion: 'name' key missing from 'file' map object");

    std::string name = node["name"].as<std::string>();
    std::string description;
    std::vector<std::string> afterGroups;

    if (node["description"]) {
      description = node["description"].as<std::string>();
    }

    if (node["after"]) {
      afterGroups = node["after"].as<std::vector<std::string>>();
    }

    rhs = loot::Group(name, afterGroups, description);

    return true;
  }
};

inline Emitter& operator<<(Emitter& out, const loot::Group& rhs) {
  out << BeginMap << Key << "name" << Value << YAML::SingleQuoted
      << rhs.GetName();

  if (!rhs.GetDescription().empty()) {
    out << Key << "description" << Value << YAML::SingleQuoted
        << rhs.GetDescription();
  }

  auto afterGroups = rhs.GetAfterGroups();
  if (!afterGroups.empty()) {
    out << Key << "after" << Value << afterGroups;
  }

  out << EndMap;

  return out;
}
}

#endif
