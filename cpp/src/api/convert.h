#ifndef LOOT_API_CONVERT
#define LOOT_API_CONVERT

#include "libloot-cpp/src/lib.rs.h"
#include "loot/metadata/group.h"
#include "loot/metadata/plugin_metadata.h"
#include "loot/vertex.h"

namespace loot {
// To public types
/////////////////////

std::string convert(const ::rust::String& string);

std::string_view convert(::rust::Str string);

loot::Group convert(const loot::rust::Group& group);

loot::File convert(const loot::rust::File& file);

loot::MessageType convert(loot::rust::MessageType messageType);

loot::MessageContent convert(const loot::rust::MessageContent& content);

loot::Message convert(const loot::rust::Message& message);

loot::Tag convert(const loot::rust::Tag& tag);

loot::PluginCleaningData convert(const loot::rust::PluginCleaningData& data);

loot::Location convert(const loot::rust::Location& location);

loot::PluginMetadata convert(const loot::rust::PluginMetadata& metadata);

std::optional<loot::EdgeType> convert(uint8_t edgeType);

loot::Vertex convert(const loot::rust::Vertex& vertex);

// From public types
///////////////////////

::rust::Str convert(std::string_view view);

::rust::Box<loot::rust::Group> convert(const loot::Group& group);

::rust::Box<loot::rust::File> convert(const loot::File& file);

loot::rust::MessageType convert(loot::MessageType messageType);

::rust::Box<loot::rust::MessageContent> convert(
    const loot::MessageContent& content);

::rust::Box<loot::rust::Message> convert(const loot::Message& message);

::rust::Box<loot::rust::Tag> convert(const loot::Tag& tag);

::rust::Box<loot::rust::PluginCleaningData> convert(
    const loot::PluginCleaningData& data);

::rust::Box<loot::rust::Location> convert(const loot::Location& location);

::rust::Box<loot::rust::PluginMetadata> convert(
    const loot::PluginMetadata& metadata);

// Between containers
////////////////////////

::rust::Vec<::rust::String> convert(const std::vector<std::string>& vector);

template<typename T, typename U>
std::vector<T> convert(const ::rust::Slice<const U>& slice) {
  std::vector<T> output;
  for (const auto& element : slice) {
    output.push_back(convert(element));
  }

  return output;
}

template<typename T, typename U>
std::vector<T> convert(const ::rust::Vec<U>& vec) {
  return convert<T, U>(::rust::Slice(vec));
}

template<typename T, typename U>
const std::vector<::rust::Box<T>> convert(const std::vector<U>& vec) {
  return convert<::rust::Box<T>, U>(::rust::Slice(vec));
}

}

#endif
