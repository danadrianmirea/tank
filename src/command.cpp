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
#include "tank/globals.h"
#include "tank/game.h"
#include "tank/term.h"
#include "tank/utils.h"
#include "tank/archive.h"
#include "tank/serialization.h"
#include "tank/command.h"
#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <set>
#include <iterator>
#include <filesystem>

namespace czh::g
{
  const std::set<std::string> remote_cmds
  {
    "fill", "tp", "kill", "clear", "summon",
    "revive", "set", "tell", "pause", "continue",
    // unsafe
    "save", "load"
  };

  bool unsafe_mode{false};

  cmd::HintProvider fixed_provider(const cmd::Hints& hints, const std::string& cond = "")
  {
    return [hints, cond](const std::string& s)
    {
      if (cond.empty() || cond == s)
        return hints;
      return cmd::Hints{};
    };
  }

  cmd::HintProvider id_provider(const std::function<bool(decltype(g::tanks)::value_type)>& pred,
                                const std::string& cond = "")
  {
    return [pred, cond](const std::string& s)
    {
      if (cond.empty() || cond == s)
      {
        cmd::Hints ret;
        for (auto& r : g::tanks)
        {
          if (pred(r))
            ret.emplace_back(std::to_string(r.first), true);
        }
        return ret;
      }
      return cmd::Hints{};
    };
  }

  cmd::HintProvider valid_id_provider(const std::string& cond = "")
  {
    return id_provider([](auto&& r) { return true; }, cond);
  }

  cmd::HintProvider alive_id_provider(const std::string& cond = "")
  {
    return id_provider([](auto&& r) { return r.second->is_alive(); }, cond);
  }

  cmd::HintProvider valid_auto_id_provider(const std::string& cond = "")
  {
    return id_provider([](auto&& r) { return r.second->is_auto(); }, cond);
  }

  cmd::HintProvider user_id_provider(const std::string& cond = "")
  {
    return [cond](const std::string& s)
    {
      cmd::Hints ret{};
      if (cond.empty() || cond == s)
      {
        for (const auto& r : g::snapshot.userinfo)
          ret.emplace_back(std::to_string(r.first), true);
        return ret;
      }
      return cmd::Hints{};
    };
  }

  cmd::HintProvider range_provider(int a, int b, const std::string& cond = "") //[a, b)
  {
    cmd::Hints ret;
    for (size_t i = a; i < b; ++i)
      ret.emplace_back(std::to_string(i), true);
    return [ret, cond](const std::string& s)
    {
      if (cond.empty() || cond == s)
        return ret;
      return cmd::Hints{};
    };
  }

  cmd::HintProvider concat(const cmd::HintProvider& a, const cmd::HintProvider& b)
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

  const std::vector<cmd::CommandInfo> commands{
    {
      "help", "[line]", {
        [](const std::string& s)
        {
          cmd::Hints ret;
          for (size_t i = 1; i < help_text.size() + 1; ++i)
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
          if (utils::is_valid_id(s))
            return cmd::Hints{};
          return cmd::Hints{{"[to y-coordinate, int]", false}};
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
            return cmd::Hints{{"[Tick, int, milliseconds]", false}};
          else if (last_arg == "seed")
            return cmd::Hints{{"[Seed, int]", false}};
          else if (last_arg == "msgTTL")
            return cmd::Hints{{"[TTL, int, milliseconds]", false}};
          else if (last_arg == "longPressTH")
            return cmd::Hints{{"[Threshold, int, microseconds]", false}};
          else if (last_arg == "unsafe")
            return cmd::Hints{{"[bool]", false}, {"true", true}, {"false", true}};
          else // Tank's
          {
            if (utils::is_valid_id(last_arg))
            {
              if (game::id_at(std::stoull(last_arg))->is_auto())
              {
                return cmd::Hints{
                  {"bullet", true}, {"name", true},
                  {"max_hp", true}, {"hp", true}, {"target", true}
                };
              }
              return cmd::Hints{
                {"bullet", true}, {"name", true},
                {"max_hp", true}, {"hp", true}
              };
            }
          }
          return cmd::Hints{};
        },
        // Arg 2: Tank setting's value or bullet setting field
        [](const std::string& last_arg)
        {
          if (last_arg == "bullet")
            return cmd::Hints{{"hp", true}, {"lethality", true}, {"range", true}};
          else if (last_arg == "name")
            return cmd::Hints{{"[Name, string]", false}};
          else if (last_arg == "max_hp")
            return cmd::Hints{{"[Max HP, int]", false}};
          else if (last_arg == "hp")
            return cmd::Hints{{"[HP, int]", false}};
          else if (last_arg == "target")
            return cmd::Hints{{"[Target, ID]", false}};
          return cmd::Hints{};
        },
        // Arg 3: Bullet setting's value
        [](const std::string& last_arg)
        {
          if (last_arg == "hp")
            return cmd::Hints{{"[HP of bullet, int]", false}};
          else if (last_arg == "lethality")
            return cmd::Hints{{"[Lethality of bullet, int]", false}};
          else if (last_arg == "range")
            return cmd::Hints{{"[Range of bullet, int]", false}};
          return cmd::Hints{};
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
}

namespace czh::cmd
{
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
        msg::error(user_id, call.error[0]);
      return;
    }

    if (g::game_mode == g::GameMode::CLIENT)
    {
      if (g::remote_cmds.find(call.name) != g::remote_cmds.end())
      {
        int ret = g::online_client.run_command(str);
        if (ret != 0)
          msg::error(user_id, "Failed to run command on server.");
        return;
      }
    }

    if (call.is("help"))
    {
      if (call.args.empty())
        g::help_pos = 0;
      else if (auto v = call.get_if(
        [&call](int i)
        {
          return call.assert(i >= 1 && i < g::help_text.size(), "Page out of range");
        }); v)
      {
        int i = std::get<0>(*v);
        g::help_pos = i - 1;
      }
      else goto invalid_args;
      g::curr_page = g::Page::HELP;
      g::output_inited = false;
    }
    else if (call.is("status"))
    {
      if (call.args.empty())
      {
        g::curr_page = g::Page::STATUS;
        g::output_inited = false;
      }
      else goto invalid_args;
    }
    else if (call.is("notification"))
    {
      //std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      if (call.args.empty())
      {
        g::curr_page = g::Page::NOTIFICATION;
        g::output_inited = false;
      }
      else if (auto v = call.get_if([&call](std::string option)
      {
        return call.assert(option == "clear" || option == "read", "Invalid option.");
      }); v)
      {
        auto [opt] = *v;
        if (opt == "clear")
          g::userdata[user_id].messages.clear();
        else if (opt == "read")
        {
          for (auto& r : g::userdata[user_id].messages)
            r.read = true;
        }
      }
      else if (call.get_if([&call](std::string option, std::string f)
      {
        return call.assert(option == "clear" && f == "read", "Invalid option.");
      }))
      {
        auto& msgs = g::userdata[user_id].messages;
        msgs.erase(std::remove_if(msgs.begin(), msgs.end(), [](auto&& m) { return m.read; }), msgs.end());
      }
      else goto invalid_args;

      if (g::curr_page == g::Page::NOTIFICATION)
        g::output_inited = false;
    }
    else if (call.is("quit"))
    {
      if (call.args.empty())
      {
        std::lock_guard<std::mutex> ml(g::mainloop_mtx);
        std::lock_guard<std::mutex> dl(g::drawing_mtx);
        term::move_cursor({0, g::screen_height + 1});
        term::output("\033[?25h");
        msg::info(user_id, "Quitting.");
        term::flush();
        game::quit();
        std::exit(0);
      }
      else goto invalid_args;
    }
    else if (call.is("pause"))
    {
      if (call.args.empty())
      {
        g::game_running = false;
        msg::info(user_id, "Stopped.");
      }
      else goto invalid_args;
    }
    else if (call.is("continue"))
    {
      if (call.args.empty())
      {
        g::game_running = true;
        msg::info(user_id, "Continuing.");
      }
      else goto invalid_args;
    }
    else if (call.is("fill"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
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
          if (g::game_map.has(map::Status::TANK, {i, j}))
          {
            if (auto t = g::game_map.at(i, j).get_tank(); t != nullptr)
            {
              t->kill();
            }
          }
          else if (g::game_map.has(map::Status::BULLET, {i, j}))
          {
            auto bullets = g::game_map.at(i, j).get_bullets();
            for (auto& r : bullets)
            {
              r->kill();
            }
          }
          game::clear_death();
        }
      }
      if (is_wall)
      {
        g::game_map.fill(zone, map::Status::WALL);
      }
      else
      {
        g::game_map.fill(zone);
      }
      msg::info(user_id, "Filled from (" + std::to_string(from_x) + ","
                         + std::to_string(from_y) + ") to (" + std::to_string(to_x) + "," + std::to_string(to_y) +
                         ").");
    }
    else if (call.is("tp"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      int id = -1;
      map::Pos to_pos;
      auto check = [](const map::Pos& p)
      {
        return !g::game_map.has(map::Status::WALL, p) && !g::game_map.has(map::Status::TANK, p);
      };

      if (auto v = call.get_if(
        [&call](int id, int to_id)
        {
          return call.assert(utils::is_alive_id(id) && utils::is_alive_id(to_id),
                             "Both tank shall be alive.");
        }); v)
      {
        int to_id;
        std::tie(id, to_id) = *v;

        auto pos = game::id_at(to_id)->get_pos();
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
          return call.assert(utils::is_alive_id(id), "Tank shall be alive.")
                 && call.assert(check(map::Pos(x, y)), "Target pos is not available.");
        }); v)
      {
        std::tie(id, to_pos.x, to_pos.y) = *v;
      }
      else goto invalid_args;

      g::game_map.remove_status(map::Status::TANK, game::id_at(id)->get_pos());
      g::game_map.add_tank(game::id_at(id), to_pos);
      game::id_at(id)->get_pos() = to_pos;
      msg::info(user_id, game::id_at(id)->get_name() + " was teleported to ("
                         + std::to_string(to_pos.x) + "," + std::to_string(to_pos.y) + ").");
    }
    else if (call.is("revive"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      int id;
      if (call.args.empty())
      {
        for (auto& r : g::tanks)
        {
          game::revive(r.second->get_id());
        }
        msg::info(user_id, "Revived all tanks.");
        return;
      }
      else if (auto v = call.get_if([&call](int id)
      {
        return call.assert(utils::is_valid_id(id), "Invalid ID.");
      }); v)
      {
        std::tie(id) = *v;
      }
      else goto invalid_args;
      game::revive(id);
      msg::info(user_id, game::id_at(id)->get_name() + " revived.");
    }
    else if (call.is("summon"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
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
        game::add_auto_tank(lvl);
      }
      msg::info(user_id, "Added " + std::to_string(num) + " AutoTanks, Level: " + std::to_string(lvl) + ".");
    }
    else if (call.is("observe"))
    {
      int id;
      if (auto v = call.get_if([&call](int id)
      {
        return call.assert(g::snapshot.tanks.find(id) != g::snapshot.tanks.end(), "Invalid ID.");
      }); v)
      {
        std::tie(id) = *v;
      }
      else goto invalid_args;
      g::tank_focus = id;
      msg::info(user_id, "Observing " + g::snapshot.tanks[id].info.name);
    }
    else if (call.is("kill"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      if (call.args.empty())
      {
        for (auto& r : g::tanks)
        {
          if (r.second->is_alive()) r.second->kill();
        }
        game::clear_death();
        msg::info(user_id, "Killed all tanks.");
      }
      else if (auto v = call.get_if([&call](int id)
      {
        return call.assert(utils::is_valid_id(id), "Invalid ID.");
      }); v)
      {
        auto [id] = *v;
        auto t = game::id_at(id);
        t->kill();
        game::clear_death();
        msg::info(user_id, t->get_name() + " was killed.");
      }
      else goto invalid_args;
    }
    else if (call.is("clear"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      if (g::curr_page == g::Page::STATUS)
        g::output_inited = false;
      if (call.args.empty())
      {
        for (auto& r : g::bullets)
        {
          if (game::id_at(r->get_tank())->is_auto())
          {
            r->kill();
          }
        }
        for (auto& r : g::tanks)
        {
          if (r.second->is_auto())
          {
            r.second->kill();
          }
        }
        game::clear_death(); // before delete
        for (auto it = g::tanks.begin(); it != g::tanks.end();)
        {
          if (!it->second->is_auto())
          {
            ++it;
          }
          else
          {
            delete it->second;
            it = g::tanks.erase(it);
          }
        }
        msg::info(user_id, "Cleared all tanks.");
      }
      else if (auto v = call.get_if([&call](std::string f)
      {
        return call.assert(f == "death", "Invalid option.");
      }); v)
      {
        for (auto& r : g::bullets)
        {
          auto t = game::id_at(r->get_tank());
          if (t->is_auto() && !t->is_alive())
          {
            r->kill();
          }
        }
        for (auto& r : g::tanks)
        {
          if (r.second->is_auto() && !r.second->is_alive())
          {
            r.second->kill();
          }
        }
        game::clear_death(); // before delete
        for (auto it = g::tanks.begin(); it != g::tanks.end();)
        {
          if (!it->second->is_auto() || it->second->is_alive())
          {
            ++it;
          }
          else
          {
            delete it->second;
            it = g::tanks.erase(it);
          }
        }
        msg::info(user_id, "Cleared all died tanks.");
      }
      else if (auto v = call.get_if(
        [&call](int id)
        {
          return call.assert(utils::is_valid_id(id), "Invalid ID.") &&
                 call.assert(game::id_at(id)->is_auto(), "User's Tank can not be cleared.");
        }); v)
      {
        auto [id] = *v;
        for (auto& r : g::bullets)
        {
          if (r->get_tank() == id)
          {
            r->kill();
          }
        }
        auto t = game::id_at(id);
        t->kill();
        game::clear_death(); // before delete
        delete t;
        g::tanks.erase(id);
        msg::info(user_id, "ID: " + std::to_string(id) + " was cleared.");
      }
      else goto invalid_args;
    }
    else if (call.is("set"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      if (auto v = call.get_if(
        [&call](int id, std::string key, int value)
        {
          if (!utils::is_valid_id(id))
          {
            call.error.emplace_back("Invalid ID.");
            return false;
          }
          auto t = game::id_at(id);
          if (key == "max_hp")
            return call.assert(value > 0, "Invalid value. (Max HP > 0)");
          else if (key == "hp")
            return call.assert(value > 0 && value <= t->get_max_hp(), "Invalid value. (0 < HP <= Max HP)");
          else if (key == "target")
          {
            return call.assert(t->is_auto(), "Only AutoTank has target.")
                   && call.assert(t->is_alive(), "The tank shall be alive.")
                   && call.assert(utils::is_valid_id(value), "Invalid target id.")
                   && call.assert(value != id, "Can not set one as a target of itself.")
                   && call.assert(game::id_at(value)->is_alive(), "Target shall be alive.");
          }
          else
          {
            call.error.emplace_back("Invalid option.");
            return false;
          }
        }); v)
      {
        auto [id, key, value] = *v;
        if (key == "max_hp")
        {
          game::id_at(id)->get_info().max_hp = value;
          msg::info(user_id, "The max_hp of " + game::id_at(id)->get_name()
                             + " was set to " + std::to_string(value) + ".");
          return;
        }
        else if (key == "hp")
        {
          auto tank = game::id_at(id);
          if (!tank->is_alive()) game::revive(id);
          tank->get_hp() = value;
          msg::info(user_id, "The hp of " + tank->get_name()
                             + " was set to " + std::to_string(value) + ".");
          return;
        }
        else if (key == "target")
        {
          auto tank = dynamic_cast<tank::AutoTank*>(game::id_at(id));
          auto target = game::id_at(value);
          int ret = tank->set_target(value);
          if (ret == 0)
            msg::info(user_id, "The target of " + tank->get_name() + " was set to " + target->get_name() + ".");
          else
            msg::info(user_id, "Failed to find route from " + tank->get_name() + " to " + target->get_name() + ".");
          return;
        }
      }
      else if (auto v = call.get_if(
        [&call](int id, std::string key, std::string value)
        {
          return call.assert(utils::is_valid_id(id), "Invalid ID.")
                 && call.assert(key == "name", "Invalid option.");
        }); v)
      {
        auto [id, key, value] = *v;
        if (key == "name")
        {
          std::string old_name = game::id_at(id)->get_name();
          game::id_at(id)->get_name() = value;
          msg::info(user_id, "The name of " + old_name + " was set to '" + game::id_at(id)->get_name() + "'.");
          return;
        }
      }
      else if (auto v = call.get_if(
        [&call](std::string key, int arg)
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
          g::tick = std::chrono::milliseconds(arg);
          msg::info(user_id, "Tick was set to " + std::to_string(arg) + ".");
        }
        else if (option == "seed")
        {
          g::seed = arg;
          g::output_inited = false;
          msg::info(user_id, "Seed was set to " + std::to_string(arg) + ".");
        }
        else if (option == "msgTTL")
        {
          g::msg_ttl = std::chrono::milliseconds(arg);
          msg::info(user_id, "Message TTL was set to " + std::to_string(arg) + ".");
        }
        else if (option == "longPressTH")
        {
          g::long_pressing_threshold = arg;
          msg::info(user_id, "Long press threshold was set to " + std::to_string(arg) + ".");
        }
      }
      else if (auto v = call.get_if(
        [&call, &user_id](std::string key, bool arg)
        {
          return call.assert(key == "unsafe", "Invalid option.")
                 && call.assert(g::unsafe_mode || user_id == g::user_id,
                                "This command can only be executed by the server itself. (see '/help' for a workaround)");
        }); v)
      {
        auto [option, arg] = *v;
        g::unsafe_mode = arg;
        if (arg)
          msg::warn(user_id, "Unsafe mode enabled.");
        else
          msg::info(user_id, "Unsafe mode disbaled.");
      }
      else if (auto v = call.get_if(
        [&call](int id, std::string f, std::string key, int value)
        {
          bool ok = call.assert(utils::is_valid_id(id), "Invalid ID.")
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
        if (key == "hp")
        {
          game::id_at(id)->get_info().bullet.hp = value;
          msg::info(user_id,
                    "The HP of " + game::id_at(id)->get_name() + "'s bullet was set to " + std::to_string(value) + ".");
        }
        else if (key == "lethality")
        {
          game::id_at(id)->get_info().bullet.lethality = value;
          msg::info(user_id,
                    "The lethality of " + game::id_at(id)->get_name()
                    + "'s bullet was set to " + std::to_string(value) + ".");
        }
        else if (key == "range")
        {
          game::id_at(id)->get_info().bullet.range = value;
          msg::info(user_id, "The range of " + game::id_at(id)->get_name()
                             + "'s bullet was set to " + std::to_string(value) + ".");
        }
      }
      else goto invalid_args;
    }
    else if (call.is("server"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      //std::lock_guard<std::mutex> dl(g::drawing_mtx);
      if (auto v = call.get_if(
        [&call](std::string key, int port)
        {
          return call.assert(g::game_mode == g::GameMode::NATIVE, "Invalid request to start server mode.")
                 && call.assert(key == "start", "Invalid option")
                 && call.assert(utils::is_port(port), "Invalid port.");
        }); v)
      {
        auto [s, port] = *v;
        g::online_server.init();
        g::online_server.start(port);
        g::game_mode = g::GameMode::SERVER;
        msg::info(user_id, "Server started at " + std::to_string(port));
      }
      else if (auto v = call.get_if(
        [&call](std::string key)
        {
          return call.assert(g::game_mode == g::GameMode::SERVER, "Invalid request to stop server mode.")
                 && call.assert(key == "stop", "Invalid option.");
        }); v)
      {
        g::online_server.stop();
        for (auto& r : g::userdata)
        {
          if (r.first == 0) continue;
          g::tanks[r.first]->kill();
          g::tanks[r.first]->clear();
          delete g::tanks[r.first];
          g::tanks.erase(r.first);
        }
        g::userdata = {{0, g::userdata[0]}};
        g::game_mode = g::GameMode::NATIVE;
        msg::info(user_id, "Server stopped");
      }
      else goto invalid_args;
    }
    else if (call.is("connect"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      if (auto v = call.get_if(
        [&call](std::string ip, int port)
        {
          return call.assert(g::game_mode == g::GameMode::NATIVE, "Invalid request to connect a server.")
                 && call.assert(utils::is_ip(ip), "Invalid IP.")
                 && call.assert(utils::is_port(port), "Invalid port.");
        }); v)
      {
        auto [ip, port] = *v;
        g::online_client.init();
        auto try_connect = g::online_client.connect(ip, port);
        if (try_connect.has_value())
        {
          g::game_mode = g::GameMode::CLIENT;
          g::user_id = *try_connect;
          g::tank_focus = g::user_id;
          g::userdata = {{g::user_id, g::UserData{.user_id = g::user_id, .active = true}}};
          g::output_inited = false;
          msg::info(user_id, "Connected to " + ip + ":" + std::to_string(port) + " as " + std::to_string(g::user_id));
        }
      }
      else if (auto v = call.get_if(
        [&call](std::string ip, int port, std::string f, int id)
        {
          return call.assert(g::game_mode == g::GameMode::NATIVE, "Invalid request to connect a server.")
                 && call.assert(utils::is_ip(ip), "Invalid IP.")
                 && call.assert(utils::is_port(port), "Invalid port.")
                 && call.assert(f == "as", "Invalid option")
                 && call.assert(id >= 0, "Invalid ID.");
        }); v)
      {
        auto [ip, port, f, id] = *v;
        g::online_client.init();
        int try_connect = g::online_client.reconnect(ip, port, id);
        if (try_connect == 0)
        {
          g::game_mode = g::GameMode::CLIENT;
          g::user_id = static_cast<size_t>(id);
          g::tank_focus = g::user_id;
          g::userdata = {{g::user_id, g::UserData{.user_id = g::user_id, .active = true}}};
          g::output_inited = false;
          msg::info(user_id, "Reconnected to " + ip + ":" + std::to_string(port) + " as " + std::to_string(g::user_id));
        }
      }
      else goto invalid_args;
    }
    else if (call.is("disconnect"))
    {
      if (g::game_mode != g::GameMode::CLIENT)
      {
        call.error.emplace_back("Invalid request to disconnect.");
        goto invalid_args;
      }
      if (call.args.empty())
      {
        g::online_client.disconnect();
        g::game_mode = g::GameMode::NATIVE;
        g::userdata = {{0, g::userdata[g::user_id]}};
        g::user_id = 0;
        g::tank_focus = g::user_id;
        g::output_inited = false;
        msg::info(g::user_id, "Disconnected.");
      }
      else goto invalid_args;
    }
    else if (call.is("tell"))
    {
      int id = -1;
      std::string msg;
      if (auto v = call.get_if(
        [&call](int id, std::string msg)
        {
          return call.assert(utils::is_valid_id(id), "Invalid ID.");
        }); v)
      {
        std::tie(id, msg) = *v;
      }
      else if (auto v = call.get_if([](std::string msg) { return true; }); v)
      {
        std::tie(msg) = *v;
      }
      else goto invalid_args;
      int ret = msg::send_message(user_id, id, msg);
      if (ret == 0)
        msg::info(user_id, "Message sent.");
      else
        msg::info(user_id, "Failed sending message.");
    }
    else if (call.is("save"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      std::string filename;
      if (auto v = call.get_if(
        [&call, &user_id](std::string fn)
        {
          return call.assert(g::unsafe_mode || user_id == g::user_id,
                             "This command can only be executed by the server itself. (see '/help' for a workaround)");
        }); v)
      {
        std::tie(filename) = *v;
      }
      else goto invalid_args;

      std::ofstream out(filename, std::ios::binary);
      if (!out.good())
      {
        msg::error(user_id, "Failed to open '" + filename + "'.");
        return;
      }

      auto archive = ser::serialize(archive::archive());
      out.write(archive.c_str(), archive.size());
      out.close();
      msg::info(user_id, "Saved to '" + filename + "'.");
    }
    else if (call.is("load"))
    {
      std::lock_guard<std::mutex> ml(g::mainloop_mtx);
      std::lock_guard<std::mutex> dl(g::drawing_mtx);
      std::string filename;
      if (auto v = call.get_if(
        [&call, &user_id](std::string fn)
        {
          return call.assert(g::unsafe_mode || user_id == g::user_id,
                             "This command can only be executed by the server itself. (see '/help' for a workaround)");
        }); v)
      {
        std::tie(filename) = *v;
      }
      else goto invalid_args;

      std::ifstream in(filename, std::ios::binary);
      if (!in.good())
      {
        msg::error(user_id, "Failed to open '" + filename + "'.");
        return;
      }
      std::string tmp;
      in.seekg(0, std::ios::end);
      auto length = in.tellg();
      in.seekg(0, std::ios::beg);
      tmp.resize(length);
      in.read(tmp.data(), length);
      in.close();
      archive::load(ser::deserialize<archive::Archive>(tmp));
      g::output_inited = false;
      msg::info(user_id, "Loaded from '" + filename + "'.");
    }
    else
    {
      msg::error(user_id, "Invalid command. Type '/help' for more infomation.");
      return;
    }

    return;
  invalid_args:
    if (!call.error.empty())
    {
      for (auto& r : call.error)
        msg::error(user_id, r);
    }
    else
    [[unlikely]]
    {
      auto it = std::find_if(g::commands.cbegin(), g::commands.cend(),
                             [&call](auto&& f) { return f.cmd == call.name; });
      if (it != g::commands.end())
        msg::error(user_id, "Invalid arguments.(" + utils::color_256_fg(it->cmd + " " + it->args, 9) + ")");
      else[[unlikely]]
          msg::error(user_id, "Invalid arguments. Type '/help' for more infomation.(UNEXPECTED)");
    }
  }
}
