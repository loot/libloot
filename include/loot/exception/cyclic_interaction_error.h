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

#ifndef LOOT_EXCEPTION_CYCLIC_INTERACTION_ERROR
#define LOOT_EXCEPTION_CYCLIC_INTERACTION_ERROR

#include <stdexcept>
#include <string>
#include <vector>

namespace loot {
/**
 * @brief An enum representing the different possible types of interactions
 *        between plugins or groups.
 */
enum struct EdgeType : unsigned int {
  hardcoded,
  masterFlag,
  master,
  masterlistRequirement,
  userRequirement,
  masterlistLoadAfter,
  userLoadAfter,
  group,
  overlap,
  tieBreak,
};

/**
 * @brief A class representing a plugin or group vertex in a cyclic interaction
 *        path, and the type of the interaction with the next vertex in the
 *        path.
 */
class Vertex {
public:
  /**
   * @brief Construct a Vertex with the given name and out edge type.
   * @param name The name of the plugin or group that this vertex represents.
   * @param outEdgeType The type of the edge going out from this vertex.
   */
  Vertex(std::string name, EdgeType outEdgeType);

  /**
   * @brief Get the name of the plugin or group.
   * @return The name of the plugin or group.
   */
  std::string GetName() const;

  /**
   * @brief Get the type of the edge going to the next vertex.
   * @details Each edge goes from the vertex that loads earlier to the vertex
   *          that loads later.
   * @return The edge type.
   */
  EdgeType GetTypeOfEdgeToNextVertex() const;

private:
  std::string name_;
  EdgeType outEdgeType_;
};

/**
 * @brief An exception class thrown if a cyclic interaction is detected when
 *        sorting a load order.
 */
class CyclicInteractionError : public std::runtime_error {
public:
  /**
   * @brief Construct an exception detailing a plugin or group graph cycle.
   * @param cycle A representation of the cyclic path.
   */
  CyclicInteractionError(std::vector<Vertex> cycle);

  /**
   * @brief Get a representation of the cyclic path.
   * @details Each Vertex is the name of a graph element (plugin or group) and
   *          the type of the edge going to the next Vertex. The last Vertex
   *          has an edge going to the first Vertex.
   * @return A vector of Vertex elements representing the cyclic path.
   */
  std::vector<Vertex> GetCycle();

private:
  const std::vector<Vertex> cycle_;
};
}

#endif
