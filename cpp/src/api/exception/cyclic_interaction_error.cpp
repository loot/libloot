/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2018    WrinklyNinja

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
#include "loot/exception/cyclic_interaction_error.h"

namespace {
using loot::EdgeType;

std::string describeEdgeType(EdgeType edgeType) {
  switch (edgeType) {
    case EdgeType::hardcoded:
      return "Hardcoded";
    case EdgeType::masterFlag:
      return "Master Flag";
    case EdgeType::master:
      return "Master";
    case EdgeType::masterlistRequirement:
      return "Masterlist Requirement";
    case EdgeType::userRequirement:
      return "User Requirement";
    case EdgeType::masterlistLoadAfter:
      return "Masterlist Load After";
    case EdgeType::userLoadAfter:
      return "User Load After";
    case EdgeType::masterlistGroup:
      return "Masterlist Group";
    case EdgeType::userGroup:
      return "User Group";
    case EdgeType::recordOverlap:
      return "Record Overlap";
    case EdgeType::assetOverlap:
      return "Asset Overlap";
    case EdgeType::tieBreak:
      return "Tie Break";
    default:
      return "Unknown";
  }
}
}

namespace loot {
// A.esp --[Master Flag]-> B.esp --Group->
std::string describeCycle(const std::vector<Vertex>& cycle) {
  std::string text;
  for (const auto& vertex : cycle) {
    text += vertex.GetName();
    if (vertex.GetTypeOfEdgeToNextVertex().has_value()) {
      text += " --[" +
              describeEdgeType(vertex.GetTypeOfEdgeToNextVertex().value()) +
              "]-> ";
    }
  }
  if (!cycle.empty()) {
    text += cycle.at(0).GetName();
  }

  return text;
}

CyclicInteractionError::CyclicInteractionError(std::vector<Vertex> cycle) :
    std::runtime_error("Cyclic interaction detected: " + describeCycle(cycle)),
    cycle_(cycle) {}

std::vector<Vertex> CyclicInteractionError::GetCycle() const { return cycle_; }
}
