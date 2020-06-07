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

#ifndef LOOT_API_HELPERS_COLLECTIONS
#define LOOT_API_HELPERS_COLLECTIONS

#include <algorithm>
#include <vector>

namespace loot {
// Append second to first, skipping any elements that are already present in
// first. Although this is O(U * M), both input vectors are expected to be
// small (with tens of elements being an unusually large number).
template<typename T>
std::vector<T> mergeVectors(std::vector<T> first,
                            const std::vector<T>& second) {
  auto initialSizeOfFirst = first.size();
  for (const auto& element : second) {
    auto end = first.cbegin() + initialSizeOfFirst;

    if (std::find(first.cbegin(), end, element) == end) {
      first.push_back(element);
    }
  }

  return first;
}

// Returns the elements in first that are not in second. Although this is
// O(F * S), both input vectors are expected to be small (with tens of elements
// being an unusually large number).
template<typename T>
std::vector<T> diffVectors(const std::vector<T>& first,
                           const std::vector<T>& second) {
  std::vector<T> result;

  for (const auto& element : first) {
    if (std::find(second.begin(), second.end(), element) == second.end()) {
      result.push_back(element);
    }
  }

  return result;
}
}

#endif
