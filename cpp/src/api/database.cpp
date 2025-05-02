
#include "api/database.h"

#include "api/convert.h"
#include "api/exception/exception.h"

namespace loot {
Database::Database(::rust::Box<loot::rust::Database>&& database) :
    database_(std::move(database)) {}

void Database::LoadMasterlist(const std::filesystem::path& masterlistPath) {
  try {
    database_->load_masterlist(masterlistPath.u8string());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::LoadMasterlistWithPrelude(
    const std::filesystem::path& masterlistPath,
    const std::filesystem::path& masterlistPreludePath) {
  try {
    database_->load_masterlist_with_prelude(masterlistPath.u8string(),
                                            masterlistPreludePath.u8string());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::LoadUserlist(const std::filesystem::path& userlistPath) {
  try {
    database_->load_userlist(userlistPath.u8string());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::WriteUserMetadata(const std::filesystem::path& outputFile,
                                 const bool overwrite) const {
  try {
    database_->write_user_metadata(outputFile.u8string(), overwrite);
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

bool Database::Evaluate(const std::string& condition) const {
  try {
    return database_->evaluate(condition);
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::vector<std::string> Database::GetKnownBashTags() const {
  try {
    return convert<std::string>(database_->known_bash_tags());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::vector<Message> Database::GetGeneralMessages(
    bool evaluateConditions) const {
  try {
    return convert<Message>(database_->general_messages(evaluateConditions));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::vector<Group> Database::GetGroups(bool includeUserMetadata) const {
  try {
    return convert<Group>(database_->groups(includeUserMetadata));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::vector<Group> Database::GetUserGroups() const {
  try {
    return convert<Group>(database_->user_groups());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::SetUserGroups(const std::vector<Group>& groups) {
  try {
    database_->set_user_groups(
        ::rust::Slice(convert<loot::rust::Group>(groups)));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::vector<Vertex> Database::GetGroupsPath(
    std::string_view fromGroupName,
    std::string_view toGroupName) const {
  try {
    return convert<Vertex>(
        database_->groups_path(convert(fromGroupName), convert(toGroupName)));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::optional<PluginMetadata> Database::GetPluginMetadata(
    std::string_view plugin,
    bool includeUserMetadata,
    bool evaluateConditions) const {
  try {
    const auto metadata = database_->plugin_metadata(
        convert(plugin), includeUserMetadata, evaluateConditions);
    if (metadata->is_some()) {
      return convert(metadata->as_ref());
    } else {
      return std::nullopt;
    }
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

std::optional<PluginMetadata> Database::GetPluginUserMetadata(
    std::string_view plugin,
    bool evaluateConditions) const {
  try {
    const auto metadata =
        database_->plugin_user_metadata(convert(plugin), evaluateConditions);
    if (metadata->is_some()) {
      return convert(metadata->as_ref());
    } else {
      return std::nullopt;
    }
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::SetPluginUserMetadata(const PluginMetadata& pluginMetadata) {
  try {
    database_->set_plugin_user_metadata(convert(pluginMetadata));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::DiscardPluginUserMetadata(std::string_view plugin) {
  try {
    database_->discard_plugin_user_metadata(convert(plugin));
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::DiscardAllUserMetadata() {
  try {
    database_->discard_all_user_metadata();
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

void Database::WriteMinimalList(const std::filesystem::path& outputFile,
                                const bool overwrite) const {
  try {
    database_->write_minimal_list(outputFile.u8string(), overwrite);
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}
}
