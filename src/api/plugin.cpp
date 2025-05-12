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

#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <numeric>

#include "api/bsa.h"
#include "api/game/game.h"
#include "api/helpers/crc.h"
#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "loot/exception/error_categories.h"
#include "loot/exception/file_access_error.h"

namespace {
using loot::BSA_FILE_EXTENSION;
using loot::GameCache;
using loot::GameType;

// Intentionally takes a copy of the first parameter.
std::filesystem::path ReplaceExtension(std::filesystem::path path,
                                       std::string_view newExtension) {
  return path.replace_extension(std::filesystem::u8path(newExtension));
}

// Intentionally takes a copy of the first parameter.
std::filesystem::path GetSuffixedArchivePath(std::filesystem::path pluginPath,
                                             std::string_view suffix,
                                             std::string_view newExtension) {
  // replace_extension() with no argument just removes the existing extension.
  pluginPath.replace_extension();
  pluginPath += suffix;
  pluginPath += newExtension;
  return pluginPath;
}

std::vector<std::filesystem::path> FindAssociatedArchive(
    const std::filesystem::path& pluginPath) {
  const auto archiveFilename = ReplaceExtension(pluginPath, BSA_FILE_EXTENSION);

  if (std::filesystem::exists(archiveFilename)) {
    return {archiveFilename};
  }

  return {};
}

std::vector<std::filesystem::path> FindAssociatedArchivesWithSuffixes(
    const std::filesystem::path& pluginPath,
    std::string_view archiveExtension,
    const std::vector<std::string>& supportedSuffixes) {
  std::vector<std::filesystem::path> paths;

  for (const auto& suffix : supportedSuffixes) {
    const auto archivePath =
        GetSuffixedArchivePath(pluginPath, suffix, archiveExtension);

    if (std::filesystem::exists(archivePath)) {
      paths.push_back(archivePath);
    }
  }

  return paths;
}

std::vector<std::filesystem::path> FindAssociatedArchivesWithArbitrarySuffixes(
    const GameCache& gameCache,
    const std::filesystem::path& pluginPath) {
  const auto basenameLength = pluginPath.stem().native().length();
  const auto pluginExtension = pluginPath.extension().native();

  std::vector<std::filesystem::path> paths;
  for (const auto& archivePath : gameCache.GetArchivePaths()) {
    // Need to check if it starts with the given plugin's basename,
    // but case insensitively. This is hard to do accurately, so
    // instead check if the plugin with the same length basename and
    // and the given plugin's file extension is equivalent.
    const auto bsaPluginFilename =
        archivePath.filename().native().substr(0, basenameLength) +
        pluginExtension;
    const auto bsaPluginPath = pluginPath.parent_path() / bsaPluginFilename;
    if (loot::equivalent(pluginPath, bsaPluginPath)) {
      paths.push_back(archivePath);
    }
  }

  return paths;
}

std::vector<std::filesystem::path> FindAssociatedArchives(
    const GameType gameType,
    const GameCache& gameCache,
    const std::filesystem::path& pluginPath) {
  switch (gameType) {
    case GameType::tes3:
    case GameType::openmw:
      return {};
    case GameType::tes5:
      // Skyrim (non-SE) plugins can only load BSAs that have exactly the same
      // basename, ignoring file extensions.
      return FindAssociatedArchive(pluginPath);
    case GameType::tes5se:
    case GameType::tes5vr:
      // Skyrim SE can load BSAs that have exactly the same
      // basename, ignoring file extensions, and also BSAs with filenames of
      // the form "<basename> - Textures.bsa" (case-insensitively).
      // This assumes that Skyrim VR works the same way as Skyrim SE.
      return FindAssociatedArchivesWithSuffixes(
          pluginPath, BSA_FILE_EXTENSION, {"", " - Textures"});
    case GameType::tes4:
    case GameType::oblivionRemastered: {
      // Oblivion .esp files can load archives which begin with the plugin
      // basename.
      if (!boost::iends_with(pluginPath.filename().u8string(), ".esp")) {
        return {};
      }

      return FindAssociatedArchivesWithArbitrarySuffixes(gameCache, pluginPath);
    }
    case GameType::fo3:
    case GameType::fonv:
    case GameType::fo4:
    case GameType::fo4vr: {
      // FO3, FNV, FO4 plugins can load archives which begin with the plugin
      // basename. This assumes that FO4 VR works the same way as FO4.
      return FindAssociatedArchivesWithArbitrarySuffixes(gameCache, pluginPath);
    }
    case GameType::starfield:
      // The game will load a BA2 that's suffixed with " - Voices_<language>"
      // where <language> is whatever language Starfield is configured to use
      // (sLanguage in the ini), so this isn't exactly correct but will work
      // so long as a plugin with voices has voices for English, which seems
      // likely.
      return FindAssociatedArchivesWithSuffixes(
          pluginPath,
          loot::BA2_FILE_EXTENSION,
          {" - Main", " - Textures", " - Localization", " - Voices_en"});
    default:
      throw std::logic_error("Unrecognised game type");
  }
}

void HandleEspluginError(unsigned int returnCode, std::string_view operation) {
  if (returnCode == ESP_OK) {
    return;
  }

  auto err = fmt::format(
      "esplugin failed to {}. Error code: {}", operation, returnCode);

  const char* e = nullptr;
  esp_get_error_message(&e);
  if (e == nullptr) {
    err += ". Details could not be fetched.";
  } else {
    err += ". Details: " + std::string(e);
  }

  auto logger = loot::getLogger();
  if (logger) {
    logger->error(err);
  }

  throw std::system_error(returnCode, loot::esplugin_category(), err);
}

template<typename... Args>
void HandleEspluginError(unsigned int returnCode,
                         std::string_view message,
                         Args... args) {
  if (returnCode == ESP_OK) {
    return;
  }

  auto operation = fmt::format(message, args...);
  return HandleEspluginError(returnCode, operation);
}

void HandleEspluginError(unsigned int returnCode,
                         const std::function<std::string()>& getMessage) {
  if (returnCode == ESP_OK) {
    return;
  }

  return HandleEspluginError(returnCode, getMessage());
}

std::string GetPluginName(GameType gameType,
                          const std::filesystem::path& pluginPath) {
  return gameType == GameType::openmw
             ? pluginPath.filename().u8string()
             : loot::TrimDotGhostExtension(pluginPath.filename().u8string());
}

std::unique_ptr<::Plugin, decltype(&esp_plugin_free)> MakeEspluginPtr() {
  return std::unique_ptr<::Plugin, decltype(&esp_plugin_free)>(nullptr,
                                                               esp_plugin_free);
}

bool ShouldIgnoreMasterFlag(GameType gameType) {
  return gameType == GameType::openmw;
}
}

namespace loot {
Plugin::Plugin(const GameType gameType,
               const GameCache& gameCache,
               const std::filesystem::path& pluginPath,
               const bool headerOnly) :
    name_(GetPluginName(gameType, pluginPath)),
    esPlugin(MakeEspluginPtr()),
    ignoreMasterFlag_(ShouldIgnoreMasterFlag(gameType)),
    isEmpty_(true) {
  auto logger = getLogger();

  try {
    if (gameType != GameType::openmw ||
        pluginPath.extension() != ".omwscripts") {
      Load(pluginPath, gameType, headerOnly);

      auto ret = esp_plugin_is_empty(esPlugin.get(), &isEmpty_);
      HandleEspluginError(ret, "check if \"{}\" is empty", name_);
    }

    archivePaths_ = FindAssociatedArchives(gameType, gameCache, pluginPath);

    if (!headerOnly) {
      crc_ = GetCrc32(pluginPath);

      // Get the assets in the BSAs that this plugin loads.
      auto assets = GetAssetsInBethesdaArchives(archivePaths_);
      std::swap(archiveAssets_, assets);

      if (logger) {
        logger->debug(
            "Plugin file \"{}\" loads {} assets from Bethesda archives",
            pluginPath.u8string(),
            GetAssetCount());
      }
    }

    const auto description = GetDescription();
    tags_ = ExtractBashTags(description);
    version_ = ExtractVersion(description);
  } catch (const std::system_error& e) {
    if (e.code().category() == esplugin_category()) {
      throw;
    }

    if (logger) {
      logger->error("Cannot read plugin file \"{}\". Details: {}",
                    pluginPath.u8string(),
                    e.what());
    }
    throw FileAccessError("Cannot read \"" + pluginPath.u8string() +
                          "\". Details: " + e.what());
  } catch (const std::exception& e) {
    if (logger) {
      logger->error("Cannot read plugin file \"{}\". Details: {}",
                    pluginPath.u8string(),
                    e.what());
    }
    throw FileAccessError("Cannot read \"" + pluginPath.u8string() +
                          "\". Details: " + e.what());
  }
}

void Plugin::ResolveRecordIds(Vec_PluginMetadata* pluginsMetadata) const {
  if (esPlugin == nullptr) {
    return;
  }

  auto ret = esp_plugin_resolve_record_ids(esPlugin.get(), pluginsMetadata);
  HandleEspluginError(ret, "resolve the record IDs of \"{}\"", name_);
}

std::string Plugin::GetName() const { return name_; }

std::optional<float> Plugin::GetHeaderVersion() const {
  if (esPlugin == nullptr) {
    return std::nullopt;
  }

  float version = 0.0f;

  const auto ret = esp_plugin_header_version(esPlugin.get(), &version);
  HandleEspluginError(ret, "get the header version of \"{}\"", name_);

  if (std::isnan(version)) {
    return std::nullopt;
  }

  return version;
}

std::optional<std::string> Plugin::GetVersion() const { return version_; }

std::vector<std::string> Plugin::GetMasters() const {
  if (esPlugin == nullptr) {
    return {};
  }

  char** masters = nullptr;
  size_t numMasters = 0;
  const auto ret = esp_plugin_masters(esPlugin.get(), &masters, &numMasters);
  HandleEspluginError(ret, "get the masters of \"{}\"", name_);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<std::string> mastersVec(masters, masters + numMasters);
  esp_string_array_free(masters, numMasters);

  return mastersVec;
}

std::vector<std::string> Plugin::GetBashTags() const { return tags_; }

std::optional<uint32_t> Plugin::GetCRC() const { return crc_; }

bool Plugin::IsMaster() const {
  if (ignoreMasterFlag_ || esPlugin == nullptr) {
    return false;
  }

  bool isMaster = false;
  const auto ret = esp_plugin_is_master(esPlugin.get(), &isMaster);
  HandleEspluginError(ret, "check if \"{}\" is a master", name_);

  return isMaster;
}

bool Plugin::IsLightPlugin() const {
  if (esPlugin == nullptr) {
    return false;
  }

  bool isLightPlugin = false;
  const auto ret = esp_plugin_is_light_plugin(esPlugin.get(), &isLightPlugin);
  HandleEspluginError(ret, "check if \"{}\" is a light plugin", name_);

  return isLightPlugin;
}

bool Plugin::IsMediumPlugin() const {
  if (esPlugin == nullptr) {
    return false;
  }

  bool isMediumPlugin = false;
  const auto ret = esp_plugin_is_medium_plugin(esPlugin.get(), &isMediumPlugin);
  HandleEspluginError(ret, "check if \"{}\" is a medium plugin", name_);

  return isMediumPlugin;
}

bool Plugin::IsUpdatePlugin() const {
  if (esPlugin == nullptr) {
    return false;
  }

  bool isUpdatePlugin = false;
  const auto ret = esp_plugin_is_update_plugin(esPlugin.get(), &isUpdatePlugin);
  HandleEspluginError(ret, "check if \"{}\" is an update plugin", name_);

  return isUpdatePlugin;
}

bool Plugin::IsBlueprintPlugin() const {
  if (esPlugin == nullptr) {
    return false;
  }

  bool isBlueprintPlugin = false;
  const auto ret =
      esp_plugin_is_blueprint_plugin(esPlugin.get(), &isBlueprintPlugin);
  HandleEspluginError(ret, "check if \"{}\" is a blueprint plugin", name_);

  return isBlueprintPlugin;
}

bool Plugin::IsValidAsLightPlugin() const {
  if (esPlugin == nullptr) {
    return false;
  }

  bool isValid = false;
  const auto ret =
      esp_plugin_is_valid_as_light_plugin(esPlugin.get(), &isValid);
  HandleEspluginError(ret, "check if \"{}\" is valid as a light plugin", name_);

  return isValid;
}

bool Plugin::IsValidAsMediumPlugin() const {
  if (esPlugin == nullptr) {
    return false;
  }

  bool isValid = false;
  const auto ret =
      esp_plugin_is_valid_as_medium_plugin(esPlugin.get(), &isValid);
  HandleEspluginError(
      ret, "check if \"{}\" is valid as a medium plugin", name_);

  return isValid;
}

bool Plugin::IsValidAsUpdatePlugin() const {
  if (esPlugin == nullptr) {
    return false;
  }

  bool isValid = false;
  const auto ret =
      esp_plugin_is_valid_as_update_plugin(esPlugin.get(), &isValid);
  HandleEspluginError(
      ret, "check if \"{}\" is valid as an update plugin", name_);

  return isValid;
}

bool Plugin::IsEmpty() const { return isEmpty_; }

bool Plugin::LoadsArchive() const { return !archivePaths_.empty(); }

bool Plugin::DoRecordsOverlap(const PluginInterface& plugin) const {
  if (esPlugin == nullptr) {
    return false;
  }

  try {
    auto& otherPlugin = dynamic_cast<const Plugin&>(plugin);

    if (otherPlugin.esPlugin == nullptr) {
      return false;
    }

    bool doPluginsOverlap = false;
    const auto ret = esp_plugin_do_records_overlap(
        esPlugin.get(), otherPlugin.esPlugin.get(), &doPluginsOverlap);
    HandleEspluginError(ret, [&]() {
      return fmt::format(
          "check if \"{}\" and \"{}\" overlap", name_, otherPlugin.GetName());
    });

    return doPluginsOverlap;
  } catch (std::bad_cast&) {
    auto logger = getLogger();
    if (logger) {
      logger->error(
          "Tried to check if records overlapped with a non-Plugin "
          "implementation of PluginInterface.");
    }
  }

  return false;
}

size_t Plugin::GetOverrideRecordCount() const {
  if (esPlugin == nullptr) {
    return 0;
  }

  size_t overrideRecordCount;
  const auto ret =
      esp_plugin_count_override_records(esPlugin.get(), &overrideRecordCount);
  HandleEspluginError(ret, "count override records in \"{}\"", name_);

  return overrideRecordCount;
}

size_t Plugin::GetAssetCount() const {
  return std::accumulate(
      archiveAssets_.begin(),
      archiveAssets_.end(),
      size_t{0},
      [](const size_t& a, const auto& b) { return a + b.second.size(); });
}

bool Plugin::DoAssetsOverlap(const PluginSortingInterface& plugin) const {
  if (archiveAssets_.empty()) {
    return false;
  }

  try {
    const auto& otherPlugin = dynamic_cast<const Plugin&>(plugin);

    return DoAssetsIntersect(archiveAssets_, otherPlugin.archiveAssets_);
  } catch (std::bad_cast&) {
    auto logger = getLogger();
    if (logger) {
      logger->error(
          "Tried to check how many FormIDs overlapped with a non-Plugin "
          "implementation of PluginSortingInterface.");
    }
    throw std::invalid_argument(
        "Tried to check how many FormIDs overlapped with a non-Plugin "
        "implementation of PluginSortingInterface.");
  }
}

bool Plugin::IsValid(const GameType gameType,
                     const std::filesystem::path& pluginPath) {
  // Check that the file has a valid extension.
  if (hasPluginFileExtension(pluginPath.filename().u8string(), gameType)) {
    if (gameType == GameType::openmw &&
        pluginPath.extension() == ".omwscripts") {
      return true;
    }

    bool isValid = false;
    auto returnCode = esp_plugin_is_valid(GetEspluginGameId(gameType),
                                          pluginPath.u8string().c_str(),
                                          true,
                                          &isValid);

    if (returnCode == ESP_OK && isValid) {
      return true;
    }
  }

  auto logger = getLogger();
  if (logger) {
    logger->debug("The file \"{}\" is not a valid plugin.",
                  pluginPath.u8string());
  }

  return false;
}

void Plugin::Load(const std::filesystem::path& path,
                  GameType gameType,
                  bool headerOnly) {
  ::Plugin* plugin = nullptr;
  auto ret = esp_plugin_new(
      &plugin, GetEspluginGameId(gameType), path.u8string().c_str());
  HandleEspluginError(ret, [&]() {
    return fmt::format("load plugin \"{}\"", path.u8string());
  });

  esPlugin = std::unique_ptr<::Plugin, decltype(&esp_plugin_free)>(
      plugin, esp_plugin_free);

  ret = esp_plugin_parse(esPlugin.get(), headerOnly);
  HandleEspluginError(ret, [&]() {
    return fmt::format("parse plugin \"{}\"", path.u8string());
  });
}

std::string Plugin::GetDescription() const {
  if (esPlugin == nullptr) {
    return "";
  }

  char* description = nullptr;
  const auto ret = esp_plugin_description(esPlugin.get(), &description);
  HandleEspluginError(ret, "read the description of \"{}\"", name_);

  if (description == nullptr) {
    return "";
  }

  std::string descriptionStr = description;
  esp_string_free(description);

  return descriptionStr;
}

std::unique_ptr<Vec_PluginMetadata, decltype(&esp_plugins_metadata_free)>
Plugin::GetPluginsMetadata(const std::vector<const Plugin*>& plugins) {
  if (plugins.empty()) {
    return std::unique_ptr<Vec_PluginMetadata,
                           decltype(&esp_plugins_metadata_free)>(
        nullptr, esp_plugins_metadata_free);
  }

  std::vector<const ::Plugin*> esPlugins;
  esPlugins.reserve(plugins.size());
  for (const auto& plugin : plugins) {
    const auto esPlugin = plugin->esPlugin.get();
    if (esPlugin != nullptr) {
      esPlugins.push_back(plugin->esPlugin.get());
    }
  }

  Vec_PluginMetadata* pluginsMetadata = nullptr;
  const auto ret = esp_get_plugins_metadata(
      esPlugins.data(), esPlugins.size(), &pluginsMetadata);
  HandleEspluginError(ret, [&]() {
    return fmt::format("get metadata for {} plugins", plugins.size());
  });

  return std::unique_ptr<Vec_PluginMetadata,
                         decltype(&esp_plugins_metadata_free)>(
      pluginsMetadata, esp_plugins_metadata_free);
}

std::string GetArchiveFileExtension(const GameType gameType) {
  if (gameType == GameType::fo4 || gameType == GameType::fo4vr ||
      gameType == GameType::starfield)
    return std::string(BA2_FILE_EXTENSION);
  else
    return std::string(BSA_FILE_EXTENSION);
}

unsigned int Plugin::GetEspluginGameId(GameType gameType) {
  switch (gameType) {
    case GameType::tes3:
    case GameType::openmw:
      return ESP_GAME_MORROWIND;
    case GameType::tes4:
    case GameType::oblivionRemastered:
      return ESP_GAME_OBLIVION;
    case GameType::tes5:
      return ESP_GAME_SKYRIM;
    case GameType::tes5se:
    case GameType::tes5vr:
      return ESP_GAME_SKYRIMSE;
    case GameType::fo3:
      return ESP_GAME_FALLOUT3;
    case GameType::fonv:
      return ESP_GAME_FALLOUTNV;
    case GameType::fo4:
    case GameType::fo4vr:
      return ESP_GAME_FALLOUT4;
    case GameType::starfield:
      return ESP_GAME_STARFIELD;
    default:
      throw std::logic_error("Unrecognised game type");
  }
}

bool hasPluginFileExtension(std::string_view filename, GameType gameType) {
  if (gameType != GameType::openmw &&
      boost::iends_with(filename, GHOST_FILE_EXTENSION)) {
    filename =
        filename.substr(0, filename.length() - GHOST_FILE_EXTENSION.length());
  }

  if (boost::iends_with(filename, ".esp") ||
      boost::iends_with(filename, ".esm")) {
    return true;
  }

  if (gameType == GameType::openmw &&
      (boost::iends_with(filename, ".omwaddon") ||
       boost::iends_with(filename, ".omwgame") ||
       boost::iends_with(filename, ".omwscripts"))) {
    return true;
  }

  if ((gameType == GameType::fo4 || gameType == GameType::fo4vr ||
       gameType == GameType::tes5se || gameType == GameType::tes5vr ||
       gameType == GameType::starfield) &&
      boost::iends_with(filename, ".esl")) {
    return true;
  }

  return false;
}

bool equivalent(const std::filesystem::path& path1,
                const std::filesystem::path& path2) {
  // If the paths are identical, they've got to be equivalent,
  // it doesn't matter if the paths exist or not.
  if (path1 == path2) {
    return true;
  }
  // If the paths are not identical, the filesystem might be case-insensitive
  // so check with the filesystem.
  try {
    return std::filesystem::equivalent(path1, path2);
  } catch (const std::filesystem::filesystem_error&) {
    // One of the paths checked for equivalence doesn't exist,
    // so they can't be equivalent.
    return false;
  } catch (const std::system_error&) {
    // This can be thrown if one or both of the paths contains a character
    // that can't be represented in Windows' multi-byte code page (e.g.
    // Windows-1252), even though Unicode paths shouldn't be a problem,
    // and throwing system_error is undocumented. Seems like a bug in MSVC's
    // implementation.
    return false;
  }
}
}
