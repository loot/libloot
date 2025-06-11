
#ifndef LOOT_API_PLUGIN
#define LOOT_API_PLUGIN

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "libloot-cpp/src/lib.rs.h"
#include "loot/metadata/tag.h"
#include "loot/plugin_interface.h"

namespace loot {

class Plugin final : public PluginInterface {
public:
  explicit Plugin(::rust::Box<loot::rust::Plugin> plugin);

  std::string GetName() const override;
  std::optional<float> GetHeaderVersion() const override;
  std::optional<std::string> GetVersion() const override;
  std::vector<std::string> GetMasters() const override;
  std::vector<std::string> GetBashTags() const override;
  std::optional<uint32_t> GetCRC() const override;

  bool IsMaster() const override;
  bool IsLightPlugin() const override;
  bool IsMediumPlugin() const override;
  bool IsUpdatePlugin() const override;
  bool IsBlueprintPlugin() const override;

  bool IsValidAsLightPlugin() const override;
  bool IsValidAsMediumPlugin() const override;
  bool IsValidAsUpdatePlugin() const override;
  bool IsEmpty() const override;
  bool LoadsArchive() const override;
  bool DoRecordsOverlap(const PluginInterface& plugin) const override;

private:
  ::rust::Box<loot::rust::Plugin> plugin_;
};
}

#endif
