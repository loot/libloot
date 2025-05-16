

#include "loot/api.h"

#include "api/game.h"
#include "libloot-cpp/src/lib.rs.h"
#include "rust/cxx.h"

extern "C" {
extern const unsigned int LIBLOOT_VERSION_MAJOR;

extern const unsigned int LIBLOOT_VERSION_MINOR;

extern const unsigned int LIBLOOT_VERSION_PATCH;

extern const uint8_t LIBLOOT_LOG_LEVEL_TRACE;

extern const uint8_t LIBLOOT_LOG_LEVEL_DEBUG;

extern const uint8_t LIBLOOT_LOG_LEVEL_INFO;

extern const uint8_t LIBLOOT_LOG_LEVEL_WARNING;

extern const uint8_t LIBLOOT_LOG_LEVEL_ERROR;

void libloot_set_logging_callback(void (*callback)(uint8_t, const char*, void*),
                                  void* context);
}

namespace {
using loot::LogLevel;

typedef std::function<void(LogLevel, std::string_view)> Callback;

static Callback STORED_CALLBACK;

LogLevel convert(uint8_t level) {
  if (level == LIBLOOT_LOG_LEVEL_TRACE) {
    return LogLevel::trace;
  } else if (level == LIBLOOT_LOG_LEVEL_DEBUG) {
    return LogLevel::debug;
  } else if (level == LIBLOOT_LOG_LEVEL_INFO) {
    return LogLevel::info;
  } else if (level == LIBLOOT_LOG_LEVEL_WARNING) {
    return LogLevel::warning;
  } else if (level == LIBLOOT_LOG_LEVEL_ERROR) {
    return LogLevel::error;
  } else {
    return LogLevel::error;
  }
}

loot::rust::LogLevel convert(LogLevel level) {
  switch (level) {
    case LogLevel::trace:
      return loot::rust::LogLevel::Trace;
    case LogLevel::debug:
      return loot::rust::LogLevel::Debug;
    case LogLevel::info:
      return loot::rust::LogLevel::Info;
    case LogLevel::warning:
      return loot::rust::LogLevel::Warning;
    case LogLevel::error:
      return loot::rust::LogLevel::Error;
    default:
      return loot::rust::LogLevel::Trace;
  }
}

void logging_callback(uint8_t level, const char* message, void* context) {
  auto& callback = *static_cast<Callback*>(context);

  callback(convert(level), message);
}
}

namespace loot {
LOOT_API void SetLoggingCallback(Callback callback) {
  STORED_CALLBACK = callback;
  libloot_set_logging_callback(logging_callback, &STORED_CALLBACK);
}

LOOT_API void SetLogLevel(LogLevel level) {
  loot::rust::set_log_level(convert(level));
}

LOOT_API bool IsCompatible(const unsigned int versionMajor,
                           const unsigned int versionMinor,
                           const unsigned int versionPatch) {
  return loot::rust::is_compatible(versionMajor, versionMinor, versionPatch);
}

LOOT_API std::unique_ptr<GameInterface> CreateGameHandle(
    const GameType game,
    const std::filesystem::path& gamePath,
    const std::filesystem::path& gameLocalPath) {
  return std::make_unique<Game>(game, gamePath, gameLocalPath);
}

LOOT_API std::string GetLiblootVersion() {
  return std::string(loot::rust::libloot_version());
}

LOOT_API std::string GetLiblootRevision() {
  return std::string(loot::rust::libloot_revision());
}
}
