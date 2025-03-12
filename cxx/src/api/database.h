#ifndef LOOT_API_DATABASE
#define LOOT_API_DATABASE

#include "libloot-cxx/src/lib.rs.h"
#include "loot/database_interface.h"
#include "rust/cxx.h"

namespace loot {
class Database final : public DatabaseInterface {
public:
  explicit Database(::rust::Box<loot::rust::Database>&& database);

  void LoadLists(
      const std::filesystem::path& masterlist_path,
      const std::filesystem::path& userlist_path = "",
      const std::filesystem::path& masterlist_prelude_path = "") override;

  void WriteUserMetadata(const std::filesystem::path& outputFile,
                         const bool overwrite) const override;

  void WriteMinimalList(const std::filesystem::path& outputFile,
                        const bool overwrite) const override;

  std::vector<std::string> GetKnownBashTags() const override;

  std::vector<Message> GetGeneralMessages(
      bool evaluateConditions = false) const override;

  std::vector<Group> GetGroups(bool includeUserMetadata = true) const override;
  std::vector<Group> GetUserGroups() const override;
  void SetUserGroups(const std::vector<Group>& groups) override;
  std::vector<Vertex> GetGroupsPath(
      const std::string& fromGroupName,
      const std::string& toGroupName) const override;

  std::optional<PluginMetadata> GetPluginMetadata(
      const std::string& plugin,
      bool includeUserMetadata = true,
      bool evaluateConditions = false) const override;

  std::optional<PluginMetadata> GetPluginUserMetadata(
      const std::string& plugin,
      bool evaluateConditions = false) const override;

  void SetPluginUserMetadata(const PluginMetadata& pluginMetadata) override;

  void DiscardPluginUserMetadata(const std::string& plugin) override;

  void DiscardAllUserMetadata() override;

private:
  ::rust::Box<loot::rust::Database> database_;
};
}

#endif
