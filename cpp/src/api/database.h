#ifndef LOOT_API_DATABASE
#define LOOT_API_DATABASE

#include "libloot-cpp/src/lib.rs.h"
#include "loot/database_interface.h"
#include "rust/cxx.h"

namespace loot {
class Database final : public DatabaseInterface {
public:
  explicit Database(::rust::Box<loot::rust::Database>&& database);

  void LoadMasterlist(const std::filesystem::path& masterlist_path) override;

  void LoadMasterlistWithPrelude(
      const std::filesystem::path& masterlist_path,
      const std::filesystem::path& masterlist_prelude_path) override;

  void LoadUserlist(const std::filesystem::path& userlist_path) override;

  void WriteUserMetadata(const std::filesystem::path& outputFile,
                         const bool overwrite) const override;

  void WriteMinimalList(const std::filesystem::path& outputFile,
                        const bool overwrite) const override;

  bool Evaluate(const std::string& condition) const override;

  std::vector<std::string> GetKnownBashTags() const override;

  std::vector<Message> GetGeneralMessages(
      bool evaluateConditions = false) const override;

  std::vector<Group> GetGroups(bool includeUserMetadata = true) const override;
  std::vector<Group> GetUserGroups() const override;
  void SetUserGroups(const std::vector<Group>& groups) override;
  std::vector<Vertex> GetGroupsPath(
      std::string_view fromGroupName,
      std::string_view toGroupName) const override;

  std::optional<PluginMetadata> GetPluginMetadata(
      std::string_view plugin,
      bool includeUserMetadata = true,
      bool evaluateConditions = false) const override;

  std::optional<PluginMetadata> GetPluginUserMetadata(
      std::string_view plugin,
      bool evaluateConditions = false) const override;

  void SetPluginUserMetadata(const PluginMetadata& pluginMetadata) override;

  void DiscardPluginUserMetadata(std::string_view plugin) override;

  void DiscardAllUserMetadata() override;

private:
  ::rust::Box<loot::rust::Database> database_;
};
}

#endif
