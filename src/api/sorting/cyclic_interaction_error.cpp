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

namespace loot {
Vertex::Vertex(std::string name, EdgeType outEdgeType) : name_(name), outEdgeType_(outEdgeType) {}

std::string Vertex::GetName() const {
  return name_;
}

EdgeType Vertex::GetTypeOfEdgeToNextVertex() const {
  return outEdgeType_;
}

std::string describe(EdgeType edgeType) {
  switch (edgeType) {
  case EdgeType::Hardcoded:
    return "Hardcoded";
  case EdgeType::MasterFlag:
    return "Master Flag";
  case EdgeType::Master:
    return "Master";
  case EdgeType::Requirement:
    return "Requirement";
  case EdgeType::LoadAfter:
    return "Load After";
  case EdgeType::Group:
    return "Group";
  case EdgeType::Overlap:
    return "Overlap";
  case EdgeType::TieBreak:
    return "Tie Break";
  default:
    return "Unknown";
  }
}


// A.esp --[Master Flag]-> B.esp --Group->
std::string describeCycle(const std::vector<Vertex>& cycle) {
  std::string text;
  for (const auto& vertex : cycle) {
    text += vertex.GetName() + " --[" + describe(vertex.GetTypeOfEdgeToNextVertex()) + "]-> ";
  }
  if (!cycle.empty()) {
    text += cycle[0].GetName();
  }

  return text;
}

CyclicInteractionError::CyclicInteractionError(std::vector<Vertex> cycle) :
    std::runtime_error("Cyclic interaction detected: " + describeCycle(cycle)),
    cycle_(cycle) {}

std::vector<Vertex> CyclicInteractionError::GetCycle() { return cycle_; }
}
