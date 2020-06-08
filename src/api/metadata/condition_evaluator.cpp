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

#include "api/metadata/condition_evaluator.h"

#include <sstream>

#include "api/helpers/crc.h"
#include "api/helpers/logging.h"
#include "loot/exception/condition_syntax_error.h"

using std::filesystem::u8path;

namespace loot {
void HandleError(const std::string operation, int returnCode) {
  if (returnCode == LCI_OK) {
    return;
  }

  const char* message = nullptr;
  std::string err = "Failed to " + operation + ". ";
  lci_get_error_message(&message);
  if (message == nullptr) {
    err += "Error code: " + std::to_string(returnCode);
  }
  else {
    err += "Details: " + std::string(message);
  }

  auto logger = getLogger();
  if (logger) {
    logger->error(err);
  }

  throw ConditionSyntaxError(err);
}

int mapGameType(GameType gameType) {
  switch (gameType) {
  case GameType::tes3:
    return LCI_GAME_MORROWIND;
  case GameType::tes4:
    return LCI_GAME_OBLIVION;
  case GameType::tes5:
    return LCI_GAME_SKYRIM;
  case GameType::tes5se:
    return LCI_GAME_SKYRIM_SE;
  case GameType::tes5vr:
    return LCI_GAME_SKYRIM_VR;
  case GameType::fo3:
    return LCI_GAME_FALLOUT_3;
  case GameType::fonv:
    return LCI_GAME_FALLOUT_NV;
  case GameType::fo4:
    return LCI_GAME_FALLOUT_4;
  case GameType::fo4vr:
    return LCI_GAME_FALLOUT_4_VR;
  default:
    throw std::runtime_error("Unrecognised game type encountered while mapping for condition evaluation.");
  }
}

std::string IntToHexString(const uint32_t value) {
  std::stringstream stream;
  stream << std::hex << value;
  return stream.str();
}

ConditionEvaluator::ConditionEvaluator(
    const GameType gameType,
    const std::filesystem::path& dataPath) {
    lci_state * state = nullptr;

    // This probably isn't correct for API users other than LOOT.
    // But that probably doesn't matter, as the only things conditional
    // on LOOT's version are LOOT-specific messages.
    auto lootPath = std::filesystem::absolute("LOOT.exe");
    int result = lci_state_create(&state, mapGameType(gameType), dataPath.u8string().c_str(), lootPath.u8string().c_str());
    HandleError("create state object for condition evaluation", result);

    lciState_ = std::shared_ptr<lci_state>(state, lci_state_destroy);
}

bool ConditionEvaluator::Evaluate(const std::string& condition) {
  if (condition.empty())
    return true;

  auto logger = getLogger();
  if (logger) {
    logger->trace("Evaluating condition: {}", condition);
  }

  int result = lci_condition_eval(condition.c_str(), lciState_.get());
  if (result != LCI_RESULT_FALSE && result != LCI_RESULT_TRUE) {
    HandleError("evaluate condition \"" + condition + "\"", result);
  }

  return result == LCI_RESULT_TRUE;
}

PluginMetadata ConditionEvaluator::EvaluateAll(const PluginMetadata& pluginMetadata) {
  PluginMetadata evaluatedMetadata(pluginMetadata.GetName());
  evaluatedMetadata.SetLocations(pluginMetadata.GetLocations());

  if (pluginMetadata.GetGroup()) {
    evaluatedMetadata.SetGroup(pluginMetadata.GetGroup().value());
  }

  std::vector<File> files;
  for (const auto& file : pluginMetadata.GetLoadAfterFiles()) {
    if (Evaluate(file.GetCondition()))
      files.push_back(file);
  }
  evaluatedMetadata.SetLoadAfterFiles(files);

  files.clear();
  for (const auto& file : pluginMetadata.GetRequirements()) {
    if (Evaluate(file.GetCondition()))
      files.push_back(file);
  }
  evaluatedMetadata.SetRequirements(files);

  files.clear();
  for (const auto& file : pluginMetadata.GetIncompatibilities()) {
    if (Evaluate(file.GetCondition()))
      files.push_back(file);
  }
  evaluatedMetadata.SetIncompatibilities(files);

  std::vector<Message> messages;
  for (const auto& message : pluginMetadata.GetMessages()) {
    if (Evaluate(message.GetCondition()))
      messages.push_back(message);
  }
  evaluatedMetadata.SetMessages(messages);

  std::vector<Tag> tags;
  for (const auto& tag : pluginMetadata.GetTags()) {
    if (Evaluate(tag.GetCondition()))
      tags.push_back(tag);
  }
  evaluatedMetadata.SetTags(tags);

  if (!evaluatedMetadata.IsRegexPlugin()) {
    std::vector<PluginCleaningData> infoVector;
    for (const auto& info : pluginMetadata.GetDirtyInfo()) {
      if (Evaluate(info, pluginMetadata.GetName()))
        infoVector.push_back(info);
    }
    evaluatedMetadata.SetDirtyInfo(infoVector);

    infoVector.clear();
    for (const auto& info : pluginMetadata.GetCleanInfo()) {
      if (Evaluate(info, pluginMetadata.GetName()))
        infoVector.push_back(info);
    }
    evaluatedMetadata.SetCleanInfo(infoVector);
  }

  return evaluatedMetadata;
}

void ConditionEvaluator::ClearConditionCache() {
  int result = lci_state_clear_condition_cache(lciState_.get());
  HandleError("clear the condition cache", result);
}

void ConditionEvaluator::RefreshState(std::shared_ptr<LoadOrderHandler> loadOrderHandler) {
  ClearConditionCache();

  std::vector<std::string> activePluginNameStrings = loadOrderHandler->GetActivePlugins();
  std::vector<const char *> activePluginNames;
  for (auto& pluginName : activePluginNameStrings) {
    activePluginNames.push_back(pluginName.c_str());
  }

  int result = lci_state_set_active_plugins(lciState_.get(),
    &activePluginNames[0],
    activePluginNames.size());
  HandleError("cache active plugins for condition evaluation", result);
}

void ConditionEvaluator::RefreshState(std::shared_ptr<GameCache> gameCache) {
  ClearConditionCache();

  std::vector<std::string> pluginNames;
  std::vector<std::string> pluginVersionStrings;
  std::vector<uint32_t> crcs;
  for (auto plugin : gameCache->GetPlugins()) {
    pluginNames.push_back(plugin->GetName());
    pluginVersionStrings.push_back(plugin->GetVersion().value_or(""));
    crcs.push_back(plugin->GetCRC().value_or(0));
  }

  std::vector<plugin_version> pluginVersions;
  std::vector<plugin_crc> pluginCrcs;
  for (size_t i = 0; i < pluginNames.size(); ++i) {
    if (!pluginVersionStrings[i].empty()) {
      plugin_version pluginVersion;
      pluginVersion.plugin_name = pluginNames[i].c_str();
      pluginVersion.version = pluginVersionStrings[i].c_str();
      pluginVersions.push_back(pluginVersion);
    }

    if (crcs[i] != 0) {
      plugin_crc pluginCrc;
      pluginCrc.plugin_name = pluginNames[i].c_str();
      pluginCrc.crc = crcs[i];
      pluginCrcs.push_back(pluginCrc);
    }
  }

  int result = lci_state_set_plugin_versions(lciState_.get(),
    &pluginVersions[0],
    pluginVersions.size());
  HandleError("cache plugin versions for condition evaluation", result);

  result = lci_state_set_crc_cache(lciState_.get(),
    &pluginCrcs[0],
    pluginCrcs.size());
  HandleError("fill CRC cache for condition evaluation", result);
}

bool ConditionEvaluator::Evaluate(const PluginCleaningData& cleaningData,
  const std::string& pluginName) {
  if (pluginName.empty())
    return false;

  return Evaluate("checksum(\"" + pluginName + "\", " + IntToHexString(cleaningData.GetCRC()) + ")");
}

void ParseCondition(const std::string& condition) {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Testing condition syntax: {}", condition);
  }

  int result = lci_condition_parse(condition.c_str());
  HandleError("parse condition \"" + condition + "\"", result);
}
}
