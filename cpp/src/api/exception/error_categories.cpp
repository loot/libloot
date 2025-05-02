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

#include "loot/exception/error_categories.h"

#include <string>

namespace loot {
namespace detail {
class esplugin_category : public std::error_category {
  const char* name() const noexcept override { return "esplugin"; }

  std::string message(int) const override { return "esplugin error"; }

  bool equivalent(const std::error_code& code, int) const noexcept override {
    return code.category().name() == name();
  }
};

class libloadorder_category : public std::error_category {
  const char* name() const noexcept override { return "libloadorder"; }

  std::string message(int) const override { return "Libloadorder error"; }

  bool equivalent(const std::error_code& code, int) const noexcept override {
    return code.category().name() == name();
  }
};

class loot_condition_interpreter_category : public std::error_category {
  const char* name() const noexcept override {
    return "loot condition interpreter";
  }

  std::string message(int) const override {
    return "loot condition interpreter error";
  }

  bool equivalent(const std::error_code& code, int) const noexcept override {
    return code.category().name() == name();
  }
};
}

LOOT_API const std::error_category& esplugin_category() {
  static detail::esplugin_category instance;
  return instance;
}

LOOT_API const std::error_category& libloadorder_category() {
  static detail::libloadorder_category instance;
  return instance;
}

LOOT_API const std::error_category& loot_condition_interpreter_category() {
  static detail::loot_condition_interpreter_category instance;
  return instance;
}
}
