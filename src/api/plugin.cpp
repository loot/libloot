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

#include "api/plugin.h"

#include <filesystem>
#include <regex>

#include <boost/algorithm/string.hpp>
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
               std::shared_ptr<GameCache> gameCache,
               std::filesystem::path pluginPath,
               const bool headerOnly) :
    name_(pluginPath.filename().u8string()),
    esPlugin(nullptr),
    isEmpty_(true),
    loadsArchive_(false),
    numOverrideRecords_(0) {
  auto logger = getLogger();

  try {
    // In case the plugin is ghosted.
    if (!std::filesystem::exists(pluginPath)) {
      pluginPath += ".ghost";
    }

    Load(pluginPath, gameType, headerOnly);

    auto ret = esp_plugin_is_empty(esPlugin.get(), &isEmpty_);
    if (ret != ESP_OK) {
      throw FileAccessError(name_ +
                            " : esplugin error code: " + std::to_string(ret));
    }

    if (!headerOnly) {
      if (logger) {
        logger->trace("{}: Caching CRC value.", name_);
      }
      crc_ = GetCrc32(pluginPath);

      if (logger) {
        logger->trace("{}: Counting override FormIDs.", name_);
      }
      ret = esp_plugin_count_override_records(esPlugin.get(),
                                              &numOverrideRecords_);
      if (ret != ESP_OK) {
        throw FileAccessError(name_ +
                              " : esplugin error code: " + std::to_string(ret));
      }
    }

    // Also read Bash Tags applied and version string in description.
    if (logger) {
      logger->trace("{}: Attempting to extract Bash Tags from the description.",
                    name_);
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

        for (auto& tag : bashTags) {
          boost::trim(tag);
          tags_.insert(Tag(tag));

          if (logger) {
            logger->trace("{}: Extracted Bash Tag: {}", name_, tag);
          }
        }
      }
    }

    loadsArchive_ = LoadsArchive(gameType, gameCache, pluginPath);
  } catch (std::exception& e) {
    if (logger) {
      logger->error(
          "Cannot read plugin file \"{}\". Details: {}", name_, e.what());
    }
    throw FileAccessError("Cannot read \"" + name_ + "\". Details: " + e.what());
  }

  if (logger) {
    logger->trace("{}: Plugin loading complete.", name_);
  }
}

std::string Plugin::GetName() const { return name_; }

float Plugin::GetHeaderVersion() const {
  float version;
  auto ret = esp_plugin_header_version(esPlugin.get(), &version);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
      " : esplugin error code: " + std::to_string(ret));
  }

  return version;
}

std::optional<std::string> Plugin::GetVersion() const {
  return ExtractVersion(GetDescription());
}

std::vector<std::string> Plugin::GetMasters() const {
  char** masters;
  uint8_t numMasters;
  auto ret = esp_plugin_masters(esPlugin.get(), &masters, &numMasters);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  std::vector<std::string> mastersVec(masters, masters + numMasters);
  esp_string_array_free(masters, numMasters);

  return mastersVec;
}

std::set<Tag> Plugin::GetBashTags() const { return tags_; }

std::optional<uint32_t> Plugin::GetCRC() const { return crc_; }

bool Plugin::IsMaster() const {
  bool isMaster;
  auto ret = esp_plugin_is_master(esPlugin.get(), &isMaster);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  return isMaster;
}

bool Plugin::IsLightMaster() const {
  bool isLightMaster;
  auto ret = esp_plugin_is_light_master(esPlugin.get(), &isLightMaster);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }

  return isLightMaster;
}

bool Plugin::IsValidAsLightMaster() const
{
  bool isValid;
  auto ret = esp_plugin_is_valid_as_light_master(esPlugin.get(), &isValid);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
      " : esplugin error code: " + std::to_string(ret));
  }

  return isValid;
}

bool Plugin::IsEmpty() const { return isEmpty_; }

bool Plugin::LoadsArchive() const { return loadsArchive_; }

bool Plugin::DoFormIDsOverlap(const PluginInterface& plugin) const {
  try {
    auto otherPlugin = dynamic_cast<const Plugin&>(plugin);

    bool doPluginsOverlap;
    auto ret = esp_plugin_do_records_overlap(
        esPlugin.get(), otherPlugin.esPlugin.get(), &doPluginsOverlap);
    if (ret != ESP_OK) {
      throw FileAccessError(name_ +
                            " : esplugin error code: " + std::to_string(ret));
    }

    return doPluginsOverlap;
  } catch (std::bad_cast&) {
    auto logger = getLogger();
    if (logger) {
      logger->error(
          "Tried to check if FormIDs overlapped with a non-Plugin "
          "implementation of PluginInterface.");
    }
  }

  return false;
}

size_t Plugin::NumOverrideFormIDs() const { return numOverrideRecords_; }

bool Plugin::IsValid(const GameType gameType,
                     const std::filesystem::path& pluginPath) {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Checking to see if \"{}\" is a valid plugin.", pluginPath.filename().u8string());
  }

  // If the filename passed ends in '.ghost', that should be trimmed.
  std::string trimmedFilename = pluginPath.filename().u8string();
  if (boost::iends_with(trimmedFilename, ".ghost")) {
    trimmedFilename = trimmedFilename.substr(0, trimmedFilename.length() - 6);
  }

  // Check that the file has a valid extension.
  if (!hasPluginFileExtension(trimmedFilename, gameType))
    return false;

  bool isValid;
  int ret = esp_plugin_is_valid(
      GetEspluginGameId(gameType), pluginPath.u8string().c_str(), true, &isValid);

  if (ret != ESP_OK || !isValid) {
    // Try adding .ghost extension.
    auto ghostedPath = pluginPath;
    ghostedPath += ".ghost";

    ret = esp_plugin_is_valid(
      GetEspluginGameId(gameType), ghostedPath.u8string().c_str(), true, &isValid);
  }

  if (ret != ESP_OK || !isValid) {
    if (logger) {
      logger->warn("The file \"{}\" is not a valid plugin.", pluginPath.filename().u8string());
    }
  }

  return ret == ESP_OK && isValid;
}

uintmax_t Plugin::GetFileSize(std::filesystem::path pluginPath) {
  if (!std::filesystem::exists(pluginPath))
    pluginPath += ".ghost";

  return std::filesystem::file_size(pluginPath);
}

bool Plugin::operator<(const Plugin& rhs) const {
  return boost::locale::to_lower(name_) < boost::locale::to_lower(rhs.name_);
}

void Plugin::Load(const std::filesystem::path& path,
                  GameType gameType,
                  bool headerOnly) {
  ::Plugin* plugin;
  int ret = esp_plugin_new(
      &plugin, GetEspluginGameId(gameType), path.u8string().c_str());
  if (ret != ESP_OK) {
    throw FileAccessError(path.u8string() +
                          " : esplugin error code: " + std::to_string(ret));
  }

  esPlugin = std::shared_ptr<std::remove_pointer<::Plugin>::type>(
      plugin, esp_plugin_free);

  ret = esp_plugin_parse(esPlugin.get(), headerOnly);
  if (ret != ESP_OK) {
    throw FileAccessError(path.u8string() +
                          " : esplugin error code: " + std::to_string(ret));
  }
}

std::string Plugin::GetDescription() const {
  char* description;
  auto ret = esp_plugin_description(esPlugin.get(), &description);
  if (ret != ESP_OK) {
    throw FileAccessError(name_ +
                          " : esplugin error code: " + std::to_string(ret));
  }
  if (description == nullptr) {
    return "";
  }

  string descriptionStr = description;
  esp_string_free(description);

  return descriptionStr;
}

std::string GetArchiveFileExtension(const GameType gameType) {
  if (gameType == GameType::fo4 || gameType == GameType::fo4vr)
    return ".ba2";
  else
    return ".bsa";
}

std::filesystem::path replaceExtension(std::filesystem::path path, const std::string& newExtension) {
  return path.replace_extension(std::filesystem::u8path(newExtension));
}

bool equivalent(const std::filesystem::path& path1, const std::filesystem::path& path2) {
  try {
    return std::filesystem::equivalent(path1, path2);
  } catch (std::filesystem::filesystem_error) {
    // One of the paths checked for equivalence doesn't exist,
    // so they can't be equivalent.
    return false;
  }
}

// Get whether the plugin loads an archive (BSA/BA2) or not.
bool Plugin::LoadsArchive(const GameType gameType,
                          const std::shared_ptr<GameCache> gameCache,
                          const std::filesystem::path& pluginPath) {
  const string archiveExtension = GetArchiveFileExtension(gameType);

  if (gameType == GameType::tes5) {
    // Skyrim plugins only load BSAs that exactly match their basename.
    return std::filesystem::exists(replaceExtension(pluginPath, archiveExtension));
  } else if (gameType != GameType::tes4 ||
             boost::iends_with(pluginPath.filename().u8string(), ".esp")) {
    // Oblivion .esp files and FO3, FNV, FO4 plugins can load archives which
    // begin with the plugin basename.

    auto basenameLength = pluginPath.stem().native().length();
    auto pluginExtension = pluginPath.extension().native();

    for (const auto& archivePath : gameCache->GetArchivePaths()) {
      // Need to check if it starts with the given plugin's basename,
      // but case insensitively. This is hard to do accurately, so
      // instead check if the plugin with the same length basename and
      // and the given plugin's file extension is equivalent.
      auto bsaPluginFilename =
          archivePath.filename().native().substr(0, basenameLength) +
          pluginExtension;
      auto bsaPluginPath =
          pluginPath.parent_path() / bsaPluginFilename;
      if (loot::equivalent(pluginPath, bsaPluginPath)) {
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
  bool espOrEsm = boost::iends_with(filename, ".esp") ||
                  boost::iends_with(filename, ".esm");
  bool lightMaster =
      (gameType == GameType::fo4 || gameType == GameType::fo4vr ||
       gameType == GameType::tes5se || gameType == GameType::tes5vr) &&
      boost::iends_with(filename, ".esl");

  return espOrEsm || lightMaster;
}
}
