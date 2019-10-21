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

#ifndef LOOT_API_GAME_LOAD_ORDER_HANDLER
#define LOOT_API_GAME_LOAD_ORDER_HANDLER

#include <filesystem>
#include <list>
#include <vector>
#include <string>
#include <unordered_set>

#include <libloadorder.hpp>

#include "loot/enum/game_type.h"

namespace loot {
class LoadOrderHandler {
public:
  LoadOrderHandler();
  ~LoadOrderHandler();

  void Init(const GameType& game,
            const std::filesystem::path& gamePath,
            const std::filesystem::path& gameLocalAppData = "");

  void LoadCurrentState();

  std::vector<std::string> GetLoadOrder() const;

  std::vector<std::string> GetActivePlugins() const;

  std::vector<std::string> GetImplicitlyActivePlugins() const;

  bool IsPluginActive(const std::string& pluginName) const;

  void SetLoadOrder(const std::vector<std::string>& loadOrder) const;

private:
  void HandleError(const std::string& operation, unsigned int returnCode) const;

  lo_game_handle gh_;
};
}

#endif
