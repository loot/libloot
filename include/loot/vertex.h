/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2016    WrinklyNinja

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

#ifndef LOOT_VERTEX
#define LOOT_VERTEX

#include <optional>
#include <string>

#include "loot/api_decorator.h"
#include "loot/enum/edge_type.h"

namespace loot {
/**
 * @brief A class representing a plugin or group vertex in a path, and the
          type of the edge to the next vertex in the path if one exists.
 */
class Vertex {
public:
  /**
   * @brief Construct a Vertex with the given name and no out edge.
   * @param name The name of the plugin or group that this vertex represents.
   */
  LOOT_API explicit Vertex(std::string name);

  /**
   * @brief Construct a Vertex with the given name and out edge type.
   * @param name The name of the plugin or group that this vertex represents.
   * @param outEdgeType The type of the edge going out from this vertex.
   */
  LOOT_API explicit Vertex(std::string name, EdgeType outEdgeType);

  /**
   * @brief Get the name of the plugin or group.
   * @return The name of the plugin or group.
   */
  LOOT_API std::string GetName() const;

  /**
   * @brief Get the type of the edge going to the next vertex.
   * @details Each edge goes from the vertex that loads earlier to the vertex
   *          that loads later.
   * @return The edge type.
   */
  LOOT_API std::optional<EdgeType> GetTypeOfEdgeToNextVertex() const;

private:
  std::string name_;
  std::optional<EdgeType> outEdgeType_;
};
}

#endif
