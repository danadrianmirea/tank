//   Copyright 2022-2024 tank - caozhanhao
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
#ifndef TANK_DEBUG_H
#define TANK_DEBUG_H
#pragma once

#include <string>
#include <stdexcept>
#include <format>
#include <print>
#include <source_location>

namespace czh::dbg
{
  inline void tank_assert(bool b,
    const std::string& detail_ = "",
    const std::source_location& sl = std::source_location::current())
  {
    if (!b)
    {
      auto what = std::format("{}:{}:{}: In function '{}': Assertion Failed:\n{}",
        sl.file_name(), sl.line(), sl.column(), sl.function_name(), detail_);
      std::println(stderr, "{}", what);
      throw std::runtime_error(what);
    }
  }

  inline std::string message;
}
#endif
