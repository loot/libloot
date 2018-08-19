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
#include <list>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include <esplugin.hpp>

#include "api/game/load_order_handler.h"
#include "loot/enum/game_type.h"
#include "loot/metadata/plugin_metadata.h"
#include "loot/plugin_interface.h"

namespace loot {
class Plugin : public PluginInterface {
public:
  Plugin(const GameType gameType,
         const boost::filesystem::path& dataPath,
         std::shared_ptr<LoadOrderHandler> loadOrderHandler,
         const std::string& name,
         const bool headerOnly);

  std::string GetName() const;
  std::string GetLowercasedName() const;
  std::string GetVersion() const;
  std::vector<std::string> GetMasters() const;
  std::set<Tag> GetBashTags() const;
  uint32_t GetCRC() const;

  bool IsMaster() const;
  bool IsLightMaster() const;
  bool IsEmpty() const;
  bool LoadsArchive() const;
  bool DoFormIDsOverlap(const PluginInterface& plugin) const;

  bool IsActive() const;

  // Load ordering functions.
  size_t NumOverrideFormIDs() const;

  // Validity checks.
  static bool IsValid(const std::string& filename,
                      const GameType gameType,
                      const boost::filesystem::path& dataPath);
  static uintmax_t GetFileSize(const std::string& filename,
                               const boost::filesystem::path& dataPath);

  bool operator<(const Plugin& rhs) const;

private:
  void Load(const boost::filesystem::path& path,
            GameType gameType,
            bool headerOnly);
  std::string GetDescription() const;

  static std::string GetArchiveFileExtension(const GameType gameType);
  static bool LoadsArchive(const std::string& pluginName,
                           const GameType gameType,
                           const boost::filesystem::path& dataPath);
  static unsigned int GetEspluginGameId(GameType gameType);

  bool isEmpty_;  // Does the plugin contain any records other than the TES4
                  // header?
  bool isActive_;
  bool loadsArchive_;
  const std::string name_;
  std::string version_;  // Obtained from description field.
  uint32_t crc_;
  std::set<Tag> tags_;

  // Useful caches.
  size_t numOverrideRecords_;

  std::shared_ptr<std::remove_pointer<::Plugin>::type> esPlugin;
};

bool hasPluginFileExtension(const std::string& filename, GameType gameType);
}

#endif
