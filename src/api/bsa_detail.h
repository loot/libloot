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
#ifndef LOOT_API_BSA_DETAIL
#define LOOT_API_BSA_DETAIL

#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "api/helpers/logging.h"

namespace loot::bsa {
struct Header {
  std::array<char, 4> typeId;  // Should always be "BSA\0"
  uint32_t version{0};  // 103 (0x67) for TES4, 104 (0x68) for FO3, FONV, TES5,
                        // 105
                        // (0x69) for TES5SE.
  uint32_t recordsOffset{0};
  uint32_t archiveFlags{0};
  uint32_t folderCount{0};
  uint32_t totalFileCount{0};
  uint32_t totalFolderNamesLength{0};
  uint32_t totalFileNamesLength{0};
  uint32_t contentTypeFlags{0};
};

struct FileRecord {
  uint64_t nameHash{0};
  uint32_t dataLength{0};
  uint32_t dataOffset{0};
};
}

namespace loot::bsa::detail {
template<typename FolderRecord>
std::map<uint64_t, std::set<uint64_t>> GetAssetsInBSA(std::istream& in,
                                                      const Header& header) {
  const auto logger = getLogger();

  std::vector<FolderRecord> folderRecords(header.folderCount);
  in.read(reinterpret_cast<char*>(folderRecords.data()),
          sizeof(FolderRecord) * folderRecords.size());

  // The next block consists of per-folder subblocks that are each a
  // byte containing the folder name length, the null-terminated folder name
  // and then the file records for that folder.
  const auto fileRecordsSize = header.folderCount +
                               header.totalFolderNamesLength +
                               sizeof(FileRecord) * header.totalFileCount;
  std::vector<uint8_t> fileRecordsBytes(fileRecordsSize);
  in.read(reinterpret_cast<char*>(fileRecordsBytes.data()), fileRecordsSize);

  // For each folder record, store its hash with the hashes of the files in that
  // folder.
  std::map<uint64_t, std::set<uint64_t>> folderFileHashes;

  // FolderRecord.fileRecordsOffset is relative to this baseline. In the file
  // fileRecordsOffset - header.totalFileNamesLength is the start off the
  // folder's subblock relative to the start of the file, but the baseline is
  // from the start of the fileRecords vector.
  const auto folderRecordOffsetBaseline =
      sizeof(Header) + sizeof(FolderRecord) * header.folderCount +
      header.totalFileNamesLength;

  for (const auto& folderRecord : folderRecords) {
    const auto folderHash = folderRecord.nameHash;

    const auto folderResult =
        folderFileHashes.emplace(folderHash, std::set<uint64_t>());

    if (!folderResult.second) {
      throw std::runtime_error("Unexpected collision for folder name hash " +
                               std::to_string(folderHash));
    }

    size_t fileRecordsOffset = 0;
    if ((header.archiveFlags & 0x1) == 0) {
      // Directory names are not included.
      fileRecordsOffset =
          folderRecord.fileRecordsOffset - folderRecordOffsetBaseline;
    } else {
      // Directory names are included.
      const auto folderNameLengthOffset =
          folderRecord.fileRecordsOffset - folderRecordOffsetBaseline;

      const auto folderNameLength = fileRecordsBytes.at(folderNameLengthOffset);

      // The real file records offset.
      fileRecordsOffset = folderNameLengthOffset + 1 + folderNameLength;
    }

    for (size_t i = 0; i < folderRecord.fileCount; ++i) {
      const auto fileRecordOffset =
          fileRecordsBytes.data() + fileRecordsOffset + i * sizeof(FileRecord);

      const FileRecord* fileRecord =
          reinterpret_cast<FileRecord*>(fileRecordOffset);

      const auto result =
          folderResult.first->second.insert(fileRecord->nameHash);

      if (!result.second) {
        throw std::runtime_error("Unexpected collision for file name hash " +
                                 std::to_string(fileRecord->nameHash) +
                                 " in set for folder name hash " +
                                 std::to_string(folderHash));
      }
    }
  }

  return folderFileHashes;
}
}

#endif
