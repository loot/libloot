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
    <http://www.gnu.org/licenses/>.
    */

#ifndef LOOT_API_METADATA_CONDITION_EVALUATOR
#define LOOT_API_METADATA_CONDITION_EVALUATOR

#include <filesystem>
#include <string>

#include <loot_condition_interpreter.h>

#include "api/game/game_cache.h"
#include "api/game/load_order_handler.h"
#include "loot/metadata/plugin_cleaning_data.h"
#include "loot/metadata/plugin_metadata.h"

namespace loot {
class ConditionEvaluator {
public:
  explicit ConditionEvaluator(const GameType gameType,
    const std::filesystem::path& dataPath);

  bool Evaluate(const std::string& condition);
  PluginMetadata EvaluateAll(const PluginMetadata& pluginMetadata);

  void ClearConditionCache();
  void RefreshActivePluginsState(std::vector<std::string> activePluginNames);
  void RefreshLoadedPluginsState(std::vector<std::shared_ptr<const PluginInterface>> plugins);

private:
  bool Evaluate(const PluginCleaningData& cleaningData,
    const std::string& pluginName);

  std::shared_ptr<lci_state> lciState_;
};

void ParseCondition(const std::string& condition);
}

#endif
