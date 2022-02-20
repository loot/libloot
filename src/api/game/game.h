/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2013-2016    WrinklyNinja

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

#ifndef LOOT_API_GAME_GAME
#define LOOT_API_GAME_GAME

#include <filesystem>
#include <string>

#include "api/api_database.h"
#include "api/game/game_cache.h"
#include "api/game/load_order_handler.h"
#include "api/metadata/condition_evaluator.h"
#include "loot/game_interface.h"

namespace loot {
class Game final : public GameInterface {
public:
  explicit Game(const GameType gameType,
                const std::filesystem::path& gamePath,
                const std::filesystem::path& gameLocalDataPath = "");

  // Internal Methods //
  //////////////////////

  GameType Type() const;
  std::filesystem::path DataPath() const;

  GameCache& GetCache();
  LoadOrderHandler& GetLoadOrderHandler();

  // Game Interface Methods //
  ////////////////////////////

  DatabaseInterface& GetDatabase() override;

  bool IsValidPlugin(const std::string& plugin) const override;

  void LoadPlugins(const std::vector<std::string>& plugins,
                   bool loadHeadersOnly) override;

  const PluginInterface* GetPlugin(
      const std::string& pluginName) const override;

  std::vector<const PluginInterface*> GetLoadedPlugins() const override;

  void IdentifyMainMasterFile(const std::string& masterFile) override;

  std::vector<std::string> SortPlugins(
      const std::vector<std::string>& plugins) override;

  void LoadCurrentLoadOrderState() override;

  bool IsLoadOrderAmbiguous() const override;

  bool IsPluginActive(const std::string& pluginName) const override;

  std::vector<std::string> GetLoadOrder() const override;

  void SetLoadOrder(const std::vector<std::string>& loadOrder) override;

private:
  void CacheArchives();

  GameType type_;
  std::filesystem::path gamePath_;

  GameCache cache_;
  LoadOrderHandler loadOrderHandler_;
  std::shared_ptr<ConditionEvaluator> conditionEvaluator_;
  ApiDatabase database_;

  std::string masterFilename_;
};
}
#endif
