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
#include "tank/message.h"
#include "tank/globals.h"
#include "tank/utils.h"

#include <string>
#include <numeric>

namespace czh::msg
{
  bool operator<(const Message& m1, const Message& m2)
  {
    return m1.priority < m2.priority;
  }

  int send_message(int from, int to, const std::string& msg_content, int priority)
  {
    Message msg{
      .from = from, .content = msg_content,
      .priority = priority, .read = false,
      .time = std::chrono::duration_cast<std::chrono::seconds>
      (std::chrono::system_clock::now().time_since_epoch()).count()
    };
    if (to == -1)
    {
      for (auto& r : g::userdata)
        r.second.messages.emplace_back(msg);
    }
    else
    {
      auto t = g::userdata.find(to);
      if (t == g::userdata.end()) return -1;
      t->second.messages.emplace_back(msg);
    }
    return 0;
  }

  void log_helper(int id, const std::string& s, const std::string& content, int priority)
  {
    send_message(-1, id, s + content, priority);
  }

  void trace(int id, const std::string& c)
  {
    log_helper(id, "[TRACE] ", c, -20);
  }

  void info(int id, const std::string& c)
  {
    log_helper(id, "[INFO] ", c, -10);
  }

  void warn(int id, const std::string& c)
  {
    log_helper(id, utils::color_256_fg("[WARNING] ", 11), c, 10);
  }

  void error(int id, const std::string& c)
  {
    log_helper(id, utils::color_256_fg("[ERROR] ", 9), c, 20);
  }

  void critical(int id, const std::string& c)
  {
    log_helper(id, utils::color_256_fg("[CRITICAL] ", 9), c, 30);
  }

  void trace(size_t id, const std::string& c)
  {
    log_helper(static_cast<int>(id), "[TRACE] ", c, -20);
  }

  void info(size_t id, const std::string& c)
  {
    log_helper(static_cast<int>(id), "[INFO] ", c, -10);
  }

  void warn(size_t id, const std::string& c)
  {
    log_helper(static_cast<int>(id), utils::color_256_fg("[WARNING] ", 11), c, 10);
  }

  void error(size_t id, const std::string& c)
  {
    log_helper(static_cast<int>(id), utils::color_256_fg("[ERROR] ", 9), c, 20);
  }

  void critical(size_t id, const std::string& c)
  {
    log_helper(static_cast<int>(id), utils::color_256_fg("[CRITICAL] ", 9), c, 30);
  }

  std::optional<Message> read_a_message(size_t id)
  {
    int max_priority = (std::numeric_limits<int>::min)();
    auto it = g::userdata[id].messages.rbegin();
    std::vector<decltype(it)> unread;
    for (;it < g::userdata[id].messages.rend(); ++it)
    {
      if (!it->read)
      {
        if(it->priority > max_priority)
          max_priority = it->priority;
        unread.emplace_back(it);
      }
    }

    // new -> old
    for(auto& r : unread)
    {
      if(r->priority == max_priority)
      {
        r->read = true;
        return *r;
      }
    }
    return std::nullopt;
  }
}
