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
#ifndef TANK_BROADCAST_H
#define TANK_BROADCAST_H
#pragma once

#include "game.h"
#include "message.h"

#include <utility>
#include <string_view>

namespace czh::bc
{
  constexpr size_t to_everyone = (std::numeric_limits<size_t>::max)();
  constexpr size_t from_system = (std::numeric_limits<size_t>::max)();

  template<typename... Args>
  int send_message(size_t from, size_t to, int priority, const std::string& c)
  {
    msg::Message msg{
      .from = from, .content = c,
      .priority = priority, .read = false,
      .time = std::chrono::duration_cast<std::chrono::seconds>
      (std::chrono::system_clock::now().time_since_epoch()).count()
    };
    if (to == to_everyone)
    {
      for (auto& r : g::state.users | std::views::values)
        r.messages.emplace_back(msg);
    }
    else
    {
      auto t = g::state.users.find(to);
      if (t == g::state.users.end()) return -1;
      t->second.messages.emplace_back(msg);
    }
    return 0;
  }

  std::optional<msg::Message> read_message(size_t id);


  enum class Severity : int
  {
    trace = -10,
    info = -20,
    warn = 10,
    error = 20,
    critical = 30
  };

  template<typename... Args>
  void log_helper(size_t to, Severity severity, std::format_string<Args...> fmt, Args&&... args)
  {
    std::string str;
    int priority = static_cast<int>(severity);
    switch (severity)
    {
      case Severity::trace:
        str = "[TRACE] ";
        break;
      case Severity::info:
        str = "[INFO] ";
        break;
      case Severity::warn:
        str = utils::color_256_fg("[WARNING] ", 11);
        break;
      case Severity::error:
        str = utils::color_256_fg("[ERROR] ", 9);
        break;
      case Severity::critical:
        str = utils::color_256_fg("[CRITICAL] ", 9);
        break;
    }
    send_message(from_system, to, priority, str + std::format(fmt, std::forward<Args>(args)...));
  }

  template<typename... Args>
  void trace(size_t id, std::format_string<Args...> fmt, Args&&... args)
  {
    log_helper(id, Severity::trace, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void info(size_t id, std::format_string<Args...> fmt, Args&&... args)
  {
    log_helper(id, Severity::info, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void warn(size_t id, std::format_string<Args...> fmt, Args&&... args)
  {
    log_helper(id, Severity::warn, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void error(size_t id, std::format_string<Args...> fmt, Args&&... args)
  {
    log_helper(id, Severity::error, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  void critical(size_t id, std::format_string<Args...> fmt, Args&&... args)
  {
    log_helper(id, Severity::critical, fmt, std::forward<Args>(args)...);
  }

  inline void trace(size_t id, const std::string& c)
  {
    log_helper(id, Severity::trace, "{}", c);
  }

  inline void info(size_t id, const std::string& c)
  {
    log_helper(id, Severity::info, "{}", c);
  }

  inline void warn(size_t id, const std::string& c)
  {
    log_helper(id, Severity::warn, "{}", c);
  }

  inline void error(size_t id, const std::string& c)
  {
    log_helper(id, Severity::error, "{}", c);
  }

  inline void critical(size_t id, const std::string& c)
  {
    log_helper(id, Severity::critical, "{}", c);
  }
}
#endif
