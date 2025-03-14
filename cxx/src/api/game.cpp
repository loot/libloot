
#include "api/game.h"

#include "api/convert.h"
#include "api/exception.h"

namespace {
loot::GameType convert(loot::rust::GameType gameType) {
  switch (gameType) {
    case loot::rust::GameType::tes3:
      return loot::GameType::tes3;
    case loot::rust::GameType::tes4:
      return loot::GameType::tes4;
    case loot::rust::GameType::tes5:
      return loot::GameType::tes5;
    case loot::rust::GameType::tes5se:
      return loot::GameType::tes5se;
    case loot::rust::GameType::tes5vr:
      return loot::GameType::tes5vr;
    case loot::rust::GameType::fo3:
      return loot::GameType::fo3;
    case loot::rust::GameType::fonv:
      return loot::GameType::fonv;
    case loot::rust::GameType::fo4:
      return loot::GameType::fo4;
    case loot::rust::GameType::fo4vr:
      return loot::GameType::fo4vr;
    case loot::rust::GameType::starfield:
      return loot::GameType::starfield;
    case loot::rust::GameType::openmw:
      return loot::GameType::openmw;
    default:
      throw std::logic_error("Unsupported GameType value");
  }
}

loot::rust::GameType convert(loot::GameType gameType) {
  switch (gameType) {
    case loot::GameType::tes3:
      return loot::rust::GameType::tes3;
    case loot::GameType::tes4:
      return loot::rust::GameType::tes4;
    case loot::GameType::tes5:
      return loot::rust::GameType::tes5;
    case loot::GameType::tes5se:
      return loot::rust::GameType::tes5se;
    case loot::GameType::tes5vr:
      return loot::rust::GameType::tes5vr;
    case loot::GameType::fo3:
      return loot::rust::GameType::fo3;
    case loot::GameType::fonv:
      return loot::rust::GameType::fonv;
    case loot::GameType::fo4:
      return loot::rust::GameType::fo4;
    case loot::GameType::fo4vr:
      return loot::rust::GameType::fo4vr;
    case loot::GameType::starfield:
      return loot::rust::GameType::starfield;
    case loot::GameType::openmw:
      return loot::rust::GameType::openmw;
    default:
      throw std::logic_error("Unsupported GameType value");
  }
}

rust::Box<loot::rust::Game> constructGame(
    const loot::GameType gameType,
    const std::filesystem::path& gamePath,
    const std::filesystem::path& localDataPath) {
  try {
    if (localDataPath.empty()) {
      return std::move(
          loot::rust::new_game(convert(gameType), gamePath.u8string()));
    } else {
      return std::move(loot::rust::new_game_with_local_path(
          convert(gameType), gamePath.u8string(), localDataPath.u8string()));
    }
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(loot::mapError(e));
  }
}

std::vector<::rust::Str> as_str_refs(const std::vector<std::string>& vector) {
  std::vector<::rust::Str> strings;
  for (const auto& str : vector) {
    strings.push_back(str);
  }

  return strings;
}
}

namespace loot {
Game::Game(const GameType gameType,
           const std::filesystem::path& gamePath,
           const std::filesystem::path& localDataPath) :
    game_(constructGame(gameType, gamePath, localDataPath)),
    database_(game_->database()) {}

GameType Game::GetType() const {
  try {
    return ::convert(game_->game_type());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

const DatabaseInterface& Game::GetDatabase() const { return database_; }

DatabaseInterface& Game::GetDatabase() { return database_; }

std::vector<std::filesystem::path> Game::GetAdditionalDataPaths() const {
  try {
    std::vector<std::filesystem::path> paths;
    for (const auto& path_str : game_->additional_data_paths()) {
      paths.push_back(
          std::filesystem::u8path(path_str.begin(), path_str.end()));
    }

    return paths;
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Game::SetAdditionalDataPaths(
    const std::vector<std::filesystem::path>& additionalDataPaths) {
  std::vector<::rust::String> path_strings;
  std::vector<::rust::Str> path_strs;
  for (const auto& path : additionalDataPaths) {
    path_strings.push_back(path.u8string());
    path_strs.push_back(path_strings.back());
  }
  try {
    game_->set_additional_data_paths(
        ::rust::Slice<const ::rust::Str>(path_strs));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Game::IsValidPlugin(const std::filesystem::path& pluginPath) const {
  return game_->is_valid_plugin(pluginPath.u8string());
}

void Game::LoadPlugins(const std::vector<std::filesystem::path>& pluginPaths,
                       bool loadHeadersOnly) {
  std::vector<::rust::String> path_strings;
  std::vector<::rust::Str> path_strs;
  for (const auto& path : pluginPaths) {
    path_strings.push_back(path.u8string());
    path_strs.push_back(path_strings.back());
  }

  try {
    if (loadHeadersOnly) {
      game_->load_plugin_headers(::rust::Slice<const ::rust::Str>(path_strs));
    } else {
      game_->load_plugins(::rust::Slice<const ::rust::Str>(path_strs));
    }
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }

  for (const auto& path : pluginPaths) {
    auto key = Filename(path.filename().u8string());
    auto it = plugins_.find(key);
    if (it != plugins_.end()) {
      plugins_.erase(it);
    }
  }
}

void Game::ClearLoadedPlugins() {
  game_->clear_loaded_plugins();
  plugins_.clear();
}

const PluginInterface* Game::GetPlugin(const std::string& pluginName) const {
  const auto pluginOpt = game_->plugin(pluginName);
  if (!pluginOpt->is_some()) {
    return nullptr;
  }

  auto key = Filename(pluginName);
  const auto it = plugins_.find(key);
  if (it != plugins_.end()) {
    return it->second.get();
  }

  try {
    auto plugin = std::make_shared<Plugin>(std::move(pluginOpt->as_ref()));
    const auto result = plugins_.emplace(key, plugin);

    return result.first->second.get();
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::vector<const PluginInterface*> Game::GetLoadedPlugins() const {
  std::vector<const PluginInterface*> plugins;
  for (const auto& pluginRef : game_->loaded_plugins()) {
    const auto plugin =
        std::make_shared<Plugin>(std::move(pluginRef.boxed_clone()));

    const auto result =
        plugins_.insert_or_assign(Filename(plugin->GetName()), plugin);
    plugins.push_back(result.first->second.get());
  }

  return plugins;
}

std::vector<std::string> Game::SortPlugins(
    const std::vector<std::string>& pluginFilenames) {
  const auto strs = as_str_refs(pluginFilenames);

  try {
    const auto results = game_->sort_plugins(::rust::Slice(strs));

    return convert<std::string>(results);
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Game::LoadCurrentLoadOrderState() {
  try {
    game_->load_current_load_order_state();
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Game::IsLoadOrderAmbiguous() const {
  try {
    return game_->is_load_order_ambiguous();
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::filesystem::path Game::GetActivePluginsFilePath() const {
  try {
    const auto path_string = game_->active_plugins_file_path();
    return std::filesystem::u8path(path_string.begin(), path_string.end());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Game::IsPluginActive(const std::string& pluginName) const {
  return game_->is_plugin_active(pluginName);
}

std::vector<std::string> Game::GetLoadOrder() const {
  return convert<std::string>(game_->load_order());
}

void Game::SetLoadOrder(const std::vector<std::string>& loadOrder) {
  const auto strs = as_str_refs(loadOrder);

  try {
    game_->set_load_order(::rust::Slice(strs));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}
}
