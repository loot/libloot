#include "api/convert.h"

#include "api/exception/exception.h"

namespace loot {
// To public types
/////////////////////

std::string convert(const ::rust::String& string) {
  return std::string(string);
}

// Although there's an explicit conversion operator declared, it seems that
// building the CXX wrapper with MSVC doesn't set __cplusplus correctly as using
// the operator causes a linker error, so this just reimpls it as a function.
std::string_view convert(::rust::Str str) {
  return std::string_view(str.data(), str.length());
}

loot::Group convert(const loot::rust::Group& group) {
  return loot::Group(convert(group.name()),
                     convert<std::string>(group.after_groups()),
                     convert(group.description()));
}

loot::File convert(const loot::rust::File& file) {
  return loot::File(convert(file.filename().as_str()),
                    convert(file.display_name()),
                    convert(file.condition()),
                    convert<loot::MessageContent>(file.detail()),
                    convert(file.constraint()));
}

loot::MessageType convert(loot::rust::MessageType messageType) {
  switch (messageType) {
    case loot::rust::MessageType::Say:
      return loot::MessageType::say;
    case loot::rust::MessageType::Warn:
      return loot::MessageType::warn;
    case loot::rust::MessageType::Error:
      return loot::MessageType::error;
    default:
      throw std::logic_error("Unsupported MessageType value");
  }
}

loot::MessageContent convert(const loot::rust::MessageContent& content) {
  return loot::MessageContent(convert(content.text()),
                              convert(content.language()));
}

loot::Message convert(const loot::rust::Message& message) {
  return loot::Message(convert(message.message_type()),
                       convert<loot::MessageContent>(message.content()),
                       convert(message.condition()));
}

loot::Tag convert(const loot::rust::Tag& tag) {
  return loot::Tag(
      convert(tag.name()), tag.is_addition(), convert(tag.condition()));
}

loot::PluginCleaningData convert(const loot::rust::PluginCleaningData& data) {
  return loot::PluginCleaningData(data.crc(),
                                  convert(data.cleaning_utility()),
                                  convert<loot::MessageContent>(data.detail()),
                                  data.itm_count(),
                                  data.deleted_reference_count(),
                                  data.deleted_navmesh_count());
}

loot::Location convert(const loot::rust::Location& location) {
  return loot::Location(convert(location.url()), convert(location.name()));
}

loot::PluginMetadata convert(const loot::rust::PluginMetadata& metadata) {
  auto output = loot::PluginMetadata(convert(metadata.name()));

  if (!metadata.group().empty()) {
    output.SetGroup(convert(metadata.group()));
  }

  output.SetLoadAfterFiles(convert<loot::File>(metadata.load_after_files()));
  output.SetRequirements(convert<loot::File>(metadata.requirements()));
  output.SetIncompatibilities(
      convert<loot::File>(metadata.incompatibilities()));
  output.SetMessages(convert<loot::Message>(metadata.messages()));
  output.SetTags(convert<loot::Tag>(metadata.tags()));
  output.SetDirtyInfo(convert<loot::PluginCleaningData>(metadata.dirty_info()));
  output.SetCleanInfo(convert<loot::PluginCleaningData>(metadata.clean_info()));
  output.SetLocations(convert<loot::Location>(metadata.locations()));

  return output;
}

std::optional<loot::EdgeType> convert(uint8_t edgeType) {
  switch (edgeType) {
    case static_cast<uint8_t>(loot::rust::EdgeType::Hardcoded):
      return loot::EdgeType::hardcoded;
    case static_cast<uint8_t>(loot::rust::EdgeType::MasterFlag):
      return loot::EdgeType::masterFlag;
    case static_cast<uint8_t>(loot::rust::EdgeType::Master):
      return loot::EdgeType::master;
    case static_cast<uint8_t>(loot::rust::EdgeType::MasterlistRequirement):
      return loot::EdgeType::masterlistRequirement;
    case static_cast<uint8_t>(loot::rust::EdgeType::UserRequirement):
      return loot::EdgeType::userRequirement;
    case static_cast<uint8_t>(loot::rust::EdgeType::MasterlistLoadAfter):
      return loot::EdgeType::masterlistLoadAfter;
    case static_cast<uint8_t>(loot::rust::EdgeType::UserLoadAfter):
      return loot::EdgeType::userLoadAfter;
    case static_cast<uint8_t>(loot::rust::EdgeType::MasterlistGroup):
      return loot::EdgeType::masterlistGroup;
    case static_cast<uint8_t>(loot::rust::EdgeType::UserGroup):
      return loot::EdgeType::userGroup;
    case static_cast<uint8_t>(loot::rust::EdgeType::RecordOverlap):
      return loot::EdgeType::recordOverlap;
    case static_cast<uint8_t>(loot::rust::EdgeType::AssetOverlap):
      return loot::EdgeType::assetOverlap;
    case static_cast<uint8_t>(loot::rust::EdgeType::TieBreak):
      return loot::EdgeType::tieBreak;
    case static_cast<uint8_t>(loot::rust::EdgeType::BlueprintMaster):
      return loot::EdgeType::blueprintMaster;
    default:
      return std::nullopt;
  }
}

loot::Vertex convert(const loot::rust::Vertex& vertex) {
  try {
    const auto outEdgeType = convert(vertex.out_edge_type());
    if (outEdgeType.has_value()) {
      return loot::Vertex(convert(vertex.name()), outEdgeType.value());
    } else {
      return loot::Vertex(convert(vertex.name()));
    }
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

// From public types
///////////////////////

::rust::Str convert(std::string_view view) {
  return ::rust::Str(view.data(), view.length());
}

::rust::Box<loot::rust::Group> convert(const loot::Group& group) {
  return loot::rust::new_group(
      group.GetName(), group.GetDescription(), convert(group.GetAfterGroups()));
}

::rust::Box<loot::rust::File> convert(const loot::File& file) {
  try {
    return loot::rust::new_file(
        std::string(file.GetName()),
        file.GetDisplayName(),
        file.GetCondition(),
        ::rust::Slice(convert<loot::rust::MessageContent>(file.GetDetail())),
        file.GetConstraint());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

loot::rust::MessageType convert(loot::MessageType messageType) {
  switch (messageType) {
    case loot::MessageType::say:
      return loot::rust::MessageType::Say;
    case loot::MessageType::warn:
      return loot::rust::MessageType::Warn;
    case loot::MessageType::error:
      return loot::rust::MessageType::Error;
    default:
      throw std::logic_error("Unsupported MessageType value");
  }
}

::rust::Box<loot::rust::MessageContent> convert(
    const loot::MessageContent& content) {
  return loot::rust::new_message_content(content.GetText(),
                                         content.GetLanguage());
}

::rust::Box<loot::rust::Message> convert(const loot::Message& message) {
  try {
    return loot::rust::multilingual_message(
        convert(message.GetType()),
        ::rust::Slice(
            convert<loot::rust::MessageContent>(message.GetContent())),
        message.GetCondition());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

::rust::Box<loot::rust::Tag> convert(const loot::Tag& tag) {
  try {
    const auto suggestion = tag.IsAddition()
                                ? loot::rust::TagSuggestion::Addition
                                : loot::rust::TagSuggestion::Removal;
    return loot::rust::new_tag(tag.GetName(), suggestion, tag.GetCondition());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

::rust::Box<loot::rust::PluginCleaningData> convert(
    const loot::PluginCleaningData& data) {
  try {
    return loot::rust::new_plugin_cleaning_data(
        data.GetCRC(),
        data.GetCleaningUtility(),
        ::rust::Slice(convert<loot::rust::MessageContent>(data.GetDetail())),
        data.GetITMCount(),
        data.GetDeletedReferenceCount(),
        data.GetDeletedNavmeshCount());
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

::rust::Box<loot::rust::Location> convert(const loot::Location& location) {
  return loot::rust::new_location(location.GetURL(), location.GetName());
}

::rust::Box<loot::rust::PluginMetadata> convert(
    const loot::PluginMetadata& metadata) {
  try {
    auto output = loot::rust::new_plugin_metadata(metadata.GetName());

    if (metadata.GetGroup().has_value()) {
      output->set_group(metadata.GetGroup().value());
    }

    output->set_load_after_files(
        ::rust::Slice(convert<loot::rust::File>(metadata.GetLoadAfterFiles())));
    output->set_requirements(
        ::rust::Slice(convert<loot::rust::File>(metadata.GetRequirements())));
    output->set_incompatibilities(::rust::Slice(
        convert<loot::rust::File>(metadata.GetIncompatibilities())));
    output->set_messages(
        ::rust::Slice(convert<loot::rust::Message>(metadata.GetMessages())));
    output->set_tags(
        ::rust::Slice(convert<loot::rust::Tag>(metadata.GetTags())));
    output->set_dirty_info(::rust::Slice(
        convert<loot::rust::PluginCleaningData>(metadata.GetDirtyInfo())));
    output->set_clean_info(::rust::Slice(
        convert<loot::rust::PluginCleaningData>(metadata.GetCleanInfo())));
    output->set_locations(
        ::rust::Slice(convert<loot::rust::Location>(metadata.GetLocations())));

    return output;
  } catch (const ::rust::Error& e) {
    std::rethrow_exception(mapError(e));
  }
}

// Between containers
////////////////////////

::rust::Vec<::rust::String> convert(const std::vector<std::string>& vector) {
  ::rust::Vec<::rust::String> strings;
  for (const auto& str : vector) {
    strings.push_back(str);
  }

  return strings;
}
}
