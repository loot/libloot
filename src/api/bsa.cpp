/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2022 Oliver Hamlet

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

#include "api/bsa.h"

#include <boost/algorithm/string.hpp>
#include <fstream>

#include "api/bsa_detail.h"
#include "api/helpers/logging.h"

namespace loot {
/*
BSA format documentation:

- Oblivion: https://en.uesp.net/wiki/Oblivion_Mod:BSA_File_Format
- Fallout 3, Fallout New Vegas, Skyrim, Skyrim Special Edition:
  https://en.uesp.net/wiki/Skyrim_Mod:Archive_File_Format

*/
constexpr std::array<char, 4> BSA_TYPE_ID = {'B', 'S', 'A', '\0'};
constexpr std::array<char, 4> BA2_TYPE_ID = {'B', 'T', 'D', 'X'};
constexpr std::array<char, 4> BA2_GENERAL_TYPE = {'G', 'N', 'R', 'L'};
constexpr std::array<char, 4> BA2_TEXTURE_TYPE = {'D', 'X', '1', '0'};

namespace bsa {
namespace v103 {
struct FolderRecord {
  uint64_t nameHash{0};
  uint32_t fileCount{0};
  uint32_t fileRecordsOffset{0};
};

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBSA(std::istream& in,
                                                      const Header& header) {
  return detail::GetAssetsInBSA<FolderRecord>(in, header);
}
}

namespace v104 {
using v103::FolderRecord;

using v103::GetAssetsInBSA;
}

namespace v105 {
struct FolderRecord {
  uint64_t nameHash{0};
  uint32_t fileCount{0};
  uint32_t padding1{0};
  uint32_t fileRecordsOffset{0};
  uint32_t padding2{0};
};

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBSA(std::istream& in,
                                                      const Header& header) {
  return detail::GetAssetsInBSA<FolderRecord>(in, header);
}
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBSA(
    std::istream& in,
    const bsa::Header& header) {
  const auto logger = getLogger();

  // Validate the header.
  if (header.typeId != BSA_TYPE_ID ||
      !(header.version == 103 || header.version == 104 ||
        header.version == 105) ||
      header.recordsOffset != 36) {
    throw std::runtime_error("BSA file has an invalid header");
  }

  if ((header.archiveFlags & 0x40) != 0) {
    throw std::runtime_error("BSA file uses big-endian numbers");
  }

  if (header.version == 103) {
    return bsa::v103::GetAssetsInBSA(in, header);
  }

  if (header.version == 104) {
    return bsa::v104::GetAssetsInBSA(in, header);
  }

  if (header.version == 105) {
    return bsa::v105::GetAssetsInBSA(in, header);
  }

  throw std::runtime_error("BSA file has an unrecognised version");
}
}

namespace ba2 {
struct Header {
  std::array<char, 4> typeId;
  uint32_t version{0};
  std::array<char, 4> archiveType;
  uint32_t fileCount{0};
  uint64_t filePathsOffset{0};
};

void StoreHashes(std::map<uint64_t, std::set<uint64_t>>& folderFileHashes,
                 const uint64_t fileHash,
                 const uint64_t folderHash) {
  const auto folderResult =
      folderFileHashes.emplace(folderHash, std::set<uint64_t>({fileHash}));

  if (!folderResult.second) {
    // Folder hash already stored, add file hash to existing set.
    const auto fileResult = folderResult.first->second.insert(fileHash);

    if (!fileResult.second) {
      const auto message = fmt::format(
          "Unexpected collision for file name hash {:x} in set for folder name "
          "hash {:x}",
          fileHash,
          folderHash);
      throw std::runtime_error(message);
    }
  }
}

// Normalise the path the same way that BA2 hashes do (it's thet same as for
// BSAs).
void NormalisePath(std::string& filePath) {
  for (size_t i = 0; i < filePath.size(); ++i) {
    // Ignore any non-ASCII characters.
    if (filePath[i] > 127) {
      continue;
    }

    // Replace any forwardslashes with backslashes and
    // lowercase any other characters.
    filePath[i] = filePath[i] == '/'
                      ? '\\'
                      : static_cast<char>(std::tolower(
                            static_cast<unsigned char>(filePath[i])));
  }
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBA2FromFilePaths(
    std::istream& in,
    const Header& header) {
  // BA2s use 32-bit hashes and I've observed collisions between different
  // official Fallout 4 BA2s, so calculate new 64-bit hashes instead of
  // using the hashes in the BA2.
  const auto logger = getLogger();

  std::map<uint64_t, std::set<uint64_t>> folderFileHashes;

  // Skip to list of file paths at the end of the BA2.
  in.seekg(header.filePathsOffset, std::ios_base::beg);

  // The file paths are prefixed by a two-byte length, and not null-terminated.
  for (size_t i = 0; i < header.fileCount; ++i) {
    uint16_t pathLength;
    in.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));

    std::string filePath(pathLength, '\0');
    in.read(filePath.data(), pathLength);

    // Normalise the path the same as is done for BSA/BA2 hash calculation,
    // so that equivalent but not equal paths (e.g. due to upper/lowercase
    // differences) are hashed to the same value.
    NormalisePath(filePath);

    // Trim trailing and leading slashes.
    boost::trim_if(filePath, [](const char c) { return c == '\\'; });

    // Now split the path so that its folder and file hashes can be calculated.
    const auto index = filePath.rfind("\\");
    if (index == std::string::npos) {
      // No slash, no directory, use a hash of zero.
      const uint64_t fileHash = std::hash<std::string>{}(filePath);
      const uint64_t folderHash = 0;
      StoreHashes(folderFileHashes, fileHash, folderHash);
    } else {
      // Split the string in two.
      const auto folderPath = filePath.substr(0, index);
      filePath = filePath.substr(index + 1);

      const uint64_t fileHash = std::hash<std::string>{}(filePath);
      const uint64_t folderHash = std::hash<std::string>{}(folderPath);
      StoreHashes(folderFileHashes, fileHash, folderHash);
    }
  }

  return folderFileHashes;
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBA2(std::istream& in,
                                                      const Header& header) {
  // Validate the header.
  if (header.typeId != BA2_TYPE_ID) {
    throw std::runtime_error("BA2 file header type ID is invalid");
  }

  if (header.version != 1) {
    throw std::runtime_error("BA2 file header version is invalid");
  }

  if (header.archiveType != BA2_GENERAL_TYPE &&
      header.archiveType != BA2_TEXTURE_TYPE) {
    throw std::runtime_error("BA2 file header archive type is invalid");
  }

  return GetAssetsInBA2FromFilePaths(in, header);
}
}

// Fallout4.esm and DLCUltraHighResolution.esm from Fallout 4 have the
// same file path appearing in multiple BA2 files, so ignore hash
// collision warnings for those files as otherwise they cause a lot of
// noise in the logs.
bool ShouldWarnAboutHashCollisions(const std::filesystem::path& archivePath) {
  const auto filename = archivePath.filename().u8string();

  return !boost::iends_with(filename, ".ba2") ||
         (!boost::istarts_with(filename, "Fallout4 - ") &&
          !boost::istarts_with(filename, "DLCUltraHighResolution - "));
}

bool DoFileNameHashSetsIntersect(const std::set<uint64_t>& left,
                                 const std::set<uint64_t>& right) {
  auto leftIt = left.begin();
  auto rightIt = right.begin();

  while (leftIt != left.end() && rightIt != right.end()) {
    if (*leftIt < *rightIt) {
      ++leftIt;
    } else if (*leftIt > *rightIt) {
      ++rightIt;
    } else {
      return true;
    }
  }

  return false;
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBethesdaArchive(
    const std::filesystem::path& archivePath) {
  // If parsing the BSA fails, log the error but don't throw an exception as
  // an issue with one archive (which may just be invalid) shouldn't cause
  // others not to be loaded.

  const auto logger = getLogger();

  if (!std::filesystem::exists(archivePath)) {
    if (logger) {
      throw std::runtime_error("Bethesda archive does not exist");
    }
  }

  std::ifstream in(archivePath, std::ios::binary);
  in.exceptions(std::ios::failbit | std::ios::badbit |
                std::ios::eofbit);  // Causes ifstream::failure to be thrown if
                                    // a problem is encountered.

  std::array<char, 4> typeId;
  in.read(typeId.data(), typeId.size());

  if (typeId == BSA_TYPE_ID) {
    bsa::Header header;
    header.typeId = typeId;

    // Read the rest of the header.
    in.read(reinterpret_cast<char*>(&header) + typeId.size(),
            sizeof(bsa::Header) - typeId.size());

    return bsa::GetAssetsInBSA(in, header);
  }

  if (typeId == BA2_TYPE_ID) {
    ba2::Header header;
    header.typeId = typeId;

    // Read the rest of the header.
    in.read(reinterpret_cast<char*>(&header) + typeId.size(),
            sizeof(ba2::Header) - typeId.size());

    return ba2::GetAssetsInBA2(in, header);
  }

  throw std::runtime_error("Bethesda archive has unrecognised typeId");
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBethesdaArchives(
    const std::vector<std::filesystem::path>& archivePaths) {
  const auto logger = getLogger();

  std::map<uint64_t, std::set<uint64_t>> archiveAssets;

  for (const auto& archivePath : archivePaths) {
    try {
      if (logger) {
        logger->trace(
            "Getting assets loaded from the Bethesda archive at \"{}\"",
            archivePath.u8string());
      }

      const auto warnAboutHashCollisions =
          ShouldWarnAboutHashCollisions(archivePath);

      const auto assets = GetAssetsInBethesdaArchive(archivePath);
      for (const auto& asset : assets) {
        const auto folderResult = archiveAssets.insert(asset);
        if (!folderResult.second) {
          // Folder already exists, add the files to its set.
          // Don't just insert the range, as it would be good to
          // log if a file's hash is already present - you wouldn't
          // expect the same file to appear in the same folder in
          // two different BSAs loaded by the same plugin.
          /*result.first->second.insert(asset.second.begin(),
                                      asset.second.end());*/
          for (const auto& fileNameHash : asset.second) {
            const auto fileResult =
                folderResult.first->second.insert(fileNameHash);
            if (!fileResult.second && warnAboutHashCollisions && logger) {
              logger->warn(
                  "The folder and file with hashes {:x} and {:x} in \"{}\" are "
                  "present in another BSA.",
                  asset.first,
                  fileNameHash,
                  archivePath.u8string());
            }
          }
        }
      }
    } catch (const std::exception& e) {
      if (logger) {
        logger->error(
            "Caught exception while trying to read Bethesda archive file "
            "at \"{}\": {}",
            archivePath.u8string(),
            e.what());
      }
    }
  }

  return archiveAssets;
}

bool DoAssetsIntersect(const std::map<uint64_t, std::set<uint64_t>>& left,
                       const std::map<uint64_t, std::set<uint64_t>>& right) {
  auto leftIt = left.begin();
  auto rightIt = right.begin();

  while (leftIt != left.end() && rightIt != right.end()) {
    if (leftIt->first < rightIt->first) {
      ++leftIt;
    } else if (leftIt->first > rightIt->first) {
      ++rightIt;
    } else if (DoFileNameHashSetsIntersect(leftIt->second, rightIt->second)) {
      return true;
    } else {
      // The folder hashes are equal but they don't contain any of the same
      // file hashes, move on to the next folder. It doesn't matter which
      // iterator gets incremented.
      ++leftIt;
    }
  }

  return false;
}
}
