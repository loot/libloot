/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */
#include "api/helpers/text.h"

#include <boost/algorithm/string.hpp>
#include <regex>

#ifdef _WIN32
#include "windows.h"
#else
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#endif

namespace loot {
/* The string below matches timestamps that use forwardslashes for date
   separators. However, Pseudosem v1.0.1 will only compare the first
   two digits as it does not recognise forwardslashes as separators. */
constexpr const char* dateRegex =
    R"((\d{1,2}/\d{1,2}/\d{1,4} \d{1,2}:\d{1,2}:\d{1,2}))";

/* The string below matches the range of version strings supported by
   Pseudosem v1.0.1, excluding space separators, as they make version
   extraction from inside sentences very tricky and have not been
   seen "in the wild". */
constexpr const char* pseudosemVersionRegex =
    R"((\d+(?:\.\d+)+(?:[-._:]?[A-Za-z0-9]+)*))"
    // The string below prevents version numbers followed by a comma from
    // matching.
    R"((?!,))";

/* The string below matches a number containing one or more
   digits found at the start of the search string or preceded by
   'v' or 'version:. */
constexpr const char* digitsVersionRegex = R"((?:^|v|version:\s*)(\d+))";

std::vector<Tag> ExtractBashTags(std::string_view description) {
  std::vector<Tag> tags;

  static constexpr const char* BASH_TAGS_OPENER = "{{BASH:";
  static constexpr std::size_t BASH_TAGS_OPENER_LENGTH =
      std::char_traits<char>::length(BASH_TAGS_OPENER);

  size_t startPos = description.find("{{BASH:");
  if (startPos == std::string::npos ||
      startPos + BASH_TAGS_OPENER_LENGTH >= description.length()) {
    return tags;
  }
  startPos += BASH_TAGS_OPENER_LENGTH;

  const size_t endPos = description.find("}}", startPos);
  if (endPos == std::string::npos) {
    return tags;
  }

  auto commaSeparatedTags = description.substr(startPos, endPos - startPos);

  std::vector<std::string> bashTags;
  boost::split(bashTags, commaSeparatedTags, [](char c) { return c == ','; });

  for (auto& tag : bashTags) {
    boost::trim(tag);
    tags.push_back(Tag(tag));
  }

  return tags;
}

std::optional<std::string> ExtractVersion(std::string_view text) {
  using std::regex;

  /* There are a few different version formats that can appear in strings
   together, and in order to extract the correct one, they must be searched
   for in order of priority. */
  static const std::vector<regex> versionRegexes({
      regex(dateRegex, regex::ECMAScript | regex::icase),
      regex(std::string(R"(version:?\s)") + pseudosemVersionRegex,
            regex::ECMAScript | regex::icase),
      regex(std::string(R"((?:^|v|\s))") + pseudosemVersionRegex,
            regex::ECMAScript | regex::icase),
      regex(digitsVersionRegex, regex::ECMAScript | regex::icase),
  });

  std::match_results<std::string_view::iterator> what;
  for (const auto& versionRegex : versionRegexes) {
    if (std::regex_search(text.begin(), text.end(), what, versionRegex)) {
      for (auto it = next(begin(what)); it != end(what); ++it) {
        if (it->str().empty())
          continue;

        // Use the first non-empty sub-match.
        std::string version = *it;
        boost::trim(version);
        return version;
      }
    }
  }

  return std::nullopt;
}

#ifdef _WIN32
int narrow(size_t value) {
  auto castValue = static_cast<int>(value);

  // Cast back again to check if any data has been lost.
  // Because one type is signed and the other is unsigned, also check that
  // the sign has been preserved.
  if (static_cast<size_t>(castValue) != value ||
      ((castValue < int{}) != (value < size_t{}))) {
    throw std::runtime_error("Failed to losslessly convert from size_t to int");
  }

  return castValue;
}

std::wstring ToWinWide(std::string_view str) {
  const size_t len = MultiByteToWideChar(
      CP_UTF8, 0, str.data(), static_cast<int>(str.length()), 0, 0);

  if (len == 0) {
    return std::wstring();
  }

  std::wstring wstr(len, 0);
  MultiByteToWideChar(CP_UTF8,
                      0,
                      str.data(),
                      narrow(str.length()),
                      wstr.data(),
                      narrow(wstr.length()));
  return wstr;
}

std::string FromWinWide(const std::wstring& wstr) {
  const size_t len = WideCharToMultiByte(CP_UTF8,
                                         0,
                                         wstr.c_str(),
                                         narrow(wstr.length()),
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);

  if (len == 0) {
    return std::string();
  }

  std::string str(len, 0);
  WideCharToMultiByte(CP_UTF8,
                      0,
                      wstr.c_str(),
                      narrow(wstr.length()),
                      str.data(),
                      narrow(str.length()),
                      nullptr,
                      nullptr);
  return str;
}
#endif

ComparableFilename ToComparableFilename(std::string_view filename) {
#ifdef _WIN32
  return ToWinWide(filename);
#else
  return icu::UnicodeString::fromUTF8(filename);
#endif
}

int CompareFilenames(std::string_view lhs, std::string_view rhs) {
  return CompareFilenames(ToComparableFilename(lhs), ToComparableFilename(rhs));
}

int CompareFilenames(const ComparableFilename& lhs,
                     const ComparableFilename& rhs) {
#ifdef _WIN32
  // Use CompareStringOrdinal as that will perform case conversion
  // using the operating system uppercase table information, which (I think)
  // will give results that match the filesystem, and is not locale-dependent.
  int result = CompareStringOrdinal(
      lhs.c_str(), -1, rhs.c_str(), -1, true);
  switch (result) {
    case CSTR_LESS_THAN:
      return -1;
    case CSTR_EQUAL:
      return 0;
    case CSTR_GREATER_THAN:
      return 1;
    default:
      throw std::invalid_argument(
          "One of the filenames to compare was invalid.");
  }
#else
  return lhs.caseCompare(rhs, U_FOLD_CASE_DEFAULT);
#endif
}

std::string NormalizeFilename(std::string_view filename) {
#ifdef _WIN32
  auto wideString = ToWinWide(filename);

  if (wideString.empty()) {
    return std::string();
  }

  CharUpperBuffW(wideString.data(), narrow(wideString.length()));
  return FromWinWide(wideString);
#else
  std::string normalizedFilename;
  icu::UnicodeString::fromUTF8(filename)
      .foldCase(U_FOLD_CASE_DEFAULT)
      .toUTF8String(normalizedFilename);
  return normalizedFilename;
#endif
}

std::string TrimDotGhostExtension(std::string&& filename) {
  // If the name passed ends in '.ghost', that should be trimmed.
  if (boost::iends_with(filename, GHOST_FILE_EXTENSION)) {
    return filename.substr(0, filename.length() - GHOST_FILE_EXTENSION_LENGTH);
  }

  return filename;
}
}
