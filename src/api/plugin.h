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
#ifndef LOOT_API_PLUGIN
#define LOOT_API_PLUGIN

#include <cstdint>
#include <esplugin.hpp>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "api/game/load_order_handler.h"
#include "loot/enum/game_type.h"
#include "loot/metadata/plugin_metadata.h"
#include "loot/plugin_interface.h"

namespace loot {
class GameCache;

// An interface containing member functions that are used when sorting plugins.
class PluginSortingInterface : public PluginInterface {
public:
  virtual size_t GetOverrideRecordCount() const = 0;
  virtual uint32_t GetRecordAndGroupCount() const = 0;

  virtual size_t GetOverlapSize(
      const std::vector<const PluginInterface*>& plugins) const = 0;

  virtual size_t GetAssetCount() const = 0;
  virtual bool DoAssetsOverlap(const PluginSortingInterface& plugin) const = 0;
};

class Plugin final : public PluginSortingInterface {
public:
  explicit Plugin(const GameType gameType,
                  const GameCache& gameCache,
                  std::filesystem::path pluginPath,
                  const bool headerOnly);

  void ResolveRecordIds(Vec_PluginMetadata* pluginsMetadata) const;

  std::string GetName() const override;
  std::optional<float> GetHeaderVersion() const override;
  std::optional<std::string> GetVersion() const override;
  std::vector<std::string> GetMasters() const override;
  std::vector<Tag> GetBashTags() const override;
  std::optional<uint32_t> GetCRC() const override;

  bool IsMaster() const override;

  bool IsLightPlugin() const override;
  bool IsMediumPlugin() const override;
  bool IsOverridePlugin() const override;

  bool IsValidAsLightPlugin() const override;
  bool IsValidAsMediumPlugin() const override;
  bool IsValidAsOverridePlugin() const override;
  bool IsEmpty() const override;
  bool LoadsArchive() const override;
  bool DoRecordsOverlap(const PluginInterface& plugin) const override;
  size_t GetOverlapSize(
      const std::vector<const PluginInterface*>& plugins) const override;

  // Load ordering functions.
  size_t GetOverrideRecordCount() const override;
  uint32_t GetRecordAndGroupCount() const override;

  size_t GetAssetCount() const;
  bool DoAssetsOverlap(const PluginSortingInterface& plugin) const;

  // Validity checks.
  static bool IsValid(const GameType gameType,
                      const std::filesystem::path& pluginPath);

  static std::unique_ptr<Vec_PluginMetadata,
                         decltype(&esp_plugins_metadata_free)>
      GetPluginsMetadata(
      std::vector<const Plugin*>);

private:
  void Load(const std::filesystem::path& path,
            GameType gameType,
            bool headerOnly);
  std::string GetDescription() const;

  static unsigned int GetEspluginGameId(GameType gameType);

  std::string name_;
  std::unique_ptr<::Plugin, decltype(&esp_plugin_free)> esPlugin;
  bool isEmpty_;  // Does the plugin contain any records other than the TES4
                  // header?
  std::optional<std::string> version_;  // Obtained from description field.
  std::optional<uint32_t> crc_;
  std::vector<Tag> tags_;
  std::vector<std::filesystem::path> archivePaths_;
  std::map<uint64_t, std::set<uint64_t>> archiveAssets_;
};

std::string GetArchiveFileExtension(const GameType gameType);

bool hasPluginFileExtension(std::string filename, GameType gameType);

bool equivalent(const std::filesystem::path& path1,
                const std::filesystem::path& path2);

std::filesystem::path replaceExtension(std::filesystem::path path,
                                       const std::string& newExtension);
}

#endif
