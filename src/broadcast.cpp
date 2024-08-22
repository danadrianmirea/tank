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
#include "tank/broadcast.h"

namespace czh::bc
{
  std::optional<msg::Message> read_message(size_t id)
  {
    int max_priority = (std::numeric_limits<int>::min)();
    auto it = g::state.users[id].messages.rbegin();
    std::vector<decltype(it)> unread;
    for (; it < g::state.users[id].messages.rend(); ++it)
    {
      if (!it->read)
      {
        if (it->priority > max_priority)
          max_priority = it->priority;
        unread.emplace_back(it);
      }
    }

    // new -> old
    for (auto& r : unread)
    {
      if (r->priority == max_priority)
      {
        r->read = true;
        return *r;
      }
    }
    return std::nullopt;
  }

}
