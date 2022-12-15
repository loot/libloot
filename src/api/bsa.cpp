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

namespace bsa::v103 {
struct FolderRecord {
  uint64_t nameHash{0};
  uint32_t fileCount{0};
  uint32_t fileRecordsOffset{0};
};

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBethesdaArchive(
    std::istream& in,
    const Header& header) {
  return detail::GetAssetsInBethesdaArchive<FolderRecord>(in, header);
}
}

namespace bsa::v104 {
using bsa::v103::FolderRecord;

using v103::GetAssetsInBethesdaArchive;
}

namespace bsa::v105 {
struct FolderRecord {
  uint64_t nameHash{0};
  uint32_t fileCount{0};
  uint32_t padding1{0};
  uint32_t fileRecordsOffset{0};
  uint32_t padding2{0};
};

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBethesdaArchive(
    std::istream& in,
    const Header& header) {
  return detail::GetAssetsInBethesdaArchive<FolderRecord>(in, header);
}
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
      logger->error("Bethesda archive path \"{}\" does not exist!",
                    archivePath.u8string());
      throw std::runtime_error("Bethesda archive does not exist");
    }
  }

  std::ifstream in(archivePath, std::ios::binary);
  in.exceptions(std::ios::failbit | std::ios::badbit |
                std::ios::eofbit);  // Causes ifstream::failure to be thrown if
                                    // a problem is encountered.

  bsa::Header header;
  in.read(reinterpret_cast<char*>(&header), sizeof(bsa::Header));

  // Validate the header.
  if (header.typeId != BSA_TYPE_ID ||
      !(header.version == 103 || header.version == 104 ||
        header.version == 105) ||
      header.recordsOffset != 36) {
    if (logger) {
      logger->error("Bethesda archive at \"{}\" has an invalid header.",
                    archivePath.u8string());
      throw std::runtime_error("Bethesda archive has an invalid header");
    }
  }

  if ((header.archiveFlags & 0x40) != 0) {
    if (logger) {
      logger->error("BSA file at \"{}\" uses big-endian numbers.");
    }
    throw std::runtime_error("BSA file uses big-endian numbers");
  }

  if (header.version == 103) {
    return bsa::v103::GetAssetsInBethesdaArchive(in, header);
  }

  if (header.version == 104) {
    return bsa::v104::GetAssetsInBethesdaArchive(in, header);
  }

  if (header.version == 105) {
    return bsa::v105::GetAssetsInBethesdaArchive(in, header);
  }

  if (logger) {
    logger->error("Unrecognised BSA version {} in archive at \"{}\"",
                  header.version,
                  archivePath.u8string());
  }

  throw std::runtime_error("BSA file has an unrecognised version");
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
            if (!fileResult.second && logger) {
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
