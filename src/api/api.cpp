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

#include "loot/api.h"

#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>

#include "api/game/game.h"

namespace fs = boost::filesystem;

namespace loot {
std::string ResolvePath(const std::string& path) {
  // NTFS junction links show up as symlinks and directories, but resolving
  // them just appends their target path.
  if (path.empty() || !fs::is_symlink(path) || fs::is_directory(path))
    return path;

  return fs::read_symlink(path).string();
}

LogLevel mapFromBoostLog(boost::log::trivial::severity_level severity) {
  using boost::log::trivial::severity_level;
  switch (severity) {
    case severity_level::trace:
      return LogLevel::trace;
    case severity_level::debug:
      return LogLevel::debug;
    case severity_level::info:
      return LogLevel::info;
    case severity_level::warning:
      return LogLevel::warning;
    case severity_level::error:
      return LogLevel::error;
    case severity_level::fatal:
      return LogLevel::fatal;
    default:
      return LogLevel::trace;
  }
}

class LoggingSink : public boost::log::sinks::basic_formatted_sink_backend<char, boost::log::sinks::concurrent_feeding> {
public:
  LoggingSink(std::function<void(LogLevel, const char*)> callback) {
    this->callback = callback;
  }

  void consume(const boost::log::record_view& rec, const std::string& str) {
    using boost::log::trivial::severity_level;
    auto severity = rec.attribute_values()[boost::log::aux::default_attribute_names::severity()].extract<severity_level>();
    if (!severity) {
      return;
    }

    callback(mapFromBoostLog(*severity), str.c_str());
  }

private:
  std::function<void(LogLevel, const char*)> callback;
};

LOOT_API void SetLoggingCallback(std::function<void(LogLevel, const char*)> callback) {
  typedef boost::log::sinks::synchronous_sink<LoggingSink> sink_t;

  auto sink_backend = boost::make_shared<LoggingSink>(callback);
  boost::shared_ptr<sink_t> sink(new sink_t(sink_backend));

  boost::log::core::get()->remove_all_sinks();
  boost::log::core::get()->add_sink(sink);
}

LOOT_API bool IsCompatible(const unsigned int versionMajor, const unsigned int versionMinor, const unsigned int versionPatch) {
  if (versionMajor > 0)
    return versionMajor == loot::LootVersion::major;
  else
    return versionMinor == loot::LootVersion::minor;
}

LOOT_API void InitialiseLocale(const std::string& id) {
  std::locale::global(boost::locale::generator().generate(id));
  boost::filesystem::path::imbue(std::locale());
}

LOOT_API std::shared_ptr<GameInterface> CreateGameHandle(const GameType game,
                                                         const std::string& gamePath,
                                                         const std::string& gameLocalPath) {
  const std::string resolvedGamePath = ResolvePath(gamePath);
  if (!gamePath.empty() && !fs::is_directory(resolvedGamePath))
    throw std::invalid_argument("Given game path \"" + gamePath + "\" does not resolve to a valid directory.");

  const std::string resolvedGameLocalPath = ResolvePath(gameLocalPath);
  if (!gameLocalPath.empty() && !fs::is_directory(resolvedGameLocalPath))
    throw std::invalid_argument("Given game path \"" + gameLocalPath + "\" does not resolve to a valid directory.");

  return std::make_shared<Game>(game, resolvedGamePath, resolvedGameLocalPath);
}
}
