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
#ifndef LOOT_YAML_FILE
#define LOOT_YAML_FILE

#define YAML_CPP_SUPPORT_MERGE_KEYS

#include <yaml-cpp/yaml.h>

#include <string>

#include "api/helpers/text.h"
#include "api/metadata/condition_evaluator.h"
#include "api/metadata/yaml/message_content.h"
#include "loot/metadata/file.h"

namespace loot {
inline bool emitAsScalar(const File& file) {
  return file.GetCondition().empty() && file.GetDetail().empty() &&
         file.GetDisplayName().empty() && file.GetConstraint().empty();
}
}

namespace YAML {
template<>
struct convert<loot::File> {
  static Node encode(const loot::File& rhs) {
    Node node;
    node["name"] = std::string(rhs.GetName());

    if (!rhs.GetCondition().empty()) {
      node["condition"] = rhs.GetCondition();
    }

    if (!rhs.GetConstraint().empty()) {
      node["constraint"] = rhs.GetConstraint();
    }

    if (!rhs.GetDisplayName().empty()) {
      node["display"] = rhs.GetDisplayName();
    }

    if (!rhs.GetDetail().empty()) {
      node["detail"] = rhs.GetDetail();
    }

    return node;
  }

  static bool decode(const Node& node, loot::File& rhs) {
    if (!node.IsMap() && !node.IsScalar()) {
      throw RepresentationException(
          node.Mark(), "bad conversion: 'file' object must be a map or scalar");
    }

    if (node.IsMap()) {
      if (!node["name"]) {
        throw RepresentationException(
            node.Mark(),
            "bad conversion: 'name' key missing from 'file' map object");
      }

      std::string name = node["name"].as<std::string>();
      std::string condition, constraint, display;
      std::vector<loot::MessageContent> detail;
      if (node["condition"]) {
        condition = node["condition"].as<std::string>();
      }

      if (node["constraint"]) {
        constraint = node["constraint"].as<std::string>();
      }

      if (node["display"]) {
        display = node["display"].as<std::string>();
      }

      if (node["detail"]) {
        if (node["detail"].IsSequence()) {
          detail = node["detail"].as<std::vector<loot::MessageContent>>();
        } else {
          detail.push_back(
              loot::MessageContent(node["detail"].as<std::string>()));
        }
      }

      // Check now that at least one item in info is English if there are
      // multiple items.
      if (detail.size() > 1) {
        const auto found = std::any_of(
            detail.begin(), detail.end(), [](const loot::MessageContent& mc) {
              return mc.GetLanguage() == loot::MessageContent::DEFAULT_LANGUAGE;
            });

        if (!found) {
          throw RepresentationException(node.Mark(),
                                        "bad conversion: multilingual messages "
                                        "must contain an English info string");
        }
      }

      // Test condition syntax.
      try {
        loot::ParseCondition(condition);
        loot::ParseCondition(constraint);
      } catch (const std::exception& e) {
        throw RepresentationException(
            node.Mark(),
            std::string("bad conversion: invalid condition syntax: ") +
                e.what());
      }

      rhs = loot::File(name, display, condition, detail, constraint);
    } else {
      rhs = loot::File(node.as<std::string>());
    }

    return true;
  }
};

inline Emitter& operator<<(Emitter& out, const loot::File& rhs) {
  if (loot::emitAsScalar(rhs)) {
    out << YAML::SingleQuoted << std::string(rhs.GetName());
  } else {
    out << BeginMap << Key << "name" << Value << YAML::SingleQuoted
        << std::string(rhs.GetName());

    if (!rhs.GetCondition().empty()) {
      out << Key << "condition" << Value << YAML::SingleQuoted
          << rhs.GetCondition();
    }

    if (!rhs.GetDisplayName().empty()) {
      out << Key << "display" << Value << YAML::SingleQuoted
          << rhs.GetDisplayName();
    }

    if (!rhs.GetConstraint().empty()) {
      out << Key << "constraint" << Value << YAML::SingleQuoted
          << rhs.GetConstraint();
    }

    if (rhs.GetDetail().size() == 1) {
      out << Key << "detail" << Value << YAML::SingleQuoted
          << rhs.GetDetail().front().GetText();
    } else if (!rhs.GetDetail().empty()) {
      out << Key << "detail" << Value << rhs.GetDetail();
    }

    out << EndMap;
  }

  return out;
}
}

#endif
