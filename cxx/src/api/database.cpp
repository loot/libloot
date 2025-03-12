
#include "api/database.h"

#include "api/convert.h"

namespace loot {
Database::Database(::rust::Box<loot::rust::Database>&& database) :
    database_(std::move(database)) {}

void Database::LoadLists(const std::filesystem::path& masterlistPath,
                         const std::filesystem::path& userlistPath,
                         const std::filesystem::path& masterlistPreludePath) {
  if (!masterlistPath.empty()) {
    if (!masterlistPreludePath.empty()) {
      database_->load_masterlist_with_prelude(masterlistPath.u8string(),
                                              masterlistPreludePath.u8string());
    } else {
      database_->load_masterlist(masterlistPath.u8string());
    }
  }

  if (!userlistPath.empty()) {
    database_->load_userlist(userlistPath.u8string());
  }
}

void Database::WriteUserMetadata(const std::filesystem::path& outputFile,
                                 const bool overwrite) const {
  database_->write_user_metadata(outputFile.u8string(), overwrite);
}

std::vector<std::string> Database::GetKnownBashTags() const {
  return convert<std::string>(database_->known_bash_tags());
}

std::vector<Message> Database::GetGeneralMessages(
    bool evaluateConditions) const {
  return convert<Message>(database_->general_messages(evaluateConditions));
}

std::vector<Group> Database::GetGroups(bool includeUserMetadata) const {
  return convert<Group>(database_->groups(includeUserMetadata));
}

std::vector<Group> Database::GetUserGroups() const {
  return convert<Group>(database_->user_groups());
}

void Database::SetUserGroups(const std::vector<Group>& groups) {
  database_->set_user_groups(::rust::Slice(convert<loot::rust::Group>(groups)));
}

std::vector<Vertex> Database::GetGroupsPath(
    const std::string& fromGroupName,
    const std::string& toGroupName) const {
  return convert<Vertex>(database_->groups_path(fromGroupName, toGroupName));
}

std::optional<PluginMetadata> Database::GetPluginMetadata(
    const std::string& plugin,
    bool includeUserMetadata,
    bool evaluateConditions) const {
  const auto metadata = database_->plugin_metadata(
      plugin, includeUserMetadata, evaluateConditions);
  if (metadata->is_some()) {
    return convert(metadata->as_ref());
  } else {
    return std::nullopt;
  }
}

std::optional<PluginMetadata> Database::GetPluginUserMetadata(
    const std::string& plugin,
    bool evaluateConditions) const {
  const auto metadata =
      database_->plugin_user_metadata(plugin, evaluateConditions);
  if (metadata->is_some()) {
    return convert(metadata->as_ref());
  } else {
    return std::nullopt;
  }
}

void Database::SetPluginUserMetadata(const PluginMetadata& pluginMetadata) {
  database_->set_plugin_user_metadata(convert(pluginMetadata));
}

void Database::DiscardPluginUserMetadata(const std::string& plugin) {
  database_->discard_plugin_user_metadata(plugin);
}

void Database::DiscardAllUserMetadata() {
  database_->discard_all_user_metadata();
}

void Database::WriteMinimalList(const std::filesystem::path& outputFile,
                                const bool overwrite) const {
  database_->write_minimal_list(outputFile.u8string(), overwrite);
}
}
