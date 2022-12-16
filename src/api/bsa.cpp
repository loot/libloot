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

struct GeneralFileRecord {
  uint32_t fileHash{
      0};  // Seems to be a hash of the file basename (without extension)
  uint32_t extension{0};   // First 4 bytes of file extension.
  uint32_t folderHash{0};  // Same value for files in the same directory.
  uint32_t unknown1{0};    // Always 00 01 10 00?
  uint32_t dataOffset{0};  // Offset from start of file.
  uint32_t unknown2{0};    // Always null?
  uint32_t unknown3{
      0};  // Length of the compressed data block (or null if uncompressed).
  uint32_t dataLength{0};  // Length of the uncompressed data block.
  uint32_t unknown4{0};    // Always 0D F0 AD BA?
};

struct TextureFileRecord {
  uint32_t fileHash{0};
  uint32_t extension{0};
  uint32_t folderHash{0};
  uint8_t unknown1{0};  // Always null?
  uint8_t subrecordCount{0};
  uint16_t subrecordLength{0};  // Length of a single subrecord, always 24?
  uint64_t unknown2{0};
};

void StoreHashes(std::map<uint64_t, std::set<uint64_t>>& folderFileHashes,
                 const uint32_t fileHash,
                 const uint32_t extension,
                 const uint32_t folderHash) {
  // The file hash can't be used on its own as it's common for two files
  // in the same directory to have the same basename but different
  // extension, so treat the extension as the high bytes of a 64-bit hash.
  const uint64_t fileAndExtensionHash =
      uint64_t{fileHash} | (uint64_t{extension} << 32);

  const auto folderResult = folderFileHashes.emplace(
      folderHash, std::set<uint64_t>({fileAndExtensionHash}));

  if (!folderResult.second) {
    // Folder hash already stored, add file hash to existing set.
    const auto fileResult =
        folderResult.first->second.insert(fileAndExtensionHash);

    if (!fileResult.second) {
      const auto message = fmt::format(
          "Unexpected collision for file name hash {:x} in set for folder name "
          "hash {:x}",
          fileAndExtensionHash,
          folderHash);
      throw std::runtime_error(message);
    }
  }
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInGeneralBA2(
    std::istream& in,
    const Header& header,
    const std::filesystem::path& archivePath) {
  const auto logger = getLogger();

  std::map<uint64_t, std::set<uint64_t>> folderFileHashes;

  for (size_t i = 0; i < header.fileCount; ++i) {
    GeneralFileRecord fileRecord;
    in.read(reinterpret_cast<char*>(&fileRecord), sizeof(GeneralFileRecord));

    // Validate assumptions (don't error if invalid, because they don't
    // actually matter for LOOT).
    if (fileRecord.unknown1 != 0x100100) {
      logger->warn(
          "Unexpected value for unknown1 field in file record with hash {:x} "
          "in BA2 file at \"{}\": {}",
          fileRecord.fileHash,
          archivePath.u8string(),
          fileRecord.unknown1);
    }

    if (fileRecord.unknown2 != 0) {
      logger->warn(
          "Unexpected value for unknown2 field in file record with hash {:x} "
          "in BA2 file at \"{}\": {}",
          fileRecord.fileHash,
          archivePath.u8string(),
          fileRecord.unknown2);
    }

    if (fileRecord.unknown4 != 0xBAADF00D) {
      logger->warn(
          "Unexpected value for unknown4 field in file record with hash {:x} "
          "in BA2 file at \"{}\": {}",
          fileRecord.fileHash,
          archivePath.u8string(),
          fileRecord.unknown4);
    }

    // Now store the hashes.
    StoreHashes(folderFileHashes,
                fileRecord.fileHash,
                fileRecord.extension,
                fileRecord.folderHash);
  }

  return folderFileHashes;
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInTextureBA2(
    std::istream& in,
    const Header& header,
    const std::filesystem::path& archivePath) {
  const auto logger = getLogger();

  std::map<uint64_t, std::set<uint64_t>> folderFileHashes;

  for (size_t i = 0; i < header.fileCount; ++i) {
    TextureFileRecord fileRecord;
    in.read(reinterpret_cast<char*>(&fileRecord), sizeof(TextureFileRecord));

    // Skip over this file record's subrecords.
    in.ignore(fileRecord.subrecordCount * fileRecord.subrecordLength);

    // Validate assumptions (don't error if invalid, because they don't
    // actually matter for LOOT).
    if (fileRecord.unknown1 != 0) {
      logger->warn(
          "Unexpected value for unknown1 field in file record with hash {:x} "
          "in BA2 file at \"{}\": {}",
          fileRecord.fileHash,
          archivePath.u8string(),
          fileRecord.unknown1);
    }

    if (fileRecord.subrecordLength != 24) {
      logger->warn(
          "Unexpected value for subrecordLength field in file record with hash "
          "{:x} in BA2 file at \"{}\": {}",
          fileRecord.fileHash,
          archivePath.u8string(),
          fileRecord.subrecordLength);
    }

    // Now store the hashes.
    StoreHashes(folderFileHashes,
                fileRecord.fileHash,
                fileRecord.extension,
                fileRecord.folderHash);
  }

  return folderFileHashes;
}

std::map<uint64_t, std::set<uint64_t>> GetAssetsInBA2(
    std::istream& in,
    const Header& header,
    const std::filesystem::path& archivePath) {
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

  // BA2s don't have the same structure as BSAs, for general BA2s everything is
  // stored in a flatter structure, and for texture BA2s a more specialised
  // structure is used. While there are still directory and file hashes, they're
  // 32-bit, not 64-bit, which makes collisions more likely. Still, try to use
  // them for simplicity.

  if (header.archiveType == BA2_GENERAL_TYPE) {
    return GetAssetsInGeneralBA2(in, header, archivePath);
  }

  return GetAssetsInTextureBA2(in, header, archivePath);
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

    return ba2::GetAssetsInBA2(in, header, archivePath);
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
