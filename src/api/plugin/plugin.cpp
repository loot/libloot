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

#include "api/plugin/plugin.h"

#include <regex>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/locale.hpp>

#include "api/game/game.h"
#include "api/helpers/crc.h"
#include "api/helpers/logging.h"
#include "api/helpers/version.h"
#include "loot/exception/file_access_error.h"

using std::set;
using std::string;

namespace loot {
Plugin::Plugin(const GameType gameType,
               const boost::filesystem::path& dataPath,
               std::shared_ptr<LoadOrderHandler> loadOrderHandler,
               const std::string& name,
               const bool headerOnly) :
  name_(name),
  esPlugin(nullptr),
  isEmpty_(true),
  isActive_(false),
  loadsArchive_(false),
  crc_(0),
  numOverrideRecords_(0) {
  auto logger = getLogger();

  try {
    boost::filesystem::path filepath = dataPath / name_;

    // In case the plugin is ghosted.
    if (!boost::filesystem::exists(filepath) && boost::filesystem::exists(filepath.string() + ".ghost"))
      filepath += ".ghost";

    Load(filepath, gameType, headerOnly);

    auto ret = esp_plugin_is_empty(esPlugin.get(), &isEmpty_);
    if (ret != ESP_OK) {
      throw FileAccessError(name + " : Libespm error code: " + std::to_string(ret));
    }

    if (!headerOnly) {
      if (logger) {
        logger->trace("{}: Caching CRC value.", name_);
      }
      crc_ = GetCrc32(filepath);

      if (logger) {
        logger->trace("{}: Counting override FormIDs.", name_);
      }
      ret = esp_plugin_count_override_records(esPlugin.get(), &numOverrideRecords_);
      if (ret != ESP_OK) {
        throw FileAccessError(name + " : Libespm error code: " + std::to_string(ret));
      }
    }

    //Also read Bash Tags applied and version string in description.
    if (logger) {
      logger->trace("{}: Attempting to extract Bash Tags from the description.", name_);
    }

    string text = GetDescription();
    size_t pos1 = text.find("{{BASH:");
    if (pos1 != string::npos && pos1 + 7 != text.length()) {
      pos1 += 7;

      size_t pos2 = text.find("}}", pos1);
      if (pos2 != string::npos && pos1 != pos2) {
        text = text.substr(pos1, pos2 - pos1);

        std::vector<string> bashTags;
        boost::split(bashTags, text, [](char c) { return c == ','; });

        for (auto &tag : bashTags) {
          boost::trim(tag);
          tags_.insert(Tag(tag));

          if (logger) {
            logger->trace("{}: Extracted Bash Tag: {}", name_, tag);
          }
        }
      }
    }
    // Get whether the plugin is active or not.
    isActive_ = loadOrderHandler->IsPluginActive(name_);

    loadsArchive_ = LoadsArchive(name_, gameType, dataPath);
  } catch (std::exception& e) {
    if (logger) {
      logger->error("Cannot read plugin file \"{}\". Details: {}", name_, e.what());
    }
    throw FileAccessError((boost::format("Cannot read \"%1%\". Details: %2%") % name % e.what()).str());
  }

  if (logger) {
    logger->trace("{}: Plugin loading complete.", name_);
  }
}

std::string Plugin::GetName() const {
  return name_;
}

std::string Plugin::GetLowercasedName() const {
  return boost::locale::to_lower(name_);
}

std::string Plugin::GetVersion() const {
  return Version(GetDescription()).AsString();
}

std::vector<std::string> Plugin::GetMasters() const {
  char ** masters;
  uint8_t numMasters;
  auto ret = esp_plugin_masters(esPlugin.get(), &masters, &numMasters);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ + " : Libespm error code: " + std::to_string(ret));
  }

  std::vector<std::string> mastersVec(masters, masters + numMasters);
  esp_string_array_free(masters, numMasters);

  return mastersVec;
}

std::set<Tag> Plugin::GetBashTags() const {
  return tags_;
}

uint32_t Plugin::GetCRC() const {
  return crc_;
}

bool Plugin::IsMaster() const {
  bool isMaster;
  auto ret = esp_plugin_is_master(esPlugin.get(), &isMaster);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ + " : Libespm error code: " + std::to_string(ret));
  }

  return isMaster;
}

bool Plugin::IsLightMaster() const {
  bool isLightMaster;
  auto ret = esp_plugin_is_light_master(esPlugin.get(), &isLightMaster);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ + " : Libespm error code: " + std::to_string(ret));
  }

  return isLightMaster;
}

bool Plugin::IsEmpty() const {
  return isEmpty_;
}

bool Plugin::LoadsArchive() const {
  return loadsArchive_;
}

bool Plugin::DoFormIDsOverlap(const PluginInterface& plugin) const {
  try {
    auto otherPlugin = dynamic_cast<const Plugin&>(plugin);

    bool doPluginsOverlap;
    auto ret = esp_plugin_do_records_overlap(esPlugin.get(), otherPlugin.esPlugin.get(), &doPluginsOverlap);
    if (ret != ESP_OK) {
      throw FileAccessError(name_ + " : Libespm error code: " + std::to_string(ret));
    }

    return doPluginsOverlap;
  } catch (std::bad_cast&) {
    auto logger = getLogger();
    if (logger) {
      logger->error("Tried to check if FormIDs overlapped with a non-Plugin implementation of PluginInterface.");
    }
  }

  return false;
}

size_t Plugin::NumOverrideFormIDs() const {
  return numOverrideRecords_;
}

bool Plugin::IsValid(const std::string& filename, const GameType gameType, const boost::filesystem::path& dataPath) {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Checking to see if \"{}\" is a valid plugin.", filename);
  }

  //If the filename passed ends in '.ghost', that should be trimmed.
  std::string name;
  if (boost::iends_with(filename, ".ghost"))
    name = filename.substr(0, filename.length() - 6);
  else
    name = filename;

  // Check that the file has a valid extension.
  if (!hasPluginFileExtension(name, gameType))
    return false;

  bool isValid;
  auto path = dataPath / filename;
  int ret = esp_plugin_is_valid(GetEspluginGameId(gameType), path.string().c_str(), true, &isValid);

  if (ret != ESP_OK || !isValid) {
    if (logger) {
      logger->warn("The file \"{}\" is not a valid plugin.", filename);
    }
  }

  return (ret == ESP_OK && isValid)
    || Plugin::IsValid(filename + ".ghost", gameType, dataPath);
}

uintmax_t Plugin::GetFileSize(const std::string & filename, const boost::filesystem::path& dataPath) {
  boost::filesystem::path realPath = dataPath / filename;
  if (!boost::filesystem::exists(realPath))
    realPath += ".ghost";

  return boost::filesystem::file_size(realPath);
}

bool Plugin::operator < (const Plugin & rhs) const {
  return boost::ilexicographical_compare(name_, rhs.name_);;
}

bool Plugin::IsActive() const {
  return isActive_;
}

void Plugin::Load(const boost::filesystem::path& path, GameType gameType, bool headerOnly) {
  ::Plugin * plugin;
  int ret = esp_plugin_new(&plugin, GetEspluginGameId(gameType), path.string().c_str());
  if (ret != ESP_OK) {
      throw FileAccessError(path.string() + " : Libespm error code: " + std::to_string(ret));
  }

  esPlugin = std::shared_ptr<std::remove_pointer<::Plugin>::type>(plugin, esp_plugin_free);

  ret = esp_plugin_parse(esPlugin.get(), headerOnly);
  if (ret != ESP_OK) {
    throw FileAccessError(path.string() + " : Libespm error code: " + std::to_string(ret));
  }
}

std::string Plugin::GetDescription() const {
  char * description;
  auto ret = esp_plugin_description(esPlugin.get(), &description);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ + " : Libespm error code: " + std::to_string(ret));
  }
  if (description == nullptr) {
    return "";
  }

  string descriptionStr = description;
  esp_string_free(description);

  return descriptionStr;
}

std::string Plugin::GetArchiveFileExtension(const GameType gameType) {
  if (gameType == GameType::fo4)
    return ".ba2";
  else
    return ".bsa";
}

bool Plugin::LoadsArchive(const std::string& pluginName, const GameType gameType, const boost::filesystem::path& dataPath) {
  // Get whether the plugin loads an archive (BSA/BA2) or not.
  const string archiveExtension = GetArchiveFileExtension(gameType);

  if (gameType == GameType::tes5) {
    // Skyrim plugins only load BSAs that exactly match their basename.
    return boost::filesystem::exists(dataPath / (pluginName.substr(0, pluginName.length() - 4) + archiveExtension));
  } else if (gameType != GameType::tes4 || boost::iends_with(pluginName, ".esp")) {
    //Oblivion .esp files and FO3, FNV, FO4 plugins can load archives which begin with the plugin basename.
    string basename = pluginName.substr(0, pluginName.length() - 4);
    for (boost::filesystem::directory_iterator it(dataPath); it != boost::filesystem::directory_iterator(); ++it) {
      if (boost::iequals(it->path().extension().string(), archiveExtension) && boost::istarts_with(it->path().filename().string(), basename)) {
        return true;
      }
    }
  }

  return false;
}

unsigned int Plugin::GetEspluginGameId(GameType gameType) {
  switch (gameType) {
  case GameType::tes4:
    return ESP_GAME_OBLIVION;
  case GameType::tes5:
    return ESP_GAME_SKYRIM;
  case GameType::tes5se:
    return ESP_GAME_SKYRIMSE;
  case GameType::fo3:
    return ESP_GAME_FALLOUT3;
  case GameType::fonv:
    return ESP_GAME_FALLOUTNV;
  default:
    return ESP_GAME_FALLOUT4;
  }
}

bool hasPluginFileExtension(const std::string& filename, GameType gameType) {
  bool espOrEsm = boost::iends_with(filename, ".esp") || boost::iends_with(filename, ".esm");
  bool lightMaster = (gameType == GameType::fo4 || gameType == GameType::tes5se)
    && boost::iends_with(filename, ".esl");

  return espOrEsm || lightMaster;
}
}
