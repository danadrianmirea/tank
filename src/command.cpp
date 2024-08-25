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
#include "tank/term.h"
#include "tank/archive.h"
#include "tank/command.h"
#include "tank/broadcast.h"
#include "tank/utils/utils.h"
#include "tank/utils/serialization.h"
#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <set>
#include <iterator>
#include <ranges>
#include <tank/online.h>

namespace czh::cmd
{
  const std::set<std::string> remote_cmds
  {
    "fill", "tp", "kill", "clear", "summon",
    "revive", "set", "tell", "pause", "continue",
    // unsafe
    "save", "load"
  };

  input::HintProvider fixed_provider(const input::Hints& hints, const std::string& cond = "")
  {
    return [hints, cond](const std::string& s)
    {
      if (cond.empty() || cond == s)
        return hints;
      return input::Hints{};
    };
  }

  input::HintProvider id_provider(const std::function<bool(decltype(g::state.tanks)::value_type)>& pred,
                                  const std::string& cond = "")
  {
    return [pred, cond](const std::string& s)
    {
      if (cond.empty() || cond == s)
      {
        input::Hints ret;
        for (auto& r : g::state.tanks)
        {
          if (pred(r))
            ret.emplace_back(std::to_string(r.first), true);
        }
        return ret;
      }
      return input::Hints{};
    };
  }

  input::HintProvider valid_id_provider(const std::string& cond = "")
  {
    return id_provider([](auto&& r) { return true; }, cond);
  }

  input::HintProvider alive_id_provider(const std::string& cond = "")
  {
    return id_provider([](auto&& r) { return r.second->is_alive(); }, cond);
  }

  input::HintProvider valid_auto_id_provider(const std::string& cond = "")
  {
    return id_provider([](auto&& r) { return r.second->is_auto; }, cond);
  }

  input::HintProvider user_id_provider(const std::string& cond = "")
  {
    return [cond](const std::string& s)
    {
      input::Hints ret{};
      if (cond.empty() || cond == s)
      {
        for (const auto& r : draw::state.snapshot.userinfo | std::views::keys)
          ret.emplace_back(std::to_string(r), true);
        return ret;
      }
      return input::Hints{};
    };
  }

  input::HintProvider range_provider(int a, int b, const std::string& cond = "") //[a, b)
  {
    input::Hints ret;
    for (size_t i = a; i < b; ++i)
      ret.emplace_back(std::to_string(i), true);
    return [ret, cond](const std::string& s)
    {
      if (cond.empty() || cond == s)
        return ret;
      return input::Hints{};
    };
  }

  input::HintProvider concat(const input::HintProvider& a, const input::HintProvider& b)
  {
    return [a, b](const std::string& s)
    {
      auto ret = a(s);
      auto ret_b = b(s);
      ret.insert(ret.end(),
                 std::make_move_iterator(ret_b.begin()), std::make_move_iterator(ret_b.end()));
      return ret;
    };
  }

  inline bool is_ip(const std::string& s)
  {
    std::regex ipv4(R"(^(?:(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)($|(?!\.$)\.)){4}$)");
    std::regex ipv6("^(?:(?:[\\da-fA-F]{1,4})($|(?!:$):)){8}$");
    return std::regex_search(s, ipv4) || std::regex_search(s, ipv6);
  }

  inline bool is_port(const int p)
  {
    return p > 0 && p < 65536;
  }

  inline bool is_valid_id(const int id)
  {
    return g::id_at(id) != nullptr;
  }

  inline bool is_alive_id(const int id)
  {
    if (!is_valid_id(id)) return false;
    return g::id_at(id)->is_alive();
  }

  inline bool is_integer(const std::string& r)
  {
    if (r[0] != '+' && r[0] != '-' && !std::isdigit(r[0]))
      return false;

    for (size_t i = 1; i < r.size(); ++i)
    {
      if (!std::isdigit(r[i]))
        return false;
    }
    return true;
  }

  inline bool is_valid_id(const std::string& s)
  {
    if (s.empty()) return false;
    if (is_integer(s) && s[0] != '-')
    {
      size_t a;
      try
      {
        a = std::stoull(s);
      }
      catch (...)
      {
        return false;
      }
      return g::id_at(a) != nullptr;
    }
    return false;
  }

  inline bool is_alive_id(const std::string& s)
  {
    if (!is_valid_id(s)) return false;
    return g::id_at(std::stoull(s))->is_alive();
  }

  const std::vector<cmd::CommandInfo> commands{
    {
      "help", "[line]", {
        [](const std::string& s)
        {
          input::Hints ret;
          for (size_t i = 1; i < draw::state.help_text.size() + 1; ++i)
            ret.emplace_back(std::to_string(i), true);
          return ret;
        }
      }
    },
    {
      "server", "start [port] (or stop)", {
        fixed_provider({{"start", true}, {"stop", true}}),
        fixed_provider({{"[port]", false}}, "start")
      }
    },
    {
      "connect", "[ip] [port]", {
        fixed_provider({{"[ip]", false}}),
        fixed_provider({{"[port]", false}}),
        fixed_provider({{"as", true}}),
        fixed_provider({{"[remote id]", false}})
      }
    },
    {"disconnect", "** No arguments **", {}},
    {
      "fill", "[status] [A x,y] [B x,y optional]", {
        fixed_provider({{"0", true}, {"1", true}}),
        fixed_provider({{"[Point A x-coordinate, int]", false}}),
        fixed_provider({{"[Point A y-coordinate, int]", false}}),
        fixed_provider({{"[Point B x-coordinate, int]", false}}),
        fixed_provider({{"[Point B y-coordinate, int]", false}}),
      }
    },
    {
      "tp", "[A id] ([B id] or [B x,y])", {
        alive_id_provider(),
        concat(alive_id_provider(), fixed_provider({{"[to x-coordinate, int]", false}})),
        [](const std::string& s)
        {
          if (is_valid_id(s))
            return input::Hints{};
          return input::Hints{{"[to y-coordinate, int]", false}};
        }
      }
    },
    {"revive", "id", {valid_id_provider()}},
    {
      "summon", "[n] [level]", {
        fixed_provider({{"[number of tanks, int]", false}}),
        range_provider(1, 11)
      }
    },
    {"observe", "[id]", {alive_id_provider()}},
    {"kill", "[id optional]", {alive_id_provider()}},
    {
      "clear", "[id optional] (or death)", {
        concat(fixed_provider({{"death", true}}), valid_auto_id_provider())
      }
    },
    {
      "set", "[id] (bullet) [attr] [value]", {
        // Arg 0: ID or Game setting fields
        concat(fixed_provider({
                 {"tick", true}, {"seed", true},
                 {"msgTTL", true}, {"longPressTH", true},
                 {"unsafe", true}
               }), valid_id_provider()),
        // Arg 1: Tank setting fields or Game setting's value
        [](const std::string& last_arg)
        {
          if (last_arg == "tick")
            return input::Hints{{"[Tick, int, milliseconds]", false}};
          else if (last_arg == "seed")
            return input::Hints{{"[Seed, int]", false}};
          else if (last_arg == "msgTTL")
            return input::Hints{{"[TTL, int, milliseconds]", false}};
          else if (last_arg == "longPressTH")
            return input::Hints{{"[Threshold, int, microseconds]", false}};
          else if (last_arg == "unsafe")
            return input::Hints{{"[bool]", false}, {"true", true}, {"false", true}};
          else // Tank's
          {
            if (is_valid_id(last_arg))
            {
              if (g::id_at(std::stoull(last_arg))->is_auto)
              {
                return input::Hints{
                  {"bullet", true}, {"name", true},
                  {"max_hp", true}, {"hp", true}, {"target", true}
                };
              }
              return input::Hints{
                {"bullet", true}, {"name", true},
                {"max_hp", true}, {"hp", true}
              };
            }
          }
          return input::Hints{};
        },
        // Arg 2: Tank setting's value or bullet setting field
        [](const std::string& last_arg)
        {
          if (last_arg == "bullet")
            return input::Hints{{"hp", true}, {"lethality", true}, {"range", true}};
          else if (last_arg == "name")
            return input::Hints{{"[Name, string]", false}};
          else if (last_arg == "max_hp")
            return input::Hints{{"[Max HP, int]", false}};
          else if (last_arg == "hp")
            return input::Hints{{"[HP, int]", false}};
          else if (last_arg == "target")
            return input::Hints{{"[Target, ID]", false}};
          return input::Hints{};
        },
        // Arg 3: Bullet setting's value
        [](const std::string& last_arg)
        {
          if (last_arg == "hp")
            return input::Hints{{"[HP of bullet, int]", false}};
          else if (last_arg == "lethality")
            return input::Hints{{"[Lethality of bullet, int]", false}};
          else if (last_arg == "range")
            return input::Hints{{"[Range of bullet, int]", false}};
          return input::Hints{};
        },
      }
    },
    {
      "tell", "[id, optional], [msg]",
      {user_id_provider(), fixed_provider({{"[Message, string]", false}})}
    },
    {"pause", "** No arguments **", {}},
    {"continue", "** No arguments **", {}},
    {"quit", "** No arguments **", {}},
    {"status", "** No arguments **", {}},
    {
      "notification", "notification (action)",
      {
        fixed_provider({{"read", true}, {"clear", true}}),
        fixed_provider({{"read", true}}, "clear")
      }
    },
    {"save", "[filename, string]", {}},
    {"load", "[filename, string]", {}}
  };

  CmdCall parse(const std::string& cmd)
  {
    if (cmd.empty()) return {.good = false, .error = {"No command input."}};
    auto it = cmd.cbegin();
    auto skip_space = [&it, &cmd] { while (it < cmd.cend() && std::isspace(*it)) ++it; };
    skip_space();

    std::string name;
    while (it < cmd.cend() && !std::isspace(*it))
      name += *it++;

    std::vector<details::Arg> args;
    while (it < cmd.cend())
    {
      skip_space();
      std::string token;

      if (it < cmd.cend() && (*it == '"' || *it == '\''))
      {
        bool is_single = *it == '\'';
        ++it;
        bool matched = false;
        while (it < cmd.cend())
        {
          if ((is_single && *it == '\'') || (!is_single && *it == '"'))
          {
            matched = true;
            break;
          }
          if (auto ch = *it++; ch == '\\')
          {
            if (it < cmd.cend())
            {
              if (*it == '"')
              {
                token += '"';
                ++it;
              }
              else if (*it == '\'')
              {
                token += '\'';
                ++it;
              }
              else if (*it == '\\')
              {
                token += '\\';
                ++it;
              }
              else if (*it == 'a')
              {
                token += '\a';
                ++it;
              }
              else if (*it == 'b')
              {
                token += '\b';
                ++it;
              }
              else if (*it == 'f')
              {
                token += '\f';
                ++it;
              }
              else if (*it == 'n')
              {
                token += '\n';
                ++it;
              }
              else if (*it == 'r')
              {
                token += '\r';
                ++it;
              }
              else if (*it == 't')
              {
                token += '\t';
                ++it;
              }
              else if (*it == 'v')
              {
                token += '\v';
                ++it;
              }
              else
                token += '\\';
            }
            else
              return {.good = false, .error = {"Synax Error: Unexpected '\\' at the end."}};
          }
          else
            token += ch;
        }

        if (it == cmd.cend() && !matched)
          return {.good = false, .error = {"Synax Error: Expected '\"'."}};
        ++it;
        args.emplace_back(token);
      }
      else if (it < cmd.cend())
      {
        bool maybe_int = true;
        while (it < cmd.cend() && !std::isspace(*it))
        {
          if (!std::isdigit(*it) && *it != '+' && *it != '-') maybe_int = false;
          token += *it++;
        }
        if (!token.empty())
        {
          if (maybe_int)
          {
            bool stoi_success = true;
            int a = 0;
            try
            {
              a = std::stoi(token);
            }
            catch (...)
            {
              stoi_success = false;
            }
            if (stoi_success)
              args.emplace_back(a);
            else
              args.emplace_back(token);
          }
          else if (token == "true") args.emplace_back(true);
          else if (token == "false") args.emplace_back(false);
          else args.emplace_back(token);
        }
      }
    }
    return CmdCall{.good = true, .name = name, .args = args};
  }

  void run_command(size_t user_id, const std::string& str)
  {
    auto call = parse(str);

    if (!call.good)
    {
      if (!call.error.empty())
        bc::error(user_id, call.error[0]);
      return;
    }

    if (g::state.mode == g::Mode::CLIENT)
    {
      if (remote_cmds.find(call.name) != remote_cmds.end())
      {
        int ret = online::cli.run_command(str);
        if (ret != 0)
          bc::error(user_id, "Failed to run command on server.");
        return;
      }
    }

    if (call.is("help"))
    {
      if (call.args.empty())
        draw::state.help_pos = 0;
      else if (auto v = call.get_if(
        [&call](int i)
        {
          return call.assert(i >= 1 && i < draw::state.help_text.size(), "Page out of range");
        }); v)
      {
        int i = std::get<0>(*v);
        draw::state.help_pos = i - 1;
      }
      else goto invalid_args;
      g::state.page = g::Page::HELP;
      draw::state.inited = false;
    }
    else if (call.is("status"))
    {
      if (call.args.empty())
      {
        g::state.page = g::Page::STATUS;
        draw::state.inited = false;
      }
      else goto invalid_args;
    }
    else if (call.is("notification"))
    {
      //std::lock_guard ml(game::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      if (call.args.empty())
      {
        g::state.page = g::Page::NOTIFICATION;
        draw::state.inited = false;
      }
      else if (auto v = call.get_if([&call](const std::string& option)
      {
        return call.assert(option == "clear" || option == "read", "Invalid option.");
      }); v)
      {
        auto [opt] = *v;
        if (opt == "clear")
          g::state.users[user_id].messages.clear();
        else if (opt == "read")
        {
          for (auto& r : g::state.users[user_id].messages)
            r.read = true;
        }
      }
      else if (call.get_if([&call](const std::string& option, const std::string& f)
      {
        return call.assert(option == "clear" && f == "read", "Invalid option.");
      }))
      {
        auto& msgs = g::state.users[user_id].messages;
        msgs.erase(std::remove_if(msgs.begin(), msgs.end(), [](auto&& m) { return m.read; }), msgs.end());
      }
      else goto invalid_args;

      if (g::state.page == g::Page::NOTIFICATION)
        draw::state.inited = false;
    }
    else if (call.is("quit"))
    {
      if (call.args.empty())
      {
        std::lock_guard ml(g::mainloop_mtx);
        std::lock_guard dl(draw::drawing_mtx);
        term::move_cursor({0, draw::state.height + 1});
        term::output("\033[?25h");
        bc::info(user_id, "Quitting.");
        term::flush();
        g::quit();
        std::exit(0);
      }
      else goto invalid_args;
    }
    else if (call.is("pause"))
    {
      if (call.args.empty())
      {
        g::state.running = false;
        bc::info(user_id, "Stopped.");
      }
      else goto invalid_args;
    }
    else if (call.is("continue"))
    {
      if (call.args.empty())
      {
        g::state.running = true;
        bc::info(user_id, "Continuing.");
      }
      else goto invalid_args;
    }
    else if (call.is("fill"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      int from_x;
      int from_y;
      int to_x;
      int to_y;
      int is_wall = 0;
      if (auto v = call.get_if([&call](int w, int, int)
      {
        return call.assert(w == 0 || w == 1, "Invalid status.([0] Empty [1] Wall)");
      }); v)
      {
        std::tie(is_wall, from_x, from_y) = *v;
        to_x = from_x;
        to_y = from_y;
      }
      else if (auto v = call.get_if([&call](int w, int, int, int, int)
      {
        return call.assert(w == 0 || w == 1, "Invalid status.([0] Empty [1] Wall)");
      }); v)
      {
        std::tie(is_wall, from_x, from_y, to_x, to_y) = *v;
      }
      else goto invalid_args;

      map::Zone zone = {
        (std::min)(from_x, to_x), (std::max)(from_x, to_x) + 1,
        (std::min)(from_y, to_y), (std::max)(from_y, to_y) + 1
      };

      for (int i = zone.x_min; i < zone.x_max; ++i)
      {
        for (int j = zone.y_min; j < zone.y_max; ++j)
        {
          if (map::map.has(map::Status::TANK, {i, j}))
          {
            if (auto t = map::map.at(i, j).get_tank(); t != nullptr)
            {
              t->kill();
            }
          }
          else if (map::map.has(map::Status::BULLET, {i, j}))
          {
            auto bullets = map::map.at(i, j).get_bullets();
            for (auto& r : bullets)
            {
              r->kill();
            }
          }
          g::clear_death();
        }
      }
      if (is_wall)
      {
        map::map.fill(zone, map::Status::WALL);
      }
      else
      {
        map::map.fill(zone);
      }
      bc::info(user_id, "Filled from ({}, {}) to ({}, {}).", from_x, from_y, to_x, to_y);
    }
    else if (call.is("tp"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      int id = -1;
      map::Pos to_pos;
      auto check = [](const map::Pos& p)
      {
        return !map::map.has(map::Status::WALL, p) && !map::map.has(map::Status::TANK, p);
      };

      if (auto v = call.get_if(
        [&call](int id, int to_id)
        {
          return call.assert(is_alive_id(id) && is_alive_id(to_id),
                             "Both tank shall be alive.");
        }); v)
      {
        int to_id;
        std::tie(id, to_id) = *v;

        auto pos = g::id_at(to_id)->pos;
        std::vector<map::Pos> avail;
        for (int x = pos.x - 3; x < pos.x + 3; ++x)
        {
          for (int y = pos.y - 3; y < pos.y + 3; ++y)
          {
            if (check({x, y}))
              avail.emplace_back(x, y);
          }
        }
        if (avail.empty())
        {
          call.error.emplace_back("Target tank has no space around.");
          goto invalid_args;
        }
        else if (avail.size() == 1)
          to_pos = avail[0];
        else
          to_pos = avail[utils::randnum<size_t>(0, avail.size())];
      }
      else if (auto v = call.get_if(
        [&check, &call](int id, int x, int y)
        {
          return call.assert(is_alive_id(id), "Tank shall be alive.")
                 && call.assert(check(map::Pos(x, y)), "Target pos is not available.");
        }); v)
      {
        std::tie(id, to_pos.x, to_pos.y) = *v;
      }
      else goto invalid_args;

      auto tank = g::id_at(id);
      map::map.remove_status(map::Status::TANK, tank->pos);
      map::map.add_tank(tank, to_pos);
      tank->pos = to_pos;
      bc::info(user_id, "{} was teleported to ({}, {}).", tank->name, to_pos.x, to_pos.y);
    }
    else if (call.is("revive"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      int id;
      if (call.args.empty())
      {
        for (auto& r : g::state.tanks | std::views::values)
          g::revive(r->get_id(), g::state.users[user_id].visible_zone, user_id);
        bc::info(user_id, "Revived all tanks.");
        return;
      }
      else if (auto v = call.get_if([&call](int id)
      {
        return call.assert(is_valid_id(id), "Invalid ID.");
      }); v)
      {
        std::tie(id) = *v;
      }
      else goto invalid_args;
      g::revive(id, g::state.users[user_id].visible_zone, user_id);
      bc::info(user_id, g::id_at(id)->name + " revived.");
    }
    else if (call.is("summon"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      int num, lvl;
      if (auto v = call.get_if(
        [&call](int num, int lvl)
        {
          return call.assert(num > 0, "Invalid number.(> 0)")
                 && call.assert(lvl <= 10 && lvl >= 1, "Invalid lvl. (1 <= lvl <= 10)");
        }); v)
      {
        std::tie(num, lvl) = *v;
      }
      else goto invalid_args;
      for (size_t i = 0; i < num; ++i)
      {
        g::add_auto_tank(lvl, g::state.users[user_id].visible_zone, user_id);
      }
      bc::info(user_id, "Added {} AutoTanks, Level: {}.", num, lvl);
    }
    else if (call.is("observe"))
    {
      int id;
      if (auto v = call.get_if([&call](int id)
      {
        return call.assert(draw::state.snapshot.tanks.find(id) != draw::state.snapshot.tanks.end(), "Invalid ID.");
      }); v)
      {
        std::tie(id) = *v;
      }
      else goto invalid_args;
      draw::state.focus = id;
      bc::info(user_id, "Observing {}.", draw::state.snapshot.tanks[id].name);
    }
    else if (call.is("kill"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      if (call.args.empty())
      {
        for (auto& r : g::state.tanks)
        {
          if (r.second->is_alive()) r.second->kill();
        }
        g::clear_death();
        bc::info(user_id, "Killed all tanks.");
      }
      else if (auto v = call.get_if([&call](int id)
      {
        return call.assert(is_valid_id(id), "Invalid ID.");
      }); v)
      {
        auto [id] = *v;
        auto t = g::id_at(id);
        t->kill();
        g::clear_death();
        bc::info(user_id, "{} was killed by command.", t->name);
      }
      else goto invalid_args;
    }
    else if (call.is("clear"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      if (g::state.page == g::Page::STATUS)
        draw::state.inited = false;
      if (call.args.empty())
      {
        for (auto& r : g::state.bullets)
        {
          if (g::id_at(r->get_tank())->is_auto)
          {
            r->kill();
          }
        }
        for (auto& r : g::state.tanks)
        {
          if (r.second->is_auto)
          {
            r.second->kill();
          }
        }
        g::clear_death(); // before delete
        for (auto it = g::state.tanks.begin(); it != g::state.tanks.end();)
        {
          if (!it->second->is_auto)
          {
            ++it;
          }
          else
          {
            delete it->second;
            it = g::state.tanks.erase(it);
          }
        }
        bc::info(user_id, "Cleared all tanks.");
      }
      else if (auto v = call.get_if([&call](const std::string& f)
      {
        return call.assert(f == "death", "Invalid option.");
      }); v)
      {
        for (auto& r : g::state.bullets)
        {
          auto t = g::id_at(r->get_tank());
          if (t->is_auto && !t->is_alive())
          {
            r->kill();
          }
        }
        for (auto& r : g::state.tanks)
        {
          if (r.second->is_auto && !r.second->is_alive())
          {
            r.second->kill();
          }
        }
        g::clear_death(); // before delete
        for (auto it = g::state.tanks.begin(); it != g::state.tanks.end();)
        {
          if (!it->second->is_auto || it->second->is_alive())
          {
            ++it;
          }
          else
          {
            delete it->second;
            it = g::state.tanks.erase(it);
          }
        }
        bc::info(user_id, "Cleared all died tanks.");
      }
      else if (auto v = call.get_if(
        [&call](int id)
        {
          return call.assert(is_valid_id(id), "Invalid ID.") &&
                 call.assert(g::id_at(id)->is_auto, "User's Tank can not be cleared.");
        }); v)
      {
        auto [id] = *v;
        for (auto& r : g::state.bullets)
        {
          if (r->get_tank() == id)
          {
            r->kill();
          }
        }
        auto t = g::id_at(id);
        t->kill();
        g::clear_death(); // before delete
        delete t;
        g::state.tanks.erase(id);
        bc::info(user_id, "ID: {} was cleared.", id);
      }
      else goto invalid_args;
    }
    else if (call.is("set"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      if (auto v = call.get_if(
        [&call](int id, const std::string& key, int value)
        {
          if (!is_valid_id(id))
          {
            call.error.emplace_back("Invalid ID.");
            return false;
          }
          auto t = g::id_at(id);
          if (key == "max_hp")
            return call.assert(value > 0, "Invalid value. (Max HP > 0)");
          else if (key == "hp")
            return call.assert(value > 0 && value <= t->max_hp, "Invalid value. (0 < HP <= Max HP)");
          else if (key == "target")
          {
            return call.assert(t->is_auto, "Only AutoTank has target.")
                   && call.assert(t->is_alive(), "The tank shall be alive.")
                   && call.assert(is_valid_id(value), "Invalid target id.")
                   && call.assert(value != id, "Can not set one as a target of itself.")
                   && call.assert(g::id_at(value)->is_alive(), "Target shall be alive.");
          }
          else
          {
            call.error.emplace_back("Invalid option.");
            return false;
          }
        }); v)
      {
        auto [id, key, value] = *v;
        auto tank = g::id_at(id);
        if (key == "max_hp")
        {
          tank->max_hp = value;
          bc::info(user_id, "The Max HP of {} was set to {}.", tank->name, value);
          return;
        }
        else if (key == "hp")
        {
          if (!tank->is_alive()) g::revive(id, g::state.users[user_id].visible_zone, user_id);
          tank->hp = value;
          bc::info(user_id, "The HP of {} was set to {}.", tank->name, value);
          return;
        }
        else if (key == "target")
        {
          auto atank = dynamic_cast<tank::AutoTank*>(tank);
          auto target = g::id_at(value);
          int ret = atank->set_target(value);
          if (ret == 0)
            bc::info(user_id, "{}'s target was set to {}.", tank->name, target->name);
          else
            bc::info(user_id, "Failed to find route from {} to {}.", atank->name, target->name);
          return;
        }
      }
      else if (auto v = call.get_if(
        [&call](int id, const std::string& key, const std::string& value)
        {
          return call.assert(is_valid_id(id), "Invalid ID.")
                 && call.assert(key == "name", "Invalid option.");
        }); v)
      {
        auto [id, key, value] = *v;
        auto tank = g::id_at(id);
        if (key == "name")
        {
          std::string old_name = tank->name;
          tank->name = value;
          bc::info(user_id, "Renamed {} to {}.", old_name, value);
          return;
        }
      }
      else if (auto v = call.get_if(
        [&call](const std::string& key, int arg)
        {
          if (key == "tick")
            return call.assert(arg > 0, "Tick shall > 0.");
          else if (key == "seed")
            return true;
          else if (key == "msgTTL")
            return call.assert(arg > 0, "MsgTTL shall > 0.");
          else if (key == "longPressTH")
            return call.assert(arg > 0, "LongPressTH shall > 0.");
          else
          {
            call.error.emplace_back("Invalid option");
            return false;
          }
        }); v)
      {
        auto [option, arg] = *v;
        if (option == "tick")
        {
          cfg::config.tick = std::chrono::milliseconds(arg);
          bc::info(user_id, "Tick was set to {}.", arg);
        }
        else if (option == "seed")
        {
          map::map.seed = arg;
          draw::state.inited = false;
          bc::info(user_id, "Seed was set to {}.", arg);
        }
        else if (option == "msgTTL")
        {
          cfg::config.msg_ttl = std::chrono::milliseconds(arg);
          bc::info(user_id, "Message TTL was set to {}.", arg);
        }
        else if (option == "longPressTH")
        {
          cfg::config.long_pressing_threshold = arg;
          bc::info(user_id, "Long press threshold was set to {}.", arg);
        }
      }
      else if (auto v = call.get_if(
        [&call, &user_id](const std::string& key, bool arg)
        {
          return call.assert(key == "unsafe", "Invalid option.")
                 && call.assert(cfg::config.unsafe_mode || user_id == g::state.id,
                                "This command can only be executed by the server itself. (see '/help' for a workaround)");
        }); v)
      {
        auto [option, arg] = *v;
        cfg::config.unsafe_mode = arg;
        if (arg)
          bc::warn(user_id, "Unsafe mode enabled.");
        else
          bc::info(user_id, "Unsafe mode disbaled.");
      }
      else if (auto v = call.get_if(
        [&call](int id, const std::string& f, const std::string& key, int value)
        {
          bool ok = call.assert(is_valid_id(id), "Invalid ID.")
                    && call.assert(f == "bullet" && (key == "hp" || key == "lethality" || key == "range"),
                                   "Invalid option");
          if (ok && key == "range" && value <= 0)
          {
            call.error.emplace_back("Range shall > 0.");
            return false;
          }
          return ok;
        }); v)
      {
        auto [id, bulletstr, key, value] = *v;
        auto tank = g::id_at(id);
        if (key == "hp")
        {
          tank->bullet_hp = value;
          bc::info(user_id, "The HP of {}'s bullet was set to {}.", tank->name, value);
        }
        else if (key == "lethality")
        {
          tank->bullet_lethality = value;
          bc::info(user_id,
                   "The lethality of {}'s bullet was set to {}.", tank->name, value);
        }
        else if (key == "range")
        {
          tank->bullet_range = value;
          bc::info(user_id, "The range of {}'s bullet was set to {}.", tank->name, value);
        }
      }
      else goto invalid_args;
    }
    else if (call.is("server"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      //std::lock_guard dl(drawing::drawing_mtx);
      if (auto v = call.get_if(
        [&call](const std::string& key, int port)
        {
          return call.assert(key == "start", "Invalid option")
                 && call.assert(g::state.mode == g::Mode::NATIVE, "Invalid request to start server mode.")
                 && call.assert(is_port(port), "Invalid port.");
        }); v)
      {
        auto [s, port] = *v;
        g::state.mode = g::Mode::SERVER;
        online::svr.start(port);
      }
      else if (auto v = call.get_if(
        [&call](const std::string& key)
        {
          return call.assert(key == "stop", "Invalid option.")
                 && call.assert(g::state.mode == g::Mode::SERVER, "Invalid request to stop server mode.");
        }); v)
      {
        online::svr.stop();
        for (auto& r : g::state.users)
        {
          if (r.first == 0) continue;
          g::state.tanks[r.first]->kill();
          g::state.tanks[r.first]->clear();
          delete g::state.tanks[r.first];
          g::state.tanks.erase(r.first);
        }
        g::state.users = {{0, g::state.users[0]}};
        g::state.mode = g::Mode::NATIVE;
        bc::info(user_id, "Server stopped.");
      }
      else goto invalid_args;
    }
    else if (call.is("connect"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      if (auto v = call.get_if(
        [&call](const std::string& ip, int port)
        {
          return call.assert(g::state.mode == g::Mode::NATIVE, "Invalid request to connect a server.")
                 && call.assert(is_ip(ip), "Invalid IP.")
                 && call.assert(is_port(port), "Invalid port.");
        }); v)
      {
        auto [ip, port] = *v;
        g::state.mode = g::Mode::CLIENT;
        auto try_connect = online::cli.signup(ip, port);
        if (try_connect.has_value())
        {
          g::state.id = *try_connect;
          draw::state.focus = g::state.id;
          g::state.users = {{g::state.id, g::UserData{.user_id = g::state.id, .active = true}}};
          draw::state.inited = false;
          bc::info(user_id, "Connected to {}:{} as {}.", ip, port, g::state.id);
        }
      }
      else if (auto v = call.get_if(
        [&call](const std::string& ip, int port, const std::string& f, int id)
        {
          return call.assert(g::state.mode == g::Mode::NATIVE, "Invalid request to connect a server.")
                 && call.assert(is_ip(ip), "Invalid IP.")
                 && call.assert(is_port(port), "Invalid port.")
                 && call.assert(f == "as", "Invalid option")
                 && call.assert(id >= 0, "Invalid ID.");
        }); v)
      {
        auto [ip, port, f, id] = *v;
        g::state.mode = g::Mode::CLIENT;
        int try_connect = online::cli.login(ip, port, id);
        if (try_connect == 0)
        {
          g::state.mode = g::Mode::CLIENT;
          g::state.id = static_cast<size_t>(id);
          draw::state.focus = g::state.id;
          g::state.users = {{g::state.id, g::UserData{.user_id = g::state.id, .active = true}}};
          draw::state.inited = false;
          bc::info(user_id, "Reconnected to {}:{} as {}.", ip, port, g::state.id);
        }
      }
      else goto invalid_args;
    }
    else if (call.is("disconnect"))
    {
      if (g::state.mode != g::Mode::CLIENT)
      {
        call.error.emplace_back("Invalid request to disconnect.");
        goto invalid_args;
      }
      if (call.args.empty())
      {
        online::cli.logout();
        g::state.mode = g::Mode::NATIVE;
        g::state.users = {{0, g::state.users[g::state.id]}};
        g::state.id = 0;
        draw::state.focus = g::state.id;
        draw::state.inited = false;
        bc::info(g::state.id, "Disconnected.");
      }
      else goto invalid_args;
    }
    else if (call.is("tell"))
    {
      size_t id = bc::to_everyone;
      std::string msg;
      if (auto v = call.get_if(
        [&call](int id, const std::string& msg)
        {
          return call.assert(is_valid_id(id), "Invalid ID.");
        }); v)
      {
        std::tie(id, msg) = *v;
      }
      else if (auto v = call.get_if([](const std::string& msg) { return true; }); v)
      {
        std::tie(msg) = *v;
      }
      else goto invalid_args;
      int ret = bc::send_message(user_id, id, 0, msg);
      if (ret == 0)
        bc::info(user_id, "Message sent.");
      else
        bc::info(user_id, "Failed sending message.");
    }
    else if (call.is("save"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      std::string filename;
      if (auto v = call.get_if(
        [&call, &user_id](const std::string& fn)
        {
          return call.assert(cfg::config.unsafe_mode || user_id == g::state.id,
                             "This command can only be executed by the server itself. (see '/help' for a workaround)");
        }); v)
      {
        std::tie(filename) = *v;
      }
      else goto invalid_args;

      std::ofstream out(filename, std::ios::binary);
      if (!out.good())
      {
        bc::error(user_id, "Failed to open '{}'.", filename);
        return;
      }

      auto archive = utils::serialize(ar::archive());
      out.write(archive.c_str(), static_cast<std::streamsize>(archive.size()));
      out.close();
      bc::info(user_id, "Saved to '{}'.", filename);
    }
    else if (call.is("load"))
    {
      std::lock_guard ml(g::mainloop_mtx);
      std::lock_guard dl(draw::drawing_mtx);
      std::string filename;
      if (auto v = call.get_if(
        [&call, &user_id](const std::string& fn)
        {
          return call.assert(cfg::config.unsafe_mode || user_id == g::state.id,
                             "This command can only be executed by the server itself. (see '/help' for a workaround)");
        }); v)
      {
        std::tie(filename) = *v;
      }
      else goto invalid_args;

      std::ifstream in(filename, std::ios::binary);
      if (!in.good())
      {
        bc::error(user_id, "Failed to open '{}'.", filename);
        return;
      }
      std::string tmp;
      in.seekg(0, std::ios::end);
      auto length = in.tellg();
      in.seekg(0, std::ios::beg);
      tmp.resize(length);
      in.read(tmp.data(), length);
      in.close();
      ar::load(utils::deserialize<ar::Archive>(tmp));
      draw::state.inited = false;
      bc::info(user_id, "Loaded from '{}'.", filename);
    }
    else
    {
      bc::error(user_id, "Invalid command. Type '/help' for more infomation.");
      return;
    }

    return;
  invalid_args:
    if (!call.error.empty())
    {
      for (auto& r : call.error)
        bc::error(user_id, r);
    }
    else
    [[unlikely]]
    {
      auto it = std::find_if(commands.cbegin(), commands.cend(),
                             [&call](auto&& f) { return f.cmd == call.name; });
      if (it != commands.end())
        bc::error(user_id, "Invalid arguments.({})", utils::color_256_fg(it->cmd + " " + it->args, 9));
      else[[unlikely]]
          bc::error(user_id, "Invalid arguments. Type '/help' for more infomation.(UNEXPECTED)");
    }
  }
}
