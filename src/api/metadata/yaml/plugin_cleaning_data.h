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

#ifndef LOOT_YAML_PLUGIN_CLEANING_DATA
#define LOOT_YAML_PLUGIN_CLEANING_DATA

#define YAML_CPP_SUPPORT_MERGE_KEYS

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <string>

#include "loot/metadata/plugin_cleaning_data.h"

namespace YAML {
template<>
struct convert<loot::PluginCleaningData> {
  static Node encode(const loot::PluginCleaningData& rhs) {
    Node node;
    node["crc"] = rhs.GetCRC();
    node["util"] = rhs.GetCleaningUtility();
    node["detail"] = rhs.GetDetail();

    if (rhs.GetITMCount() > 0)
      node["itm"] = rhs.GetITMCount();
    if (rhs.GetDeletedReferenceCount() > 0)
      node["udr"] = rhs.GetDeletedReferenceCount();
    if (rhs.GetDeletedNavmeshCount() > 0)
      node["nav"] = rhs.GetDeletedNavmeshCount();

    return node;
  }

  static bool decode(const Node& node, loot::PluginCleaningData& rhs) {
    if (!node.IsMap())
      throw RepresentationException(
          node.Mark(), "bad conversion: 'cleaning data' object must be a map");
    if (!node["crc"])
      throw RepresentationException(
          node.Mark(),
          "bad conversion: 'crc' key missing from 'cleaning data' object");
    if (!node["util"])
      throw RepresentationException(
          node.Mark(),
          "bad conversion: 'util' key missing from 'cleaning data' object");

    uint32_t crc = node["crc"].as<uint32_t>();
    int itm = 0, ref = 0, nav = 0;

    if (node["itm"])
      itm = node["itm"].as<unsigned int>();
    if (node["udr"])
      ref = node["udr"].as<unsigned int>();
    if (node["nav"])
      nav = node["nav"].as<unsigned int>();

    std::string utility = node["util"].as<std::string>();

    std::vector<loot::MessageContent> detail;
    if (node["detail"]) {
      if (node["detail"].IsSequence())
        detail = node["detail"].as<std::vector<loot::MessageContent>>();
      else {
        detail.push_back(
            loot::MessageContent(node["detail"].as<std::string>()));
      }
    }

    // Check now that at least one item in info is English if there are multiple
    // items.
    if (detail.size() > 1) {
      bool found = false;
      for (const auto& mc : detail) {
        if (mc.GetLanguage() == loot::MessageContent::DEFAULT_LANGUAGE)
          found = true;
      }
      if (!found)
        throw RepresentationException(node.Mark(),
                                      "bad conversion: multilingual messages "
                                      "must contain an English info string");
    }

    rhs = loot::PluginCleaningData(crc, utility, detail, itm, ref, nav);

    return true;
  }
};

inline Emitter& operator<<(Emitter& out, const loot::PluginCleaningData& rhs) {
  out << BeginMap << Key << "crc" << Value << Hex << rhs.GetCRC() << Dec << Key
      << "util" << Value << YAML::SingleQuoted << rhs.GetCleaningUtility();

  if (!rhs.GetDetail().empty()) {
    if (rhs.GetDetail().size() == 1)
      out << Key << "detail" << Value << YAML::SingleQuoted
          << rhs.GetDetail().front().GetText();
    else
      out << Key << "detail" << Value << rhs.GetDetail();
  }

  if (rhs.GetITMCount() > 0)
    out << Key << "itm" << Value << rhs.GetITMCount();
  if (rhs.GetDeletedReferenceCount() > 0)
    out << Key << "udr" << Value << rhs.GetDeletedReferenceCount();
  if (rhs.GetDeletedNavmeshCount() > 0)
    out << Key << "nav" << Value << rhs.GetDeletedNavmeshCount();

  out << EndMap;

  return out;
}
}

#endif
