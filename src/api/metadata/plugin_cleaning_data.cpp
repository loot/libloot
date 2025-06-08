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

#include "loot/metadata/plugin_cleaning_data.h"

namespace loot {
PluginCleaningData::PluginCleaningData(uint32_t crc, std::string_view utility) :
    crc_(crc), utility_(utility) {}

PluginCleaningData::PluginCleaningData(
    uint32_t crc,
    std::string_view utility,
    const std::vector<MessageContent>& detail,
    unsigned int itm,
    unsigned int ref,
    unsigned int nav) :
    crc_(crc),
    itm_(itm),
    ref_(ref),
    nav_(nav),
    utility_(utility),
    detail_(detail) {}

uint32_t PluginCleaningData::GetCRC() const { return crc_; }

unsigned int PluginCleaningData::GetITMCount() const { return itm_; }

unsigned int PluginCleaningData::GetDeletedReferenceCount() const {
  return ref_;
}

unsigned int PluginCleaningData::GetDeletedNavmeshCount() const { return nav_; }

std::string PluginCleaningData::GetCleaningUtility() const { return utility_; }

std::vector<MessageContent> PluginCleaningData::GetDetail() const {
  return detail_;
}

bool operator==(const PluginCleaningData& lhs, const PluginCleaningData& rhs) {
  return lhs.GetCRC() == rhs.GetCRC() &&
         lhs.GetITMCount() == rhs.GetITMCount() &&
         lhs.GetDeletedReferenceCount() == rhs.GetDeletedReferenceCount() &&
         lhs.GetDeletedNavmeshCount() == rhs.GetDeletedNavmeshCount() &&
         lhs.GetCleaningUtility() == rhs.GetCleaningUtility() &&
         lhs.GetDetail() == rhs.GetDetail();
}

bool operator!=(const PluginCleaningData& lhs, const PluginCleaningData& rhs) {
  return !(lhs == rhs);
}

bool operator<(const PluginCleaningData& lhs, const PluginCleaningData& rhs) {
  if (lhs.GetCRC() < rhs.GetCRC()) {
    return true;
  }

  if (rhs.GetCRC() < lhs.GetCRC()) {
    return false;
  }

  if (lhs.GetCleaningUtility() < rhs.GetCleaningUtility()) {
    return true;
  }

  if (rhs.GetCleaningUtility() < lhs.GetCleaningUtility()) {
    return false;
  }

  if (lhs.GetITMCount() < rhs.GetITMCount()) {
    return true;
  }

  if (rhs.GetITMCount() < lhs.GetITMCount()) {
    return false;
  }

  if (lhs.GetDeletedReferenceCount() < rhs.GetDeletedReferenceCount()) {
    return true;
  }

  if (rhs.GetDeletedReferenceCount() < lhs.GetDeletedReferenceCount()) {
    return false;
  }

  if (lhs.GetDeletedNavmeshCount() < rhs.GetDeletedNavmeshCount()) {
    return true;
  }

  if (rhs.GetDeletedNavmeshCount() < lhs.GetDeletedNavmeshCount()) {
    return false;
  }

  return lhs.GetDetail() < rhs.GetDetail();
}

bool operator>(const PluginCleaningData& lhs, const PluginCleaningData& rhs) {
  return rhs < lhs;
}

bool operator<=(const PluginCleaningData& lhs, const PluginCleaningData& rhs) {
  return !(lhs > rhs);
}

bool operator>=(const PluginCleaningData& lhs, const PluginCleaningData& rhs) {
  return !(lhs < rhs);
}
}
