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
#include "tank/command.h"
#include <string>
#include <vector>
#include <mutex>
#include <set>
#include <iterator>

namespace czh::g
{
  const std::set<std::string> remote_cmds
  {
    "fill", "tp", "kill", "clear", "summon", "revive", "set", "tell", "pause", "continue"
  };

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
        fixed_provider({{"[port]", false}})
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
                 {"msgTTL", true}, {"longPressTH", true}
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
    {"tell", "[id, optional], [msg]", {valid_id_provider(), fixed_provider({{"[Message, string]", false}})}},
    {"pause", "** No arguments **", {}},
    {"continue", "** No arguments **", {}},
    {"quit", "** No arguments **", {}},
    {"status", "** No arguments **", {}}
  };
}

namespace czh::cmd
{
  CmdCall parse(const std::string& cmd)
  {
    if (cmd.empty()) return {};
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
      std::string temp;
      bool maybe_int = true;
      while (it < cmd.cend() && !std::isspace(*it))
      {
        if (!std::isdigit(*it) && *it != '+' && *it != '-') maybe_int = false;
        temp += *it++;
      }
      if (!temp.empty())
      {
        if (maybe_int)
        {
          bool stoi_success = true;
          int a = 0;
          try
          {
            a = std::stoi(temp);
          }
          catch (...)
          {
            stoi_success = false;
          }
          if (stoi_success)
            args.emplace_back(a);
          else
            args.emplace_back(temp);
        }
        else
        {
          args.emplace_back(temp);
        }
      }
    }
    return CmdCall{.name = name, .args = args};
  }

  void run_command(size_t user_id, const std::string& str)
  {
    auto call = parse(str);
    if (g::game_mode == g::GameMode::CLIENT)
    {
      if (g::remote_cmds.find(call.name) != g::remote_cmds.end())
      {
        g::online_client.run_command(str);
        return;
      }
    }

    if (call.is("help"))
    {
      if (call.args.empty())
        g::help_lineno = 1;
      else if (auto v = call.get_if(
        [](int i)
        {
          return i >= 1 && i < g::help_text.size();
        }); v)
      {
        int i = std::get<0>(*v);
        g::help_lineno = i;
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
    else if (call.is("quit"))
    {
      if (call.args.empty())
      {
        std::lock_guard<std::mutex> l(g::mainloop_mtx);
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
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      int from_x;
      int from_y;
      int to_x;
      int to_y;
      int is_wall = 0;
      if (auto v = call.get_if([](int w, int, int) { return w == 0 || w == 1; }); v)
      {
        std::tie(is_wall, from_x, from_y) = *v;
        to_x = from_x;
        to_y = from_y;
      }
      else if (auto v = call.get_if([](int w, int, int, int, int) { return w == 0 || w == 1; }); v)
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
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      int id = -1;
      map::Pos to_pos;
      auto check = [](const map::Pos& p)
      {
        return !g::game_map.has(map::Status::WALL, p) && !g::game_map.has(map::Status::TANK, p);
      };

      if (auto v = call.get_if(
        [](int id, int to_id)
        {
          return utils::is_alive_id(id) && utils::is_alive_id(to_id);
        }); v)
      {
        int to_id;
        std::tie(id, to_id) = *v;

        auto pos = game::id_at(to_id)->get_pos();
        map::Pos pos_up(pos.x, pos.y + 1);
        map::Pos pos_down(pos.x, pos.y - 1);
        map::Pos pos_left(pos.x - 1, pos.y);
        map::Pos pos_right(pos.x + 1, pos.y);
        if (check(pos_up))
          to_pos = pos_up;
        else if (check(pos_down))
          to_pos = pos_down;
        else if (check(pos_left))
          to_pos = pos_left;
        else if (check(pos_right))
          to_pos = pos_right;
        else goto invalid_args;
      }
      else if (auto v = call.get_if(
        [&check](int id, int x, int y)
        {
          return utils::is_alive_id(id) && check(map::Pos(x, y));
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
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      int id;
      if (call.args.empty())
      {
        for (auto& r : g::tanks)
        {
          if (!r.second->is_alive()) game::revive(r.second->get_id());
        }
        msg::info(user_id, "Revived all tanks.");
        return;
      }
      else if (auto v = call.get_if([](int id) { return utils::is_valid_id(id); }); v)
      {
        std::tie(id) = *v;
      }
      else goto invalid_args;
      game::revive(id);
      msg::info(user_id, game::id_at(id)->get_name() + " revived.");
    }
    else if (call.is("summon"))
    {
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      int num, lvl;
      if (auto v = call.get_if(
        [](int num, int lvl)
        {
          return num > 0 && lvl <= 10 && lvl >= 1;
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
      if (auto v = call.get_if([](int id) { return g::snapshot.tanks.find(id) != g::snapshot.tanks.end(); }); v)
      {
        std::tie(id) = *v;
      }
      else goto invalid_args;
      g::tank_focus = id;
      msg::info(user_id, "Observing " + g::snapshot.tanks[id].info.name);
    }
    else if (call.is("kill"))
    {
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      if (call.args.empty())
      {
        for (auto& r : g::tanks)
        {
          if (r.second->is_alive()) r.second->kill();
        }
        game::clear_death();
        msg::info(user_id, "Killed all tanks.");
      }
      else if (auto v = call.get_if([](int id) { return utils::is_valid_id(id); }); v)
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
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
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
      else if (auto v = call.get_if([](std::string f) { return f == "death"; }); v)
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
        [](int id) { return utils::is_valid_id(id) && game::id_at(id)->is_auto(); }); v)
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
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      if (auto v = call.get_if(
        [](int id, std::string key, int value)
        {
          if (!utils::is_valid_id(id)) return false;
          auto t = game::id_at(id);
          return utils::is_valid_id(id) &&
                 ((key == "max_hp" && value > 0)
                  || (key == "hp" && value > 0 && value <= t->get_max_hp())
                  || (key == "target" && t->is_auto() && t->is_alive()
                      && utils::is_valid_id(value) && value != id && game::id_at(value)->is_alive()));
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
          tank->target(value, game::id_at(value)->get_pos());
          msg::info(user_id, "The target of " + tank->get_name() + " was set to " + std::to_string(value) + ".");
          return;
        }
      }
      else if (auto v = call.get_if(
        [](int id, std::string key, std::string value) { return utils::is_valid_id(id) && key == "name"; }); v)
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
        [](std::string key, int arg)
        {
          return (key == "tick" && arg > 0)
                 || (key == "seed")
                 || (key == "msgTTL" && arg > 0)
                 || (key == "longPressTH" && arg > 0);
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
        [](int id, std::string f, std::string key, int value)
        {
          return utils::is_valid_id(id) && f == "bullet"
                 && (key == "hp" || key == "lethality" || key == "range");
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
                    "The lethality of " + game::id_at(id)->get_name() + "'s bullet was set to " + std::to_string(value)
                    +
                    ".");
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
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      if (auto v = call.get_if(
        [](std::string key, int port)
        {
          return g::game_mode == g::GameMode::NATIVE && key == "start" && utils::is_port(port);
        }); v)
      {
        auto [s, port] = *v;
        g::online_server.init();
        g::online_server.start(port);
        g::game_mode = g::GameMode::SERVER;
        msg::info(user_id, "Server started at " + std::to_string(port));
      }
      else if (auto v = call.get_if(
        [](std::string key)
        {
          return g::game_mode == g::GameMode::SERVER && key == "stop";
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
      std::lock_guard<std::mutex> l(g::mainloop_mtx);
      if (auto v = call.get_if(
        [](std::string ip, int port)
        {
          return g::game_mode == g::GameMode::NATIVE && utils::is_ip(ip) && utils::is_port(port);
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
          g::userdata = {{g::user_id, g::UserData{.user_id = g::user_id}}};
          g::output_inited = false;
          msg::info(user_id, "Connected to " + ip + ":" + std::to_string(port) + " as " + std::to_string(g::user_id));
        }
      }
      else goto invalid_args;
    }
    else if (call.is("disconnect"))
    {
      if (g::game_mode == g::GameMode::CLIENT && call.args.empty())
      {
        g::online_client.disconnect();
        g::game_mode = g::GameMode::NATIVE;
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
        [](int id, std::string msg) { return utils::is_valid_id(id); }); v)
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
    else
    {
      msg::error(user_id, "Invalid command. Type '/help' for more infomation.");
      return;
    }

    return;
  invalid_args:
    auto it = std::find_if(g::commands.cbegin(), g::commands.cend(),
                           [&call](auto&& f) { return f.cmd == call.name; });
    if (it != g::commands.end())
      msg::error(user_id, "Invalid arguments.(" + utils::color_256_fg(it->cmd + " " + it->args, 9) + ")");
    else[[unlikely]]
        msg::error(user_id, "Invalid arguments. Type '/help' for more infomation.(UNEXPECTED)");
    return;
  }
}
