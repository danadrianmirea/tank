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
#include "tank/drawing.h"
#include "tank/broadcast.h"
#include "tank/bullet.h"
#include "tank/config.h"
#include "tank/game.h"
#include "tank/input.h"
#include "tank/online.h"
#include "tank/term.h"
#include "tank/utils/debug.h"
#include "tank/utils/utils.h"

#include <iomanip>
#include <mutex>
#include <ranges>
#include <string>
#include <vector>

namespace czh::draw
{
  DrawingState state{
    .inited = false,
    .focus = 0,
    .status_pos = 0,
    .help_pos = 0,
    .notification_pos = 0,
    .visible_zone = {-128, 128, -128, 128},
    .height = term::get_height(),
    .width = term::get_width(),
    .fps = 60,
    .last_drawing = std::chrono::steady_clock::now(),
    .last_message_displayed = std::chrono::steady_clock::now(),
    .style =
    Style{.background = 15, .wall = 9, .tanks = {10, 3, 4, 5, 6, 11, 12, 13, 14, 57, 100, 214}}
  };
  std::mutex drawing_mtx;
  const PointView empty_point_view{.status = map::Status::END, .tank_id = -1, .text = ""};
  const PointView wall_point_view{.status = map::Status::WALL, .tank_id = -1, .text = ""};

  const PointView& generate(const map::Pos& i, size_t seed)
  {
    if (map::generate(i, seed).has(map::Status::WALL))
    {
      return wall_point_view;
    }
    return empty_point_view;
  }

  const PointView& generate(int x, int y, size_t seed)
  {
    if (map::generate(x, y, seed).has(map::Status::WALL))
    {
      return wall_point_view;
    }
    return empty_point_view;
  }

  bool PointView::is_empty() const { return status == map::Status::END; }

  const PointView& MapView::at(int x, int y) const { return at(map::Pos(x, y)); }

  const PointView& MapView::at(const map::Pos& i) const
  {
    if (view.contains(i))
    {
      return view.at(i);
    }
    return draw::generate(i, seed);
  }

  bool MapView::is_empty() const { return view.empty(); }

  PointView extract_point(const map::Pos& p)
  {
    if (map::map.has(map::Status::TANK, p))
    {
      return {
        .status = map::Status::TANK, .tank_id = static_cast<int>(map::map.at(p).get_tank()->get_id()), .text = ""
      };
    }
    else if (map::map.has(map::Status::BULLET, p))
    {
      return {
        .status = map::Status::BULLET,
        .tank_id = static_cast<int>(map::map.at(p).get_bullets()[0]->get_tank()),
        .text = map::map.at(p).get_bullets()[0]->get_text()
      };
    }
    else if (map::map.has(map::Status::WALL, p))
    {
      return {.status = map::Status::WALL, .tank_id = -1, .text = ""};
    }
    else
    {
      return {.status = map::Status::END, .tank_id = -1, .text = ""};
    }
    return {};
  }

  MapView extract_map(const map::Zone& zone)
  {
    MapView ret;
    ret.seed = map::map.seed;
    for (int i = zone.x_min; i < zone.x_max; ++i)
    {
      for (int j = zone.y_min; j < zone.y_max; ++j)
      {
        if (!map::map.at(i, j).is_generated())
        {
          ret.view[map::Pos{i, j}] = extract_point({i, j});
        }
      }
    }
    return ret;
  }

  std::map<size_t, TankView> extract_tanks()
  {
    std::map<size_t, TankView> view;
    for (auto& r : g::state.tanks | std::views::values)
    {
      auto tv = TankView{
        .id = r->get_id(),
        .name = r->name,
        .max_hp = r->max_hp,
        .hp = r->hp,
        .is_auto = r->is_auto,
        .is_alive = r->is_alive(),
        .pos = r->pos,
        .direction = r->direction,
        .bullet_lethality = r->bullet_lethality
      };
      if (r->is_auto)
      {
        auto at = dynamic_cast<tank::AutoTank*>(r);
        if (at->is_target_good())
        {
          tv.gap = at->gap;
          tv.has_good_target = true;
          tv.target_id = at->get_target_id();
        }
      }
      view[r->get_id()] = tv;
    }
    return view;
  }

  std::optional<TankView> view_id_at(size_t id)
  {
    auto it = state.snapshot.tanks.find(id);
    if (it == state.snapshot.tanks.end())
      return std::nullopt;
    return it->second;
  }

  std::map<size_t, UserView> extract_userinfo()
  {
    std::map<size_t, UserView> view;
    for (auto& r : g::state.users)
    {
      view[r.first] = UserView{.user_id = r.second.user_id, .ip = r.second.ip, .active = r.second.active};
    }
    return view;
  }

  std::string colorify_text(size_t id, const std::string& str)
  {
    int color;
    if (id == 0)
    {
      color = state.style.tanks[0];
    }
    else
    {
      color = state.style.tanks[id % state.style.tanks.size()];
    }
    return utils::color_256_fg(str, color);
  }

  std::string colorify_tank(size_t id, const std::string& str)
  {
    int color;
    if (id == 0)
    {
      color = state.style.tanks[0];
    }
    else
    {
      color = state.style.tanks[id % state.style.tanks.size()];
    }
    return utils::color_256_bg(str, color);
  }

  void update_point(const map::Pos& pos)
  {
    term::move_cursor({
      static_cast<size_t>((pos.x - state.visible_zone.x_min) * 2),
      static_cast<size_t>(state.visible_zone.y_max - pos.y - 1)
    });
    switch (state.snapshot.map.at(pos).status)
    {
      case map::Status::TANK:
        term::output(colorify_tank(state.snapshot.map.at(pos).tank_id, "  "));
        break;
      case map::Status::BULLET:
        term::output(
          utils::color_256_bg(colorify_text(state.snapshot.map.at(pos).tank_id, state.snapshot.map.at(pos).text),
                              state.style.background));
        break;
      case map::Status::WALL:
        term::output(utils::color_256_bg("  ", state.style.wall));
        break;
      case map::Status::END:
        term::output(utils::color_256_bg("  ", state.style.background));
        break;
    }
  }

  bool check_zone_size(const map::Zone& z)
  {
    size_t h = z.y_max - z.y_min;
    size_t w = z.x_max - z.x_min;
    if (h != state.height - 2)
      return false;
    if (state.width % 2 == 0)
    {
      if (state.width != w * 2)
        return false;
    }
    else
    {
      if (state.width - 1 != w * 2)
        return false;
    }
    return true;
  }

  // [X min, X max)   [Y min, Y max)
  map::Zone get_visible_zone(size_t w, size_t h, size_t id)
  {
    const auto pos = view_id_at(id)->pos;
    map::Zone ret{};
    const int offset_x = static_cast<int>(h / 4);
    ret.x_min = pos.x - offset_x;
    ret.x_max = static_cast<int>(w / 2) + ret.x_min;

    const int offset_y = static_cast<int>(h / 2);
    ret.y_min = pos.y - offset_y;
    ret.y_max = static_cast<int>(h - 2) + ret.y_min;
    return ret;
  }

  map::Zone get_visible_zone(size_t id)
  {
    auto ret = get_visible_zone(state.width, state.height, id);
    dbg::tank_assert(check_zone_size(ret));
    return ret;
  }

  std::set<map::Pos> get_screen_changes(const map::Direction& move)
  {
    std::set<map::Pos> ret;
    // When the visible zone moves, every point in the screen doesn't move, but its corresponding pos changes.
    // so we need to do something to get the correct changes:
    // 1.  if there's no difference in the two point in the moving direction, ignore.
    // 2.  move the map's map_changes to its corresponding screen position.
    auto zone = state.visible_zone.bigger_zone(2);
    switch (move)
    {
      case map::Direction::UP:
        for (int i = state.visible_zone.x_min; i < state.visible_zone.x_max; i++)
        {
          for (int j = state.visible_zone.y_min - 1; j < state.visible_zone.y_max + 1; j++)
          {
            if (!state.snapshot.map.at(i, j + 1).is_empty() || !state.snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i, j + 1));
            }
          }
        }
        for (auto& p : state.snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x, p.y + 1});
          }
        }
        break;
      case map::Direction::DOWN:
        for (int i = state.visible_zone.x_min; i < state.visible_zone.x_max; i++)
        {
          for (int j = state.visible_zone.y_min - 1; j < state.visible_zone.y_max + 1; j++)
          {
            if (!state.snapshot.map.at(i, j - 1).is_empty() || !state.snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i, j - 1));
            }
          }
        }
        for (auto& p : state.snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x, p.y - 1});
          }
        }
        break;
      case map::Direction::LEFT:
        for (int i = state.visible_zone.x_min - 1; i < state.visible_zone.x_max + 1; i++)
        {
          for (int j = state.visible_zone.y_min; j < state.visible_zone.y_max; j++)
          {
            if (!state.snapshot.map.at(i - 1, j).is_empty() || !state.snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i - 1, j));
            }
          }
        }
        for (auto& p : state.snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x - 1, p.y});
          }
        }
        break;
      case map::Direction::RIGHT:
        for (int i = state.visible_zone.x_min - 1; i < state.visible_zone.x_max + 1; i++)
        {
          for (int j = state.visible_zone.y_min; j < state.visible_zone.y_max; j++)
          {
            if (!state.snapshot.map.at(i + 1, j).is_empty() || !state.snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i + 1, j));
            }
          }
        }
        for (auto& p : state.snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x + 1, p.y});
          }
        }
        break;
      case map::Direction::END:
        for (auto& p : state.snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(p);
          }
        }
        break;
    }
    state.snapshot.changes.clear();
    return ret;
  }

  void next_zone(const map::Direction& direction)
  {
    switch (direction)
    {
      case map::Direction::UP:
        state.visible_zone.y_max++;
        state.visible_zone.y_min++;
        break;
      case map::Direction::DOWN:
        state.visible_zone.y_max--;
        state.visible_zone.y_min--;
        break;
      case map::Direction::LEFT:
        state.visible_zone.x_max--;
        state.visible_zone.x_min--;
        break;
      case map::Direction::RIGHT:
        state.visible_zone.x_max++;
        state.visible_zone.x_min++;
        break;
      default:
        break;
    }
  }

  bool completely_out_of_zone(size_t id)
  {
    auto pos = view_id_at(id)->pos;
    return ((state.visible_zone.x_min - 1 > pos.x) || (state.visible_zone.x_max + 1 <= pos.x) ||
            (state.visible_zone.y_min - 1 > pos.y) || (state.visible_zone.y_max + 1 <= pos.y));
  }

  bool out_of_zone(size_t id)
  {
    auto pos = view_id_at(id)->pos;
    int x_offset = 5;
    int y_offset = 5;
    if (state.width < 25)
    {
      x_offset = 0;
    }
    if (state.height < 15 || state.width < 25)
    {
      x_offset = 0;
      y_offset = 0;
    }
    return ((state.visible_zone.x_min + x_offset > pos.x) || (state.visible_zone.x_max - x_offset <= pos.x) ||
            (state.visible_zone.y_min + y_offset > pos.y) || (state.visible_zone.y_max - y_offset <= pos.y));
  }

  int update_snapshot()
  {
    if (g::state.mode == g::Mode::SERVER || g::state.mode == g::Mode::NATIVE)
    {
      state.snapshot.map = extract_map(state.visible_zone.bigger_zone(10));
      state.snapshot.tanks = extract_tanks();
      state.snapshot.changes = g::state.users[g::state.id].map_changes;
      g::state.users[g::state.id].map_changes.clear();
      g::state.users[g::state.id].visible_zone = state.visible_zone;
      state.snapshot.userinfo = extract_userinfo();
      return 0;
    }
    else
    {
      int ret = online::cli.update();
      if (ret == 0)
        return 0;
      else
      {
        state.inited = false;
        return -1;
      }
    }
    return -1;
  }

  // beg, end, lineno
  std::tuple<size_t, size_t, std::string> text_display_helper(size_t display_height, size_t content_pos,
                                                              size_t content_size)
  {
    if (display_height > content_size)
      return {0, content_size, std::format("Line {}/{} (END)", content_pos + 1, content_size)};

    size_t beg = content_pos;
    size_t end = content_pos + display_height;
    if (end >= content_size)
    {
      end = content_size;
      beg = end - display_height;
      return {beg, end, std::format("Line {}/{} (END)", content_pos + 1, content_size)};
    }
    int percent = static_cast<int>(100.00 * (static_cast<double>(end) / static_cast<double>(content_size)));
    return {beg, end, std::format("Line {}/{} {}%", content_pos + 1, content_size, percent)};
  }

  void flexible_output(const std::string& left, const std::string& right)
  {
    int a = static_cast<int>(state.width) - static_cast<int>(utils::display_width_all(left, right));
    if (a > 0)
    {
      term::output(left, std::string(a, ' '), right);
    }
    else
    {
      if (auto w = utils::display_width(left); w < state.width)
      {
        term::output(left, std::string(state.width - w, ' '));
      }
      else
      {
        size_t sz = 0;
        size_t n = left.size() - (w - state.width);
        while (n > 0 && utils::display_width(left.begin(), left.begin() + n) > state.width - 1)
        {
          --n;
          while ((left[n] & 0b11000000) == 0b10000000)
            --n;
        }
        term::output(left.substr(0, n), utils::color_256_fg(">", 9));
      }
    }
  }

  std::vector<std::string> fit_into_screen(std::string_view raw, std::string indent = "")
  {
    std::vector<std::string> ret;
    static constexpr auto pad = [](std::string& line, size_t width)
    {
      if (state.width > width)
        line.insert(line.end(), state.width - width, ' ');
    };
    if (auto width = utils::display_width(raw); width > state.width)
    {
      // Get indent if not provided
      if (indent.empty())
      {
        for (auto& i : raw)
        {
          if (i == ' ')
            indent += i;
          else
            break;
        }
      }

      std::string line;
      for (size_t i = 0; i < raw.size();)
      {
        while (line.size() == indent.size() && raw[i] == ' ')
          ++i;

        line += raw[i];
        for (size_t j = i + 1; (raw[j] & 0b11000000) == 0b10000000 && j < raw.size(); ++j)
          line += raw[j];

        if (utils::display_width(line) >= state.width)
        {
          if (i > 0 && utils::display_width(line) > state.width)
          {
            int j = i - 1;
            while (j > 0 && (raw[j] & 0b11000000) == 0b10000000)
              --j;
            line.erase(line.size() - (i - j), i - j);
            i = j;
          }

          if (i + 1 < raw.size() && raw[i + 1] != ' ' && raw[i] != ' ')
          {
            int j = static_cast<int>(i);
            while (j > 0 && raw[j] != ' ')
            {
              --j;
              while (j > 0 && (raw[j] & 0b11000000) == 0b10000000)
                --j;
            }

            if (utils::display_width(raw.begin() + j, raw.begin() + i) < 5)
            {
              line.erase(line.size() - (i - j), i - j);
              pad(line, utils::display_width(line));
              ret.emplace_back(line);
              i = j;
            }
            else
            {
              pad(line, utils::display_width(line));
              ret.emplace_back(line);
            }
            line = indent;
          }
          else
          {
            pad(line, utils::display_width(line));
            ret.emplace_back(line);
            line = indent;
          }
        }
        do
          ++i;
        while (i < raw.size() && (raw[i] & 0b11000000) == 0b10000000);
      }
      if (!line.empty())
      {
        ret.emplace_back(line);
      }
    }
    else
    {
      std::string temp{raw};
      pad(temp, width);
      ret.emplace_back(temp);
    }
    return ret;
  }

  void update_help_text()
  {
    static const std::string help =
        R"(
Intro:
  In Tank, you will take control of a powerful tank in a maze, showcasing your strategic skills on the infinite map and overcome unpredictable obstacles. You can play solo or team up with friends.

Control:
  Move: WASD or direction keys
  Attack: space
  Status: 'o' or 'O'
  Notification: 'i' or 'I'
  Command: '/'

Tank:
  User's Tank:
    HP: 10000, Lethality: 100
  Auto Tank:
    HP: (11 - level) * 150, Lethality: (11 - level) * 15
    The higher level the tank is, the faster it moves.

Command:
  help [line]
    - Get this help.
    - Use 'Enter' to return game.

  notification
    - Show Notification page.
  notification read
    - Set all messages as read.
  notification clear
    - Clear all messages.
  notification clear read
    - Clear read messages.

  status
    - show Status page.

  quit
    - Quit Tank.

  pause
    - Pause.

  continue
    - Continue.

  save [filename]
    - Save the game to a file.

  load [filename]
    - load the game from a file.

    Note:
      Normally save and load can only be executed by the server itself, but you can use 'set unsafe true' to get around it. Notice that it is dangerous to let remote user access to your filesystem.

  fill [Status] [A x,y] [B x,y optional]
    - Status: [0] Empty [1] Wall
    - Fill the area from A to B as the given Status.
    - B defaults to the same as A
    - e.g.  fill 1 0 0 10 10   |   fill 1 0 0

  tp [A id] ([B id] or [B x,y])
    - Teleport A to B
    - A should be alive, and there should be space around B.
    - e.g.  tp 0 1   |  tp 0 1 1

  revive [A id optional]
    - Revive A.
    - Default to revive all tanks.

  summon [n] [level]
    - Summon n tanks with the given level.
    - e.g. summon 50 10

  kill [A id optional]
    - Kill A.
    - Default to kill all tanks.

  clear [A id optional]
    - Clear A.(only Auto Tank)
    - Default to clear all auto tanks.
  clear death
    - Clear all the died Auto Tanks
    Note:
       Clear is to delete rather than to kill, so the cleared tank can't revive.
       And the bullets of the cleared tank will also be cleared.

  set [A id] [key] [value]
    - Set A's attribute below:
      - max_hp (int): Max hp of A. This will take effect when A is revived.
      - hp (int): hp of A. This takes effect immediately but won't last when A is revived.
      - target (id, int): Auto Tank's target. Target should be alive.
      - name (string): Name of A.
  set [A id] bullet [key] [value]
      - hp (int): hp of A's bullet.
      - lethality (int): lethality of A's bullet. (negative to increase hp)
      - range (int): range of A's bullet.
      - e.g. set 0 max_hp 1000  |  set 0 bullet lethality 10
      Note:
        When a bullet hits the wall, its hp decreases by one. That means it can bounce "hp - 1" times.
  set tick [tick]
      - tick (int, milliseconds): minimum time of the game's(or server's) mainloop.
  set msgTTL [ttl]
      - TTL (int, milliseconds): a message's time to live.
  set longPressTH [threshold]
      - threshold (int, microseconds): long pressing threshold.
  set seed [seed]
      - seed (int): the game map's seed.
  set unsafe [bool]
      - true or false.
      WARNING:
        This will make the remote user accessible to your filesystem (through save, load).

  tell [A id optional] [msg]
    - Send a message to A.
    - id (int): defaults to be -1, in which case all the players will receive the message.
    - msg (string): the message's content.

  observe [A id]
    - Observe A.

  server start [port]
    - Start Tank Server.
    - port (int): the server's port.
  server stop
    - Stop Tank Server.

  connect [ip] [port] (as [id])
    - Connect to Tank Server.
    - ip (string): the server's IP.
    - port (int): the server's port.
    - id (int, optional): login as the remote user id.

  disconnect
    - Disconnect from the Server.
)";
    static auto raw_lines = help | std::views::split('\n');
    state.help_text.clear();
    for (const auto& r : raw_lines)
    {
      if (r.size() >= state.width)
      {
        auto ret = fit_into_screen(std::string_view{r});
        state.help_text.insert(state.help_text.end(), std::make_move_iterator(ret.begin()),
                               std::make_move_iterator(ret.end()));
      }
      else
        state.help_text.emplace_back(std::string_view{r});
    }
  }

  void draw()
  {
    if (g::state.suspend)
      return;
    term::hide_cursor();
    // std::lock_guard ml(g::mainloop_mtx);
    std::lock_guard dl(drawing_mtx);

    if (state.height != term::get_height() || state.width != term::get_width())
    {
      term::clear();
      state.inited = false;
      state.height = term::get_height();
      state.width = term::get_width();
      if (input::state.typing_command)
      {
        input::state.visible_line = {0, 0};
        input::edit_refresh_line_nolock();
      }
      update_help_text();
    }
    switch (g::state.page)
    {
      case g::Page::GAME:
      {
        // check zone
        if (!check_zone_size(state.visible_zone))
        {
          state.visible_zone = get_visible_zone(state.focus);
          state.inited = false;
          return;
        }

        auto move = map::Direction::END;

        if (out_of_zone(state.focus))
        {
          if (completely_out_of_zone(state.focus))
          {
            state.visible_zone = get_visible_zone(state.focus);
            state.inited = false;
            int r = update_snapshot();
            if (r != 0)
              return;
          }
          else
          {
            move = view_id_at(state.focus)->direction;
            next_zone(move);
          }
        }

        // output
        if (!state.inited)
        {
          term::move_cursor({0, 0});
          for (int j = state.visible_zone.y_max - 1; j >= state.visible_zone.y_min; j--)
          {
            for (int i = state.visible_zone.x_min; i < state.visible_zone.x_max; i++)
            {
              update_point(map::Pos(i, j));
            }
          }
          state.inited = true;
        }
        else
        {
          auto changes = get_screen_changes(move);
          for (auto& p : changes)
          {
            if (state.visible_zone.contains(p))
            {
              update_point(p);
            }
          }
        }

        auto now = std::chrono::steady_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_drawing);
        if (delta_time.count() != 0)
        {
          double curr_fps = 1.0 / (static_cast<double>(delta_time.count()) / 1000.0);
          state.fps = static_cast<int>((static_cast<double>(state.fps) + 0.01 * curr_fps) / 1.01);
        }
        state.last_drawing = now;

        // status bar
        term::move_cursor(term::TermPos(0, state.height - 2));
        auto& focus_tank = state.snapshot.tanks[state.focus];
        std::string left = colorify_text(focus_tank.id, focus_tank.name) + " HP: " + std::to_string(focus_tank.hp) +
                           "/" + std::to_string(focus_tank.max_hp) + " Pos: (" + std::to_string(focus_tank.pos.x) +
                           ", " + std::to_string(focus_tank.pos.y) + ")";
        std::string right = std::to_string(state.fps) + " fps";

        flexible_output(left, right);
      }
      break;
      case g::Page::STATUS:
      {
        if (!state.inited)
        {
          term::clear();
          state.inited = true;
        }

        size_t cursor_y = 0;

        // User Status
        term::mvoutput({state.width / 2 - 5, cursor_y++}, "User Status");
        size_t user_id_size = utils::numlen(std::prev(state.snapshot.userinfo.end())->first);
        if (user_id_size < 2)
          user_id_size = 2;

        auto ipsz_r = state.snapshot.userinfo | std::views::transform([](auto&& p) { return p.second.ip.size(); });
        size_t ip_size = (std::ranges::max)(ipsz_r);

        if (ip_size == 0)
          ip_size = 6;

        size_t status_size = 7;

        term::move_cursor({0, cursor_y++});
        term::output(std::left, std::setw(static_cast<int>(user_id_size)), "ID", "  ",
                     std::setw(static_cast<int>(ip_size)), "IP", "  ", std::setw(static_cast<int>(status_size)),
                     "Status");

        size_t i = 0;
        for (auto it = state.snapshot.userinfo.cbegin(); it != state.snapshot.userinfo.cend(); ++i, ++it)
        {
          const auto& user = it->second;
          term::move_cursor({0, cursor_y++});

          term::output(std::left, std::setw(static_cast<int>(user_id_size)), user.user_id, "  ");
          if (user.ip.empty())
          {
            term::output(std::left, std::setw(static_cast<int>(ip_size)), "Native", "  ",
                         std::setw(static_cast<int>(status_size)), "Native");
          }
          else
          {
            term::output(std::left, std::setw(static_cast<int>(ip_size)), user.ip, "  ");
            std::string status_str;
            if (user.active)
              status_str = utils::color_256_fg("Online", 2);
            else
              status_str = utils::color_256_fg("Offline", 9);

            term::output(std::left, utils::setw(status_size, status_str));
          }
        }

        // Tank Status
        term::mvoutput({state.width / 2 - 5, cursor_y++}, "Tank Status");
        size_t tank_id_size = utils::numlen(std::prev(state.snapshot.tanks.end())->first);

        if (tank_id_size < 2)
          tank_id_size = 2;
        auto namesz_r = state.snapshot.tanks | std::views::transform([](auto&& a) { return a.second.name.size(); });
        size_t name_size = (std::ranges::max)(namesz_r);

        auto get_pos_size = [](const map::Pos& p)
        {
          // (x, y)
          return std::to_string(p.x).size() + std::to_string(p.y).size() + 4;
        };

        auto possz_r = state.snapshot.tanks |
                       std::views::transform([&get_pos_size](auto&& a) { return get_pos_size(a.second.pos); });
        size_t pos_size = (std::ranges::max)(possz_r);

        auto hpsz_r = state.snapshot.tanks | std::views::transform([](auto&& t) { return t.second.hp; });
        size_t hp_size = utils::numlen((std::ranges::max)(hpsz_r));


        auto atksz_r = state.snapshot.tanks | std::views::transform([](auto&& t) { return t.second.bullet_lethality; });
        size_t atk_size = utils::numlen((std::ranges::max)(atksz_r));

        if (atk_size < 3)
          atk_size = 3;
        size_t gap_size = 3;
        size_t target_size = state.width - tank_id_size - name_size - pos_size - hp_size - atk_size - gap_size - 12;
        if (target_size < 6)
          target_size = 6;
        term::move_cursor({0, cursor_y++});
        term::output(std::left, std::setw(static_cast<int>(tank_id_size)), "ID", "  ",
                     std::setw(static_cast<int>(name_size)), "Name", "  ", std::setw(static_cast<int>(pos_size)), "Pos",
                     "  ", std::setw(static_cast<int>(hp_size)), "HP", "  ", std::setw(static_cast<int>(atk_size)),
                     "ATK", "  ", std::setw(static_cast<int>(gap_size)), "Gap", "  ",
                     std::setw(static_cast<int>(target_size)), "Target", "  ");

        if (state.status_pos >= state.snapshot.tanks.size())
          state.status_pos = 0;

        size_t display_height = state.height - state.snapshot.userinfo.size() - 6;

        auto [beg, end, lineno] = text_display_helper(display_height, state.status_pos, state.snapshot.tanks.size());

        state.status_pos = beg;

        size_t j = 0;
        for (auto it = state.snapshot.tanks.cbegin(); it != state.snapshot.tanks.cend(); ++j, ++it)
        {
          if (j >= beg && j < end)
          {
            const auto& tank = it->second;
            term::move_cursor({0, cursor_y++});

            std::string pos_str = '(' + std::to_string(tank.pos.x) + ',' +
                                  std::string(pos_size - get_pos_size(tank.pos) + 1, ' ') + std::to_string(tank.pos.y) +
                                  ')';

            std::string tank_str = colorify_text(tank.id, tank.name);
            term::output(std::left, std::setw(static_cast<int>(tank_id_size)), tank.id, "  ",
                         utils::setw(name_size, tank_str), "  ", std::setw(static_cast<int>(pos_size)), pos_str, "  ",
                         std::setw(static_cast<int>(hp_size)), tank.hp, "  ", std::setw(static_cast<int>(atk_size)),
                         tank.bullet_lethality, "  ");
            if (tank.is_auto)
            {
              term::output(std::left, std::setw(static_cast<int>(gap_size)), tank.gap, "  ");
              if (tank.has_good_target)
              {
                std::string target_str = colorify_text(tank.target_id, state.snapshot.tanks[tank.target_id].name);
                target_str += "(" + std::to_string(tank.target_id) + ")";

                term::output(std::left, utils::setw(target_size, target_str));
              }
              else
                term::output(std::left, std::setw(static_cast<int>(target_size)), "-");
            }
            else
            {
              term::output(std::left, std::setw(static_cast<int>(gap_size)), "-", "  ",
                           std::setw(static_cast<int>(target_size)), "-");
            }
          }
        }
        term::mvoutput({(state.width - lineno.size()) / 2, state.height - 2}, "\x1b[2K", lineno);
      }
      break;
      case g::Page::MAIN:
      {
        if (!state.inited)
        {
          constexpr std::string_view tank = R"(
 _____  _    _   _ _  __
|_   _|/ \  | \ | | |/ /
  | | / _ \ |  \| | ' /
  | |/ ___ \| |\  | . \
  |_/_/   \_\_| \_|_|\_\
)";
          size_t x = state.width / 2 - 12;
          size_t y = 2;
          if (state.width > 24)
          {
            auto&& splitted = tank | std::views::split('\n');
            for (auto&& r : splitted)
              term::mvoutput({x, y++}, std::string_view{r});
          }
          else
          {
            x = state.width / 2 - 2;
            term::mvoutput({x, y++}, "TANK");
          }

          term::mvoutput({x + 5, y + 3}, ">>> Enter <<<");
          term::mvoutput({x + 1, y + 4}, "Type '/help' to get help.");
          state.inited = true;
        }
      }
      break;
      case g::Page::HELP:
      {
        if (state.help_text.empty())
          update_help_text();
        if (!state.inited)
        {
          term::clear();
          size_t cursor_y = 0;
          term::mvoutput({state.width / 2 - 4, cursor_y++}, "Tank Help");

          if (state.help_pos >= state.help_text.size())
            state.help_pos = 0;

          auto [beg, end, lineno] = text_display_helper(state.height - 3, state.help_pos, state.help_text.size());

          state.help_pos = beg;

          for (size_t i = beg; i < end; ++i)
            term::mvoutput({0, cursor_y++}, state.help_text[i]);

          term::mvoutput({(state.width - lineno.size()) / 2, state.height - 2}, "\x1b[2K", lineno);
          state.inited = true;
        }
      }
      break;
      case g::Page::NOTIFICATION:
      {
        auto& msgs = g::state.users[g::state.id].messages;
        const auto add_notification_text = [](const msg::Message& msg) -> size_t
        {
          auto time = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(std::chrono::seconds(msg.time)));
          char buf[16];
#ifdef _WIN32
          tm now{};
          localtime_s(&now, &time);
          std::strftime(buf, sizeof(buf), "[%H:%M:%S]", &now);
#else
          std::strftime(buf, sizeof(buf), "[%H:%M:%S]", std::localtime(&time));
#endif
          std::string time_str(buf);

          std::string raw;

          raw += time_str;
          if (msg.from != -1)
            raw += std::to_string(msg.from) + ": ";

          size_t indent_size = raw.size();

          raw += msg.content;

          auto msg_text = fit_into_screen(raw, std::string(indent_size, ' '));
          auto ret = msg_text.size();
          state.notification_text.insert(state.notification_text.begin(),
                                         std::make_move_iterator(msg_text.begin()),
                                         std::make_move_iterator(msg_text.end()));
          return ret;
        };

        static constexpr auto output_notification = []
        {
          size_t cursor_y = 0;
          term::mvoutput({state.width / 2 - 6, cursor_y++}, "Notification");

          if (state.notification_pos >= state.notification_text.size())
            state.notification_pos = 0;

          auto [beg, end, lineno] =
              text_display_helper(state.height - 3, state.notification_pos, state.notification_text.size());

          state.notification_pos = beg;

          for (size_t i = beg; i < end; ++i)
            term::mvoutput({0, cursor_y++}, "\x1b[2K", state.notification_text[i]);

          term::mvoutput({(state.width - lineno.size()) / 2, state.height - 2}, "\x1b[2K", lineno);
        };

        if (!state.inited)
        {
          term::clear();
          state.notification_text.clear();
          for (const auto& msg : msgs)
            add_notification_text(msg);
          output_notification();
          for (auto& r : msgs)
          {
            if (!r.read)
              r.read = true;
          }
          state.inited = true;
        }
        else
        {
          bool updated = false;
          for (auto& r : msgs)
          {
            if (!r.read)
            {
              r.read = true;
              state.notification_pos += add_notification_text(r);
              updated = true;
            }
          }
          if (updated)
            output_notification();
        }
      }
      break;
    }

    static const auto show_info = []
    {
      std::string right = "Tank Version 0.2.1 (Compile: " + std::string(__DATE__) + ")";
      std::string left;
      if (g::state.mode == g::Mode::NATIVE)
        left += "Native Mode";
      else if (g::state.mode == g::Mode::SERVER)
      {
        left += "Server Mode | Port: " + std::to_string(online::svr.get_port()) + " | ";
        size_t active_users =
            std::ranges::count_if(g::state.users | std::views::values, [](auto&& u) { return u.active; });

        left += "User: " + std::to_string(active_users) + "/" + std::to_string(g::state.users.size());
      }
      else if (g::state.mode == g::Mode::CLIENT)
      {
        left += "Client Mode | ";
        left += "ID: " + std::to_string(g::state.id) + " | Connected to " + online::cli.get_host() + ":" +
            std::to_string(online::cli.get_port()) + " | ";
        if (online::state.delay < 50)
          left += utils::color_256_fg(std::to_string(online::state.delay) + " ms", 2);
        else if (online::state.delay < 100)
          left += utils::color_256_fg(std::to_string(online::state.delay) + " ms", 11);
        else
          left += utils::color_256_fg(std::to_string(online::state.delay) + " ms", 9);
      }
      term::output("\x1b[2K");
      flexible_output(left, right);
    };

    // command
    if (input::state.typing_command)
    {
      input::update_cursor_nolock();
      term::show_cursor();
    }
    else if (!dbg::message.empty())
    {
      term::move_cursor(term::TermPos(0, state.height - 1));
      flexible_output(dbg::message, "");
    }
    else
    {
      term::move_cursor(term::TermPos(0, state.height - 1));
      if (g::state.users[g::state.id].messages.empty())
        show_info();
      else
      {
        auto now = std::chrono::steady_clock::now();
        auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_message_displayed);
        if (d2 > cfg::config.msg_ttl)
        {
          auto msg = bc::read_message(g::state.id);
          if (msg.has_value())
          {
            std::string str = ((msg->from == bc::from_system) ? "" : std::to_string(msg->from) + ": ") + msg->content;
            std::erase_if(str, [](auto&& ch) { return ch == '\n' || ch == '\r'; });
            if (auto w = utils::display_width(str); w > state.width)
            {
              size_t n = str.size() - (w - state.width + 1);
              while (n > 0 && utils::display_width(str.begin(), str.begin() + n) > state.width - 1)
              {
                --n;
                while ((str[n] & 0b11000000) == 0b10000000)
                  --n;
              }
              term::output("\x1b[2K", str.substr(0, n), utils::color_256_fg(">", 9));
            }
            else
              term::output("\x1b[2K", str);

            state.last_message_displayed = now;
          }
          else
            show_info();
        }
      }
    }
    term::flush();
  }
} // namespace czh::draw
