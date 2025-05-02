#include "api/plugin.h"

#include <cmath>
#include <typeinfo>

#include "api/convert.h"
#include "api/exception/exception.h"

namespace loot {
Plugin::Plugin(::rust::Box<loot::rust::Plugin> plugin) :
    plugin_(std::move(plugin)) {}

std::string Plugin::GetName() const { return std::string(plugin_->name()); }

std::optional<float> Plugin::GetHeaderVersion() const {
  const auto value = plugin_->header_version();
  if (std::isnan(value)) {
    return std::nullopt;
  } else {
    return value;
  }
}

std::optional<std::string> Plugin::GetVersion() const {
  const auto value = plugin_->version();
  if (value.empty()) {
    return std::nullopt;
  } else {
    return std::string(value);
  }
}

std::vector<std::string> Plugin::GetMasters() const {
  try {
    return convert<std::string>(plugin_->masters());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::vector<std::string> Plugin::GetBashTags() const {
  return convert<std::string>(plugin_->bash_tags());
}

std::optional<uint32_t> Plugin::GetCRC() const {
  try {
    auto optional = plugin_->crc();
    if (optional->is_some()) {
      return optional->as_ref();
    }

    return std::nullopt;
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Plugin::IsMaster() const { return plugin_->is_master(); }

bool Plugin::IsLightPlugin() const { return plugin_->is_light_plugin(); }

bool Plugin::IsMediumPlugin() const { return plugin_->is_medium_plugin(); }

bool Plugin::IsUpdatePlugin() const { return plugin_->is_update_plugin(); }

bool Plugin::IsBlueprintPlugin() const {
  return plugin_->is_blueprint_plugin();
}

bool Plugin::IsValidAsLightPlugin() const {
  try {
    return plugin_->is_valid_as_light_plugin();
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Plugin::IsValidAsMediumPlugin() const {
  try {
    return plugin_->is_valid_as_medium_plugin();
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Plugin::IsValidAsUpdatePlugin() const {
  try {
    return plugin_->is_valid_as_update_plugin();
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Plugin::IsEmpty() const { return plugin_->is_empty(); }

bool Plugin::LoadsArchive() const { return plugin_->loads_archive(); }

bool Plugin::DoRecordsOverlap(const PluginInterface& plugin) const {
  try {
    auto& otherPlugin = dynamic_cast<const Plugin&>(plugin);

    return plugin_->do_records_overlap(*otherPlugin.plugin_);
  } catch (std::bad_cast&) {
    return false;
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}
}
