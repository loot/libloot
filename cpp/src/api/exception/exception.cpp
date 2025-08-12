#include "api/exception/exception.h"

#include <charconv>

#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/plugin_not_loaded_error.h"
#include "loot/exception/undefined_group_error.h"
#include "loot/vertex.h"

namespace {
using std::string_view_literals::operator""sv;
using loot::EdgeType;
using loot::Vertex;

constexpr std::string_view CYCLIC_ERROR_PREFIX = "CyclicInteractionError: "sv;
constexpr std::string_view UNDEFINED_GROUP_ERROR_PREFIX =
    "UndefinedGroupError: "sv;
constexpr std::string_view PLUGIN_NOT_LOADED_ERROR_PREFIX =
    "PluginNotLoadedError: "sv;
constexpr std::string_view INVALID_ARGUMENT_PREFIX = "InvalidArgument: "sv;

bool startsWith(std::string_view str, std::string_view prefix) {
  if (str.size() < prefix.size()) {
    return false;
  }

  return str.substr(0, prefix.size()) == prefix;
}

std::string replace(std::string_view str,
                    std::string_view from,
                    std::string_view to) {
  std::string out;
  out.reserve(str.size());

  size_t startPos = 0;
  auto findPos = str.find(from, startPos);
  while (findPos != std::string_view::npos) {
    out.append(str.substr(startPos, findPos - startPos));
    out.append(to);

    startPos = findPos + from.size();
    findPos = str.find(from, startPos);
  }

  out.append(str.substr(startPos));

  return out;
}

EdgeType toEdgeType(std::string_view edgeTypeDisplay) {
  if (edgeTypeDisplay == "Hardcoded") {
    return EdgeType::hardcoded;
  } else if (edgeTypeDisplay == "Master Flag") {
    return EdgeType::masterFlag;
  } else if (edgeTypeDisplay == "Master") {
    return EdgeType::master;
  } else if (edgeTypeDisplay == "Masterlist Requirement") {
    return EdgeType::masterlistRequirement;
  } else if (edgeTypeDisplay == "User Requirement") {
    return EdgeType::userRequirement;
  } else if (edgeTypeDisplay == "Masterlist Load After") {
    return EdgeType::masterlistLoadAfter;
  } else if (edgeTypeDisplay == "User Load After") {
    return EdgeType::userLoadAfter;
  } else if (edgeTypeDisplay == "Masterlist Group") {
    return EdgeType::masterlistGroup;
  } else if (edgeTypeDisplay == "User Group") {
    return EdgeType::userGroup;
  } else if (edgeTypeDisplay == "Record Overlap") {
    return EdgeType::recordOverlap;
  } else if (edgeTypeDisplay == "Asset Overlap") {
    return EdgeType::assetOverlap;
  } else if (edgeTypeDisplay == "Tie Break") {
    return EdgeType::tieBreak;
  } else if (edgeTypeDisplay == "Blueprint Master") {
    return EdgeType::blueprintMaster;
  } else {
    std::string what("Unrecognised edge type: ");
    what += edgeTypeDisplay;
    throw std::logic_error(what);
  }
}

std::vector<Vertex> parseCyclicError(std::string_view what) {
  constexpr std::string_view SEPARATOR = " > ";
  const auto suffix = what.substr(CYCLIC_ERROR_PREFIX.size());

  std::vector<Vertex> vertices;
  size_t pos = 0;
  while (pos < suffix.size()) {
    const auto sepPos = suffix.find(SEPARATOR, pos);
    const auto escapedName = suffix.substr(pos, sepPos - pos);
    const auto name = replace(replace(escapedName, "\\>", ">"), "\\\\", "\\");

    const auto secondSepPos = suffix.find(SEPARATOR, sepPos + SEPARATOR.size());
    if (secondSepPos != std::string::npos) {
      const auto escapedEdgeName =
          suffix.substr(sepPos + SEPARATOR.size(),
                        secondSepPos - (sepPos + SEPARATOR.size()));

      vertices.push_back(Vertex(name, toEdgeType(escapedEdgeName)));

      pos = secondSepPos + SEPARATOR.size();
    } else {
      vertices.push_back(Vertex(name));
      pos = suffix.size();
    }
  }

  return vertices;
}

std::string getErrorSuffix(std::string_view what) {
  const auto sepPos = what.find(": ");

  return std::string(what.substr(sepPos + 2));
}
}

namespace loot {
std::exception_ptr mapError(const ::rust::Error& error) {
  if (startsWith(error.what(), CYCLIC_ERROR_PREFIX)) {
    return std::make_exception_ptr(
        CyclicInteractionError(parseCyclicError(error.what())));
  } else if (startsWith(error.what(), UNDEFINED_GROUP_ERROR_PREFIX)) {
    return std::make_exception_ptr(
        UndefinedGroupError(getErrorSuffix(error.what())));
  } else if (startsWith(error.what(), PLUGIN_NOT_LOADED_ERROR_PREFIX)) {
    return std::make_exception_ptr(
        PluginNotLoadedError("The plugin \"" + getErrorSuffix(error.what()) +
                             "\" has not been loaded"));
  } else if (startsWith(error.what(), INVALID_ARGUMENT_PREFIX)) {
    return std::make_exception_ptr(
        std::invalid_argument(getErrorSuffix(error.what())));
  } else {
    return std::make_exception_ptr(std::runtime_error(error.what()));
  }
}
}
