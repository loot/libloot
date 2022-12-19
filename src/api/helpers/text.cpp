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
static constexpr const char* dateRegex =
    R"((\d{1,2}/\d{1,2}/\d{1,4} \d{1,2}:\d{1,2}:\d{1,2}))";

/* The string below matches the range of version strings supported by
   Pseudosem v1.0.1, excluding space separators, as they make version
   extraction from inside sentences very tricky and have not been
   seen "in the wild". */
static constexpr const char* pseudosemVersionRegex =
    R"((\d+(?:\.\d+)+(?:[-._:]?[A-Za-z0-9]+)*))"
    // The string below prevents version numbers followed by a comma from
    // matching.
    R"((?!,))";

/* The string below matches a number containing one or more
   digits found at the start of the search string or preceded by
   'v' or 'version:. */
static constexpr const char* digitsVersionRegex = R"((?:^|v|version:\s*)(\d+))";

std::vector<Tag> ExtractBashTags(const std::string& description) {
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

std::optional<std::string> ExtractVersion(const std::string& text) {
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

  std::smatch what;
  for (const auto& versionRegex : versionRegexes) {
    if (std::regex_search(text, what, versionRegex)) {
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

std::wstring ToWinWide(const std::string& str) {
  const size_t len = MultiByteToWideChar(
      CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), 0, 0);

  if (len == 0) {
    return std::wstring();
  }

  std::wstring wstr(len, 0);
  MultiByteToWideChar(CP_UTF8,
                      0,
                      str.c_str(),
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

int CompareFilenames(const std::string& lhs, const std::string& rhs) {
#ifdef _WIN32
  // On Windows, use CompareStringOrdinal as that will perform case conversion
  // using the operating system uppercase table information, which (I think)
  // will give results that match the filesystem, and is not locale-dependent.
  int result = CompareStringOrdinal(
      ToWinWide(lhs).c_str(), -1, ToWinWide(rhs).c_str(), -1, true);
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
  auto unicodeLhs = icu::UnicodeString::fromUTF8(lhs);
  auto unicodeRhs = icu::UnicodeString::fromUTF8(rhs);
  return unicodeLhs.caseCompare(unicodeRhs, U_FOLD_CASE_DEFAULT);
#endif
}

std::string NormalizeFilename(const std::string& filename) {
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
}
