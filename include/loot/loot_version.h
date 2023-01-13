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

#ifndef LOOT_LOOT_VERSION
#define LOOT_LOOT_VERSION

#include <string>

#include "loot/api_decorator.h"

namespace loot {
/** @brief libloot's major version number. */
inline constexpr unsigned int LIBLOOT_VERSION_MAJOR = 0;

/** @brief libloot's minor version number. */
inline constexpr unsigned int LIBLOOT_VERSION_MINOR = 19;

/** @brief libloot's patch version number. */
inline constexpr unsigned int LIBLOOT_VERSION_PATCH = 2;

/**
 * @brief Get the library version.
 * @return A string of the form "major.minor.patch".
 */
LOOT_API std::string GetLiblootVersion();

/**
 * @brief Get the source control revision that libloot was built from.
 * @return A string containing the revision ID.
 */
LOOT_API std::string GetLiblootRevision();
}

#endif
