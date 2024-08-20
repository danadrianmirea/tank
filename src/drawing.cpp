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
#include "tank/game.h"
#include "tank/bullet.h"
#include "tank/utils.h"
#include "tank/term.h"
#include "tank/drawing.h"

#include <cstring>

#include "tank/globals.h"

#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>

namespace czh::g
{
  bool output_inited = false;
  std::size_t screen_height = 0;
  std::size_t screen_width = 0;
  size_t tank_focus = 0;
  g::Page curr_page = g::Page::MAIN;
  size_t help_pos = 0;
  size_t status_pos = 0;
  size_t notification_pos = 0;
  std::vector<std::string> help_text;
  map::Zone visible_zone = {-128, 128, -128, 128};
  drawing::Snapshot snapshot{};
  int fps = 60;
  const drawing::PointView empty_point_view{.status = map::Status::END, .tank_id = -1, .text = ""};
  const drawing::PointView wall_point_view{.status = map::Status::WALL, .tank_id = -1, .text = ""};
  std::chrono::steady_clock::time_point last_drawing = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last_message_displayed = std::chrono::steady_clock::now();
  drawing::Style style{
    .background = 15, .wall = 9, .tanks =
    {
      10, 3, 4, 5, 6, 11, 12, 13, 14, 57, 100, 214
    }
  };
  std::mutex drawing_mtx;
}

namespace czh::drawing
{
  const PointView& generate(const map::Pos& i, size_t seed)
  {
    if (map::generate(i, seed).has(map::Status::WALL))
    {
      return g::wall_point_view;
    }
    return g::empty_point_view;
  }

  const PointView& generate(int x, int y, size_t seed)
  {
    if (map::generate(x, y, seed).has(map::Status::WALL))
    {
      return g::wall_point_view;
    }
    return g::empty_point_view;
  }

  bool PointView::is_empty() const
  {
    return status == map::Status::END;
  }

  const PointView& MapView::at(int x, int y) const
  {
    return at(map::Pos(x, y));
  }

  const PointView& MapView::at(const map::Pos& i) const
  {
    if (view.contains(i))
    {
      return view.at(i);
    }
    return drawing::generate(i, seed);
  }

  bool MapView::is_empty() const
  {
    return view.empty();
  }

  PointView extract_point(const map::Pos& p)
  {
    if (g::game_map.has(map::Status::TANK, p))
    {
      return {
        .status = map::Status::TANK,
        .tank_id = static_cast<int>(g::game_map.at(p).get_tank()->get_id()),
        .text = ""
      };
    }
    else if (g::game_map.has(map::Status::BULLET, p))
    {
      return {
        .status = map::Status::BULLET,
        .tank_id = static_cast<int>(g::game_map.at(p).get_bullets()[0]->get_tank()),
        .text = g::game_map.at(p).get_bullets()[0]->get_text()
      };
    }
    else if (g::game_map.has(map::Status::WALL, p))
    {
      return {
        .status = map::Status::WALL,
        .tank_id = -1,
        .text = ""
      };
    }
    else
    {
      return {
        .status = map::Status::END,
        .tank_id = -1,
        .text = ""
      };
    }
    return {};
  }

  MapView extract_map(const map::Zone& zone)
  {
    MapView ret;
    ret.seed = g::seed;
    for (int i = zone.x_min; i < zone.x_max; ++i)
    {
      for (int j = zone.y_min; j < zone.y_max; ++j)
      {
        if (!g::game_map.at(i, j).is_generated())
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
    for (auto& r : g::tanks)
    {
      auto tv = TankView{
        .info = r.second->get_info(),
        .hp = r.second->get_hp(),
        .pos = r.second->get_pos(),
        .direction = r.second->get_direction(),
        .is_auto = r.second->is_auto(),
        .is_alive = r.second->is_alive(),
        .has_good_target = false,
        .target_id = 0
      };
      if (r.second->is_auto())
      {
        auto at = dynamic_cast<tank::AutoTank*>(r.second);
        if (at->is_target_good())
        {
          tv.has_good_target = true;
          tv.target_id = at->get_target_id();
        }
      }
      view[r.first] = tv;
    }
    return view;
  }

  std::optional<TankView> view_id_at(size_t id)
  {
    auto it = g::snapshot.tanks.find(id);
    if (it == g::snapshot.tanks.end()) return std::nullopt;
    return it->second;
  }

  std::map<size_t, UserView> extract_userinfo()
  {
    std::map<size_t, UserView> view;
    for (auto& r : g::userdata)
    {
      view[r.first] =
          UserView{
            .user_id = r.second.user_id,
            .ip = r.second.ip,
            .active = r.second.active
          };
    }
    return view;
  }

  std::string colorify_text(size_t id, const std::string& str)
  {
    int color;
    if (id == 0)
    {
      color = g::style.tanks[0];
    }
    else
    {
      color = g::style.tanks[id % g::style.tanks.size()];
    }
    return utils::color_256_fg(str, color);
  }

  std::string colorify_tank(size_t id, const std::string& str)
  {
    int color;
    if (id == 0)
    {
      color = g::style.tanks[0];
    }
    else
    {
      color = g::style.tanks[id % g::style.tanks.size()];
    }
    return utils::color_256_bg(str, color);
  }

  void update_point(const map::Pos& pos)
  {
    term::move_cursor({
      static_cast<size_t>((pos.x - g::visible_zone.x_min) * 2),
      static_cast<size_t>(g::visible_zone.y_max - pos.y - 1)
    });
    switch (g::snapshot.map.at(pos).status)
    {
      case map::Status::TANK:
        term::output(colorify_tank(g::snapshot.map.at(pos).tank_id, "  "));
        break;
      case map::Status::BULLET:
        term::output(utils::color_256_bg(colorify_text(g::snapshot.map.at(pos).tank_id,
                                                       g::snapshot.map.at(pos).text), g::style.background));
        break;
      case map::Status::WALL:
        term::output(utils::color_256_bg("  ", g::style.wall));
        break;
      case map::Status::END:
        term::output(utils::color_256_bg("  ", g::style.background));
        break;
    }
  }

  bool check_zone_size(const map::Zone& z)
  {
    size_t h = z.y_max - z.y_min;
    size_t w = z.x_max - z.x_min;
    if (h != g::screen_height - 2) return false;
    if (g::screen_width % 2 == 0)
    {
      if (g::screen_width != w * 2) return false;
    }
    else
    {
      if (g::screen_width - 1 != w * 2) return false;
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
    auto ret = get_visible_zone(g::screen_width, g::screen_height, id);
    utils::tank_assert(check_zone_size(ret));
    return ret;
  }

  std::set<map::Pos> get_screen_changes(const map::Direction& move)
  {
    std::set<map::Pos> ret;
    // When the visible zone moves, every point in the screen doesn't move, but its corresponding pos changes.
    // so we need to do something to get the correct changes:
    // 1.  if there's no difference in the two point in the moving direction, ignore.
    // 2.  move the map's map_changes to its corresponding screen position.
    auto zone = g::visible_zone.bigger_zone(2);
    switch (move)
    {
      case map::Direction::UP:
        for (int i = g::visible_zone.x_min; i < g::visible_zone.x_max; i++)
        {
          for (int j = g::visible_zone.y_min - 1; j < g::visible_zone.y_max + 1; j++)
          {
            if (!g::snapshot.map.at(i, j + 1).is_empty() || !g::snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i, j + 1));
            }
          }
        }
        for (auto& p : g::snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x, p.y + 1});
          }
        }
        break;
      case map::Direction::DOWN:
        for (int i = g::visible_zone.x_min; i < g::visible_zone.x_max; i++)
        {
          for (int j = g::visible_zone.y_min - 1; j < g::visible_zone.y_max + 1; j++)
          {
            if (!g::snapshot.map.at(i, j - 1).is_empty() || !g::snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i, j - 1));
            }
          }
        }
        for (auto& p : g::snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x, p.y - 1});
          }
        }
        break;
      case map::Direction::LEFT:
        for (int i = g::visible_zone.x_min - 1; i < g::visible_zone.x_max + 1; i++)
        {
          for (int j = g::visible_zone.y_min; j < g::visible_zone.y_max; j++)
          {
            if (!g::snapshot.map.at(i - 1, j).is_empty() || !g::snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i - 1, j));
            }
          }
        }
        for (auto& p : g::snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x - 1, p.y});
          }
        }
        break;
      case map::Direction::RIGHT:
        for (int i = g::visible_zone.x_min - 1; i < g::visible_zone.x_max + 1; i++)
        {
          for (int j = g::visible_zone.y_min; j < g::visible_zone.y_max; j++)
          {
            if (!g::snapshot.map.at(i + 1, j).is_empty() || !g::snapshot.map.at(i, j).is_empty())
            {
              ret.insert(map::Pos(i, j));
              ret.insert(map::Pos(i + 1, j));
            }
          }
        }
        for (auto& p : g::snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(map::Pos{p.x + 1, p.y});
          }
        }
        break;
      case map::Direction::END:
        for (auto& p : g::snapshot.changes)
        {
          if (zone.contains(p))
          {
            ret.insert(p);
          }
        }
        break;
    }
    g::snapshot.changes.clear();
    return ret;
  }

  void next_zone(const map::Direction& direction)
  {
    switch (direction)
    {
      case map::Direction::UP:
        g::visible_zone.y_max++;
        g::visible_zone.y_min++;
        break;
      case map::Direction::DOWN:
        g::visible_zone.y_max--;
        g::visible_zone.y_min--;
        break;
      case map::Direction::LEFT:
        g::visible_zone.x_max--;
        g::visible_zone.x_min--;
        break;
      case map::Direction::RIGHT:
        g::visible_zone.x_max++;
        g::visible_zone.x_min++;
        break;
      default:
        break;
    }
  }

  bool completely_out_of_zone(size_t id)
  {
    auto pos = view_id_at(id)->pos;
    return
    ((g::visible_zone.x_min - 1 > pos.x)
     || (g::visible_zone.x_max + 1 <= pos.x)
     || (g::visible_zone.y_min - 1 > pos.y)
     || (g::visible_zone.y_max + 1 <= pos.y));
  }

  bool out_of_zone(size_t id)
  {
    auto pos = view_id_at(id)->pos;
    int x_offset = 5;
    int y_offset = 5;
    if (g::screen_width < 25)
    {
      x_offset = 0;
    }
    if (g::screen_height < 15 || g::screen_width < 25)
    {
      x_offset = 0;
      y_offset = 0;
    }
    return
    ((g::visible_zone.x_min + x_offset > pos.x)
     || (g::visible_zone.x_max - x_offset <= pos.x)
     || (g::visible_zone.y_min + y_offset > pos.y)
     || (g::visible_zone.y_max - y_offset <= pos.y));
  }

  int update_snapshot()
  {
    if (g::game_mode == g::GameMode::SERVER || g::game_mode == g::GameMode::NATIVE)
    {
      g::snapshot.map = extract_map(g::visible_zone.bigger_zone(10));
      g::snapshot.tanks = extract_tanks();
      g::snapshot.changes = g::userdata[g::user_id].map_changes;
      g::userdata[g::user_id].map_changes.clear();
      g::snapshot.userinfo = extract_userinfo();
      return 0;
    }
    else
    {
      int ret = g::online_client.update();
      if (ret == 0)
      {
        g::client_failed_attempts = 0;
        return 0;
      }
      else
      {
        g::client_failed_attempts++;
        g::output_inited = false;
        return -1;
      }
    }
    return -1;
  }

  std::pair<size_t, size_t> get_display_section(size_t display_height, size_t content_pos, size_t content_size)
  {
    if (display_height > content_size)
      return {0, content_size};
    size_t offset = display_height / 2;
    size_t beg;
    if (content_pos < offset)
      beg = 0;
    else
      beg = content_pos - offset;
    size_t end = beg + display_height;
    if (end > content_size)
    {
      end = content_size;
      beg = content_size - display_height;
    }
    return {beg, end};
  }

  void flexible_output(const std::string& left, const std::string& right)
  {
    int a = static_cast<int>(g::screen_width) - static_cast<int>(utils::display_width(left, right));
    if (a > 0)
    {
      term::output(left, std::string(a, ' '), right);
    }
    else
    {
      int b = static_cast<int>(g::screen_width) - static_cast<int>(utils::display_width(left));
      if (b > 0)
      {
        term::output(left, std::string(b, ' '));
      }
      else
      {
        term::output(left.substr(0, static_cast<int>(left.size()) + b));
      }
    }
  }

  std::vector<std::string> fit_into_screen(const std::string& raw, std::string indent = "")
  {
    std::vector<std::string> ret;
    if (auto width = utils::display_width(raw); width > g::screen_width)
    {
      // Get indent if not provided
      if (indent.empty())
      {
        for (auto& i : raw)
        {
          if (std::isspace(i))
            indent += i;
          else
            break;
        }
      }

      std::string line;
      for (size_t i = 0; i < raw.size(); ++i)
      {
        if (line.size() == indent.size() && std::isspace(raw[i]))
          continue;
        line += raw[i];
        if (utils::display_width(line) == g::screen_width)
        {
          if (i + 1 < raw.size() && std::isalpha(raw[i + 1]) && std::isalpha(raw[i]))
          {
            int j = static_cast<int>(i);
            for (; j >= 0 && !std::isspace(raw[j]); --j)
            {
              line.pop_back();
            }
            line.insert(line.end(), i - j, ' ');
            ret.emplace_back(line);
            line = indent;
            i = j;
          }
          else
          {
            ret.emplace_back(line);
            line = indent;
          }
        }
      }
      if (!line.empty())
      {
        ret.emplace_back(line);
      }
    }
    else
    {
      std::string temp = raw;
      temp.insert(temp.end(), g::screen_width - width, ' ');
      ret.emplace_back(temp);
    }
    return ret;
  }

  void draw()
  {
    if (g::game_suspend) return;
    term::hide_cursor();
    std::lock_guard<std::mutex> l1(g::mainloop_mtx);
    std::lock_guard<std::mutex> l2(g::drawing_mtx);

    static const std::string shadow_beg = "\x1b[48;5;8m";
    static const std::string shadow_end = "\x1b[49m";
    if (g::screen_height != term::get_height() || g::screen_width != term::get_width())
    {
      term::clear();
      g::output_inited = false;
      g::screen_height = term::get_height();
      g::screen_width = term::get_width();
      if (g::typing_command)
      {
        g::visible_cmd_line = {0, 0};
        input::edit_refresh_line_nolock();
      }

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
      static auto raw_lines = utils::split<std::vector<std::string_view> >(help, "\n");
      g::help_text.clear();
      for (auto& r : raw_lines)
      {
        if (r.size() >= g::screen_width)
        {
          auto ret = fit_into_screen(std::string{r});
          g::help_text.insert(g::help_text.end(),
                              std::make_move_iterator(ret.begin()), std::make_move_iterator(ret.end()));
        }
        else
          g::help_text.emplace_back(r);
      }
    }
    switch (g::curr_page)
    {
      case g::Page::GAME:
      {
        // check zone
        if (!check_zone_size(g::visible_zone))
        {
          g::visible_zone = get_visible_zone(g::tank_focus);
          g::output_inited = false;
          return;
        }

        auto move = map::Direction::END;

        if (out_of_zone(g::tank_focus))
        {
          if (completely_out_of_zone(g::tank_focus))
          {
            g::visible_zone = get_visible_zone(g::tank_focus);
            g::output_inited = false;
            int r = update_snapshot();
            if (r != 0) return;
          }
          else
          {
            move = view_id_at(g::tank_focus)->direction;
            next_zone(move);
          }
        }

        // output
        if (!g::output_inited)
        {
          term::move_cursor({0, 0});
          for (int j = g::visible_zone.y_max - 1; j >= g::visible_zone.y_min; j--)
          {
            for (int i = g::visible_zone.x_min; i < g::visible_zone.x_max; i++)
            {
              update_point(map::Pos(i, j));
            }
          }
          g::output_inited = true;
        }
        else
        {
          auto changes = get_screen_changes(move);
          for (auto& p : changes)
          {
            if (g::visible_zone.contains(p))
            {
              update_point(p);
            }
          }
        }

        auto now = std::chrono::steady_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - g::last_drawing);
        if (delta_time.count() != 0)
        {
          double curr_fps = 1.0 / (static_cast<double>(delta_time.count()) / 1000.0);
          g::fps = static_cast<int>((static_cast<double>(g::fps) + 0.01 * curr_fps) / 1.01);
        }
        g::last_drawing = now;

        // status bar
        term::move_cursor(term::TermPos(0, g::screen_height - 2));
        auto& focus_tank = g::snapshot.tanks[g::tank_focus];
        std::string left = colorify_text(focus_tank.info.id, focus_tank.info.name)
                           + " HP: " + std::to_string(focus_tank.hp) + "/" + std::to_string(focus_tank.info.max_hp)
                           + " Pos: (" + std::to_string(focus_tank.pos.x) + ", " + std::to_string(focus_tank.pos.y) +
                           ")";
        std::string right = std::to_string(g::fps) + " fps";

        flexible_output(left, right);
      }
      break;
      case g::Page::STATUS:
      {
        if (!g::output_inited)
        {
          term::clear();
          g::output_inited = true;
        }

        size_t cursor_y = 0;

        // User Status
        term::mvoutput({g::screen_width / 2 - 5, cursor_y++}, "User Status");
        size_t user_id_size = std::to_string(std::max_element(g::snapshot.userinfo.begin(),
                                                              g::snapshot.userinfo.end(),
                                                              [](auto&& a, auto&& b)
                                                              {
                                                                return a.second.user_id <
                                                                       b.second.user_id;
                                                              })->second.user_id).size();
        if (user_id_size < 2) user_id_size = 2;

        size_t ip_size = std::max_element(g::snapshot.userinfo.begin(), g::snapshot.userinfo.end(),
                                          [](auto&& a, auto&& b)
                                          {
                                            return a.second.ip.size() <
                                                   b.second.ip.size();
                                          })->second.ip.size();
        if (ip_size == 0) ip_size = 6;

        size_t status_size = 7;

        term::move_cursor({0, cursor_y++});
        term::output(std::left,
                     std::setw(static_cast<int>(user_id_size)), "ID", "  ",
                     std::setw(static_cast<int>(ip_size)), "IP", "  ",
                     std::setw(static_cast<int>(status_size)), "Status");

        size_t i = 0;
        auto is_active_user_item = [&i] { return i == g::status_pos; };
        for (; i < g::snapshot.userinfo.size(); ++i)
        {
          auto user = g::snapshot.userinfo[i];
          term::move_cursor({0, cursor_y++});
          if (is_active_user_item())
            term::output(shadow_beg);

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

            if (is_active_user_item())
              status_str += shadow_beg;

            term::output(std::left, utils::setw(status_size, status_str));
          }

          if (is_active_user_item())
            term::output(shadow_end);
        }

        // Tank Status
        term::mvoutput({g::screen_width / 2 - 5, cursor_y++}, "Tank Status");
        size_t tank_id_size = std::to_string(std::max_element(g::snapshot.tanks.begin(), g::snapshot.tanks.end(),
                                                              [](auto&& a, auto&& b)
                                                              {
                                                                return a.second.info.id <
                                                                       b.second.info.id;
                                                              })->second.info.id).size();
        if (tank_id_size < 2) tank_id_size = 2;
        size_t name_size = std::max_element(g::snapshot.tanks.begin(), g::snapshot.tanks.end(),
                                            [](auto&& a, auto&& b)
                                            {
                                              return a.second.info.name.size() <
                                                     b.second.info.name.size();
                                            })->second.info.name.size();
        auto get_pos_size = [](const map::Pos& p)
        {
          // (x, y)
          return std::to_string(p.x).size() + std::to_string(p.y).size() + 4;
        };
        size_t pos_size = get_pos_size(std::max_element(g::snapshot.tanks.begin(), g::snapshot.tanks.end(),
                                                        [&get_pos_size](auto&& a, auto&& b)
                                                        {
                                                          return get_pos_size(a.second.pos) <
                                                                 get_pos_size(b.second.pos);
                                                        })->second.pos);
        size_t hp_size = std::to_string(std::max_element(g::snapshot.tanks.begin(), g::snapshot.tanks.end(),
                                                         [](auto&& a, auto&& b)
                                                         {
                                                           return a.second.hp <
                                                                  b.second.hp;
                                                         })->second.hp).size();
        size_t atk_size = std::to_string(std::max_element(g::snapshot.tanks.begin(), g::snapshot.tanks.end(),
                                                          [](auto&& a, auto&& b)
                                                          {
                                                            return
                                                                a.second.info.bullet.lethality <
                                                                b.second.info.bullet.lethality;
                                                          })->second.info.bullet.lethality).size();
        if (atk_size < 3) atk_size = 3;
        size_t gap_size = 3;
        size_t target_size = g::screen_width - tank_id_size - name_size - pos_size - hp_size - atk_size - gap_size - 12;
        if (target_size < 6) target_size = 6;
        term::move_cursor({0, cursor_y++});
        term::output(std::left,
                     std::setw(static_cast<int>(tank_id_size)), "ID", "  ",
                     std::setw(static_cast<int>(name_size)), "Name", "  ",
                     std::setw(static_cast<int>(pos_size)), "Pos", "  ",
                     std::setw(static_cast<int>(hp_size)), "HP", "  ",
                     std::setw(static_cast<int>(atk_size)), "ATK", "  ",
                     std::setw(static_cast<int>(gap_size)), "Gap", "  ",
                     std::setw(static_cast<int>(target_size)), "Target", "  ");

        if (g::status_pos >= g::snapshot.tanks.size() + g::snapshot.userinfo.size())
          g::status_pos = 0;

        size_t display_height = g::screen_height - g::snapshot.userinfo.size() - 6;

        size_t content_pos = 0;
        if (g::status_pos >= g::snapshot.userinfo.size())
          content_pos = g::status_pos - g::snapshot.userinfo.size();

        auto [beg, end] = get_display_section(display_height,
                                              content_pos,
                                              g::snapshot.tanks.size());
        size_t j = 0;
        auto is_active_tank_item = [&j]
        {
          if (g::status_pos < g::snapshot.userinfo.size())
            return false;
          return j == g::status_pos - g::snapshot.userinfo.size();
        };
        for (; j < g::snapshot.tanks.size(); ++j)
        {
          if (j >= beg && j < end)
          {
            const auto& tank = g::snapshot.tanks[j];
            term::move_cursor({0, cursor_y++});
            if (is_active_tank_item())
            {
              term::output(shadow_beg);
            }
            std::string pos_str = utils::contact('(', tank.pos.x,
                                                 ',', std::string(pos_size - get_pos_size(tank.pos) + 1, ' '),
                                                 tank.pos.y, ')');

            std::string tank_str = colorify_text(tank.info.id, tank.info.name);
            if (is_active_tank_item())
              tank_str += shadow_beg;
            term::output(std::left,
                         std::setw(static_cast<int>(tank_id_size)), tank.info.id, "  ",
                         utils::setw(name_size, tank_str), "  ",
                         std::setw(static_cast<int>(pos_size)), pos_str, "  ",
                         std::setw(static_cast<int>(hp_size)), tank.hp, "  ",
                         std::setw(static_cast<int>(atk_size)), tank.info.bullet.lethality, "  ");
            if (tank.is_auto)
            {
              term::output(std::left, std::setw(static_cast<int>(gap_size)), tank.info.gap, "  ");
              if (tank.has_good_target)
              {
                std::string target_str = colorify_text(tank.target_id, g::snapshot.tanks[tank.target_id].info.name);
                if (is_active_tank_item())
                  target_str += shadow_beg + "(" + std::to_string(tank.target_id) + ")";
                else
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
            if (is_active_tank_item())
            {
              term::output(shadow_end);
            }
          }
        }
        term::mvoutput({g::screen_width / 2 - 3, g::screen_height - 2}, "Line ", g::status_pos + 1);
      }
      break;
      case g::Page::MAIN:
      {
        if (!g::output_inited)
        {
          constexpr std::string_view tank = R"(
 _____  _    _   _ _  __
|_   _|/ \  | \ | | |/ /
  | | / _ \ |  \| | ' /
  | |/ ___ \| |\  | . \
  |_/_/   \_\_| \_|_|\_\
)";
          size_t x = g::screen_width / 2 - 12;
          size_t y = 2;
          if (g::screen_width > 24)
          {
            auto splitted = utils::split<std::vector<std::string_view> >(tank, "\n");
            for (auto& r : splitted)
            {
              term::mvoutput({x, y++}, r);
            }
          }
          else
          {
            x = g::screen_width / 2 - 2;
            term::mvoutput({x, y++}, "TANK");
          }

          term::mvoutput({x + 5, y + 3}, ">>> Enter <<<");
          term::mvoutput({x + 1, y + 4}, "Type '/help' to get help.");
          g::output_inited = true;
        }
      }
      break;
      case g::Page::HELP:
      {
        if (!g::output_inited)
        {
          term::clear();
          size_t cursor_y = 0;
          term::mvoutput({g::screen_width / 2 - 4, cursor_y++}, "Tank Help");

          if (g::help_pos >= g::help_text.size()) g::help_pos = 0;

          auto [beg, end] = get_display_section(g::screen_height - 3,
                                                g::help_pos, g::help_text.size());

          for (size_t i = beg; i < end; ++i)
          {
            if (i == g::help_pos)
            {
              term::mvoutput({0, cursor_y++}, shadow_beg, g::help_text[i],
                             std::string(g::screen_width - g::help_text[i].size(), ' '), shadow_end);
            }
            else
            {
              term::mvoutput({0, cursor_y++}, g::help_text[i]);
            }
          }
          term::mvoutput({g::screen_width / 2 - 3, g::screen_height - 2}, "Line ", g::help_pos + 1);
          g::output_inited = true;
        }
      }
      break;
      case g::Page::NOTIFICATION:
      {
        if (!g::output_inited)
        {
          term::clear();
          g::output_inited = true;
        }
        size_t cursor_y = 0;
        term::mvoutput({g::screen_width / 2 - 6, cursor_y++}, "Notification");

        const auto& msgs = g::userdata[g::user_id].messages;
        if (g::notification_pos >= msgs.size()) g::notification_pos = 0;

        size_t content_size = 0;
        for (auto& r : msgs)
          content_size += r.content.size() / g::screen_width + 1;
        auto [beg, end] = get_display_section(g::screen_height - 3,
                                              g::notification_pos, content_size);

        size_t line_pos = 0;
        for (size_t i = 0; i < msgs.size(); ++i)
        {
          const auto& msg = msgs[msgs.size() - i - 1];
          if (line_pos >= beg && line_pos < end)
          {
            auto time = std::chrono::system_clock::to_time_t(
              std::chrono::system_clock::time_point(std::chrono::seconds(msg.time)));
            char buf[16];
            std::strftime(buf, sizeof(buf), "[%H:%M:%S]", std::localtime(&time));
            std::string time_str(buf);

            std::string raw;
            if (!msg.read)
              raw += utils::color_256_fg("NEW> ", 9);
            raw += time_str;
            if (msg.from != -1)
              raw += std::to_string(msg.from) + ": ";

            size_t indent_size = raw.size();

            raw += msg.content;

            auto msg_text = fit_into_screen(raw, std::string(indent_size, '-'));

            if (i == g::notification_pos)
            {
              for (auto& r : msg_text)
              {
                size_t pos = r.find("\x1b[0m");
                // Note that shadow_beg and shadow_end don't contains "\x1b[0m"
                while (pos != std::string::npos)
                {
                  r.insert(pos + 4, shadow_beg);
                  pos = r.find("\x1b[0m", pos + 3);
                }
                term::mvoutput({0, cursor_y++}, shadow_beg, r, shadow_end);
              }
            }
            else
            {
              for (auto& r : msg_text)
              {
                term::mvoutput({0, cursor_y++}, r);
              }
            }
          }
          line_pos += msg.content.size() / g::screen_width + 1;
        }

        term::mvoutput({g::screen_width / 2 - 3, g::screen_height - 2}, "Line ", g::notification_pos + 1);
        g::output_inited = true;
      }
      break;
    }

    static const auto show_info = []
    {
      std::string left = "Tank Version 0.2.1 (Compile: " + std::string(__DATE__) + ")";
      std::string right;
      if (g::game_mode == g::GameMode::NATIVE)
        right += "Native Mode";
      else if (g::game_mode == g::GameMode::SERVER)
      {
        right += "Server Mode | Port: " + std::to_string(g::online_server.get_port()) + " | ";
        size_t active_users = 0;
        for (auto& r : g::userdata)
        {
          if (r.second.active)
            ++active_users;
        }
        right += "User: " + std::to_string(active_users) + "/" + std::to_string(g::userdata.size());
      }
      else if (g::game_mode == g::GameMode::CLIENT)
      {
        right += "Client Mode | ";
        right += "ID: " + std::to_string(g::user_id) + " | Connected to " + g::online_client.get_host() + ":"
            + std::to_string(g::online_client.get_port()) + " | ";
        if (g::delay < 50)
          right += utils::color_256_fg(std::to_string(g::delay) + " ms", 2);
        else if (g::delay < 100)
          right += utils::color_256_fg(std::to_string(g::delay) + " ms", 11);
        else
          right += utils::color_256_fg(std::to_string(g::delay) + " ms", 9);
      }
      flexible_output(left, right);
    };

    // command
    if (g::typing_command)
    {
      term::move_cursor({g::cmd_pos - g::visible_cmd_line.first + 1, g::screen_height - 1});
      term::show_cursor();
    }
    else
    {
      term::move_cursor(term::TermPos(0, g::screen_height - 1));
      if (g::userdata[g::user_id].messages.empty()) show_info();
      else
      {
        auto now = std::chrono::steady_clock::now();
        auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>(now - g::last_message_displayed);
        if (d2 > g::msg_ttl)
        {
          auto msg = msg::read_a_message(g::user_id);
          if (msg.has_value())
          {
            std::string str = ((msg->from == -1) ? "" : std::to_string(msg->from) + ": ") + msg->content;
            int a2 = static_cast<int>(g::screen_width) - static_cast<int>(utils::display_width(str));

            str.erase(std::remove_if(str.begin(), str.end(),
              [](auto&& ch){return ch == '\n' || ch == '\r';}), str.end());

            if (a2 > 0)
              term::output(str, std::string(a2, ' '));
            else
            {
              term::output(str.substr(0, static_cast<int>(str.size()) + a2 - 1),
                           utils::color_256_fg(">", 9));
            }
            g::last_message_displayed = now;
          }
          else show_info();
        }
      }
    }
    term::flush();
  }
}
