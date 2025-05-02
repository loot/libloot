#include "api/exception/exception.h"

#include <charconv>

#include "loot/exception/condition_syntax_error.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/error_categories.h"
#include "loot/exception/file_access_error.h"
#include "loot/exception/undefined_group_error.h"
#include "loot/vertex.h"

namespace {
using std::string_view_literals::operator""sv;
using loot::EdgeType;
using loot::Vertex;

constexpr std::string_view CYCLIC_ERROR_PREFIX = "CyclicInteractionError: "sv;
constexpr std::string_view UNDEFINED_GROUP_ERROR_PREFIX =
    "UndefinedGroupError: "sv;
constexpr std::string_view ESPLUGIN_ERROR_PREFIX = "EspluginError: "sv;
constexpr std::string_view LIBLOADORDER_ERROR_PREFIX = "LibloadorderError: "sv;
constexpr std::string_view LCI_ERROR_PREFIX = "LciError: "sv;
constexpr std::string_view FILE_ACCESS_ERROR_PREFIX = "FileAccessError: "sv;
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

  size_t i = 0;
  while (i < str.size()) {
    if (i + from.size() <= str.size() && str.substr(i, from.size()) == from) {
      out.append(to);
      i += from.size();
    } else {
      out.push_back(str[i]);
      i += 1;
    }
  }

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
  const auto suffix = what.substr(0, CYCLIC_ERROR_PREFIX.size());

  std::vector<Vertex> vertices;
  size_t pos = 0;
  while (pos < suffix.size()) {
    const auto sepPos = suffix.find("--", pos);
    const auto escapedName = suffix.substr(pos, sepPos);
    const auto name = replace(replace(escapedName, "\\-", "-"), "\\\\", "\\");

    if (sepPos != std::string::npos) {
      const auto secondSepPos = suffix.find("--", sepPos + 2);
      const auto escapedEdgeName =
          suffix.substr(sepPos + 2, secondSepPos - (sepPos + 2));

      vertices.push_back(Vertex(name, toEdgeType(escapedEdgeName)));

      pos = secondSepPos + 2;
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

std::pair<int, std::string> parseSystemError(std::string_view whatSuffix) {
  const auto sepPos = whatSuffix.find(": ");
  int code;
  const auto result =
      std::from_chars(whatSuffix.data(), whatSuffix.data() + sepPos, code);
  if (result.ec != std::errc{}) {
    std::string err = "Could not parse error code from string: ";
    err += whatSuffix;
    throw std::runtime_error(err);
  }

  return std::make_pair(code, std::string(whatSuffix.substr(sepPos + 2)));
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
  } else if (startsWith(error.what(), ESPLUGIN_ERROR_PREFIX)) {
    const auto [code, details] = parseSystemError(
        std::string_view(error.what()).substr(ESPLUGIN_ERROR_PREFIX.size()));

    return std::make_exception_ptr(
        std::system_error(code, esplugin_category(), details));

  } else if (startsWith(error.what(), LIBLOADORDER_ERROR_PREFIX)) {
    const auto [code, details] =
        parseSystemError(std::string_view(error.what())
                             .substr(LIBLOADORDER_ERROR_PREFIX.size()));

    return std::make_exception_ptr(
        std::system_error(code, libloadorder_category(), details));
  } else if (startsWith(error.what(), LCI_ERROR_PREFIX)) {
    const auto [code, details] = parseSystemError(
        std::string_view(error.what()).substr(LCI_ERROR_PREFIX.size()));

    return std::make_exception_ptr(ConditionSyntaxError(
        code, loot_condition_interpreter_category(), details));
  } else if (startsWith(error.what(), FILE_ACCESS_ERROR_PREFIX)) {
    return std::make_exception_ptr(
        FileAccessError(getErrorSuffix(error.what())));
  } else if (startsWith(error.what(), FILE_ACCESS_ERROR_PREFIX)) {
    return std::make_exception_ptr(
        FileAccessError(getErrorSuffix(error.what())));
  } else if (startsWith(error.what(), INVALID_ARGUMENT_PREFIX)) {
    return std::make_exception_ptr(
        std::invalid_argument(getErrorSuffix(error.what())));
  } else {
    return std::make_exception_ptr(std::runtime_error(error.what()));
  }
}
}
