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

#include "tank/online.h"
#include "tank/message.h"
#include "tank/command.h"
#include "tank/game_map.h"
#include "tank/game.h"
#include "tank/drawing.h"
#include "tank/broadcast.h"
#include "tank/utils/utils.h"
#include "tank/utils/serialization.h"
#include "tank/utils/debug.h"

#include <string>
#include <chrono>
#include <utility>
#include <vector>
#include <format>

namespace czh::online
{
  OnlineState state
  {
    .delay = 0
  };
  TankServer svr;
  TankClient cli;
  std::mutex online_mtx;

  std::string make_request(const std::string& cmd)
  {
    return utils::serialize(cmd, utils::serialize(" "));
  }

  template<typename... Args>
  std::string make_request(const std::string& cmd, Args&&... args)
  {
    return utils::serialize(cmd, utils::serialize(std::forward<Args>(args)...));
  }

  template<typename... Args>
  std::string make_response(Args&&... args)
  {
    return utils::serialize(std::forward<Args>(args)...);
  }

  void TankServer::start(int port_)
  {
    port = port_;
    if (svr != nullptr)
    {
      svr->stop();
      if (th.joinable())
        th.join();
      delete svr;
    }
    svr = new utils::TCPServer(
      [](utils::Socket_t fd, const std::string& req) -> std::string
      {
        auto [cmd, args] = utils::deserialize<std::string, std::string>(req);
        if (cmd == "tank_react")
        {
          auto [id, event] = utils::deserialize<size_t, tank::NormalTankEvent>(args);
          g::tank_react(id, event);
          return "";
        }
        else if (cmd == "update")
        {
          auto [id, zone] = utils::deserialize<size_t, map::Zone>(args);
          auto beg = std::chrono::steady_clock::now();
          std::lock_guard ml(g::mainloop_mtx);
          //std::lock_guard dl(drawing::drawing_mtx);
          auto& user = g::state.users[id];
          std::set<map::Pos> changes;
          for (auto& r : user.map_changes)
          {
            if (zone.contains(r))
              changes.insert(r);
          }
          auto d = std::chrono::duration_cast<std::chrono::milliseconds>
              (std::chrono::steady_clock::now() - beg);

          std::vector<msg::Message> msgs;
          for (auto it = user.messages.rbegin(); it < user.messages.rend(); ++it)
          {
            if (!it->read)
            {
              msgs.emplace_back(*it);
              it->read = true;
            }
            else // New messages are at the end of the std::vector, so just break.
              break;
          }
          user.map_changes.clear();
          user.last_update = std::chrono::steady_clock::now();
          user.visible_zone = zone.bigger_zone(-10);

          return make_response(d.count(), draw::extract_userinfo(),
                               changes, draw::extract_tanks(),
                               msgs, draw::extract_map(zone));
        }
        else if (cmd == "register")
        {
          std::string ipstr;
          if (auto ip = utils::get_peer_ip(fd); ip.has_value())
            ipstr = *ip;

          std::lock_guard ml(g::mainloop_mtx);
          std::lock_guard dl(draw::drawing_mtx);
          auto id = g::add_tank(draw::state.visible_zone, g::state.id);
          g::state.users[id] = g::UserData{
            .user_id = id,
            .ip = ipstr
          };
          g::state.users[id].last_update = std::chrono::steady_clock::now();
          g::state.users[id].active = true;
          bc::info(bc::to_everyone, "{} registered as {}.", ipstr, id);
          if (g::state.page == g::Page::STATUS)
            draw::state.inited = false;
          return make_response(id);
        }
        else if (cmd == "deregister")
        {
          std::string ipstr;
          if (auto ip = utils::get_peer_ip(fd); ip.has_value())
            ipstr = *ip;
          auto id = utils::deserialize<size_t>(args);
          std::lock_guard ml(g::mainloop_mtx);
          std::lock_guard dl(draw::drawing_mtx);
          bc::info(bc::to_everyone, "{} ({}) deregistered.", ipstr, id);
          g::state.tanks[id]->kill();
          g::state.tanks[id]->clear();
          delete g::state.tanks[id];
          g::state.tanks.erase(id);
          g::state.users.erase(id);
          return "";
        }
        else if (cmd == "login")
        {
          std::string ipstr;
          if (auto ip = utils::get_peer_ip(fd); ip.has_value())
            ipstr = *ip;
          auto id = utils::deserialize<size_t>(args);
          if (id == g::state.id)
            return make_response(-1, std::string{"Cannot login as the server user."});

          std::lock_guard ml(g::mainloop_mtx);
          std::lock_guard dl(draw::drawing_mtx);
          auto tank = g::id_at(id);
          if (tank == nullptr || tank->is_auto)
            return make_response(-1, std::string{"No such user."});
          else if (tank->is_alive())
            return make_response(-1, std::string{"Already logined."});
          bc::info(bc::to_everyone, "{} ({}) logined.", ipstr, id);
          g::revive(id, g::state.users[id].visible_zone, id);
          g::state.users[id].last_update = std::chrono::steady_clock::now();
          g::state.users[id].active = true;
          return make_response(0, std::string{"Success."});
        }
        else if (cmd == "logout")
        {
          std::string ipstr;
          if (auto ip = utils::get_peer_ip(fd); ip.has_value())
            ipstr = *ip;
          auto id = utils::deserialize<size_t>(args);
          std::lock_guard ml(g::mainloop_mtx);
          std::lock_guard dl(draw::drawing_mtx);
          bc::info(bc::to_everyone, "{} ({}) logout.", ipstr, id);
          g::state.tanks[id]->kill();
          g::state.tanks[id]->clear();
          g::state.users[id].active = false;
          return "";
        }
        else if (cmd == "add_auto_tank")
        {
          auto [id, zone, lvl]
              = utils::deserialize<size_t, map::Zone, size_t>(args);
          std::lock_guard ml(g::mainloop_mtx);
          std::lock_guard dl(draw::drawing_mtx);
          g::add_auto_tank(lvl, zone, id);
          return "";
        }
        else if (cmd == "run_command")
        {
          auto [id, command] = utils::deserialize<size_t, std::string>(args);
          cmd::run_command(id, command);
          return "";
        }
        return "";
      },

      [](utils::Socket_t fd)
      {
      },
      [](utils::Socket_t fd)
      {
        std::string ipstr;
        if (auto ip = utils::get_peer_ip(fd); ip.has_value())
          ipstr = *ip;
        bc::info(bc::to_everyone, std::format("{} disconnected unexpectedly.", ipstr));
      }
    );

    dbg::tank_assert(g::state.mode == g::Mode::SERVER);
    try
    {
      svr->bind_and_listen(port_);
      bc::info(g::state.id, "Server started at {}.", port);
      th = std::thread([this] { svr->start(); });
    }
    catch (std::runtime_error& err)
    {
      g::state.mode = g::Mode::NATIVE;
      bc::error(g::state.id, err.what());
      return;
    }
  }

  void TankServer::stop()
  {
    svr->stop();
    th.join();
    delete svr;
  }

  TankServer::~TankServer()
  {
    delete svr;
  }

  int TankServer::get_port() const
  {
    return port;
  }


  void TankServer::reset()
  {
    delete svr;
    svr = nullptr;
  }

  void TankClient::cli_failed(bool shutdown)
  {
    dbg::tank_assert(g::state.mode == g::Mode::CLIENT);
    cli->disconnect();
    delete cli;
    cli = nullptr;

    g::state.mode = g::Mode::NATIVE;
    g::state.users = {{0, g::state.users[g::state.id]}};
    g::state.id = 0;
    draw::state.focus = g::state.id;
    draw::state.inited = false;
    if (shutdown)
      bc::error(g::state.id, "Server is about to shutdown");
    else
      bc::error(g::state.id, "Disconnected due to network issues.");
  }

  std::optional<size_t> TankClient::signup(const std::string& addr_, int port_)
  {
    if (cli != nullptr)
    {
      cli->disconnect();
      delete cli;
      cli = nullptr;
    }
    cli = new utils::TCPClient();
    cli->init();
    std::lock_guard l(online_mtx);
    host = addr_;
    port = port_;
    if (cli->connect(addr_, port_) != 0)
    {
      cli_failed();
      return std::nullopt;
    }

    std::string content = make_request("register");
    auto [err, res] = cli->send_and_recv(content);

    if (err == utils::RecvRet::ok)
    {
      auto id = utils::deserialize<size_t>(res);
      return id;
    }
    else if (err == utils::RecvRet::failed)
    {
      cli_failed();
      return std::nullopt;
    }
    else if (err == utils::RecvRet::shutdown)
    {
      cli_failed(true);
      return std::nullopt;
    }
    return std::nullopt;
  }

  int TankClient::login(const std::string& addr_, int port_, size_t id)
  {
    if (cli != nullptr)
    {
      cli->disconnect();
      delete cli;
      cli = nullptr;
    }
    cli = new utils::TCPClient();
    cli->init();
    std::lock_guard l(online_mtx);
    host = addr_;
    port = port_;
    if (cli->connect(addr_, port_) != 0)
    {
      cli_failed();
      return -1;
    }

    std::string content = make_request("login", id);
    auto [err, res] = cli->send_and_recv(content);

    if (err == utils::RecvRet::ok)
    {
      auto [i, msg] = utils::deserialize<int, std::string>(res);
      if (i != 0)
      {
        bc::error(g::state.id, msg);
        return -2;
      }
      return 0;
    }
    else if (err == utils::RecvRet::failed)
    {
      cli_failed();
      return -1;
    }
    else if (err == utils::RecvRet::shutdown)
    {
      cli_failed(true);
      return -1;
    }
    return 0;
  }

  void TankClient::logout()
  {
    std::lock_guard l(online_mtx);
    std::string content = make_request("logout", g::state.id);
    if (cli->send(content) < 0)
    {
#ifdef _WIN32
      char buf[256];
      strerror_s(buf, sizeof(buf), errno);
      bc::error(g::state.id, std::format("send(): {}", buf));
#else
      bc::error(g::state.id, std::format("send(): {}", strerror(errno)));
#endif
    }
    cli->disconnect();
    delete cli;
    cli = nullptr;
  }

  int TankClient::tank_react(tank::NormalTankEvent e)
  {
    std::lock_guard l(online_mtx);
    std::string content = make_request("tank_react", g::state.id, e);
    if (cli->send(content) != 0)
    {
      cli_failed();
      return -1;
    }
    return 0;
  }

  int TankClient::update()
  {
    std::lock_guard l(online_mtx);
    auto beg = std::chrono::steady_clock::now();
    std::string content = make_request("update", g::state.id, draw::state.visible_zone.bigger_zone(10));
    auto [err, res] = cli->send_and_recv(content);

    if (err == utils::RecvRet::ok)
    {
      int delay;
      auto old_seed = draw::state.snapshot.map.seed;
      std::vector<msg::Message> msgs;
      std::tie(delay, draw::state.snapshot.userinfo, draw::state.snapshot.changes, draw::state.snapshot.tanks,
               msgs, draw::state.snapshot.map)
          = utils::deserialize<
            decltype(delay),
            decltype(draw::state.snapshot.userinfo),
            decltype(draw::state.snapshot.changes),
            decltype(draw::state.snapshot.tanks),
            decltype(msgs),
            decltype(draw::state.snapshot.map)>(res);
      int curr_delay = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>
                         (std::chrono::steady_clock::now() - beg).count()) - delay;
      state.delay = static_cast<int>((state.delay + 0.1 * curr_delay) / 1.1);

      for (auto& r : msgs) // reverse
        g::state.users[g::state.id].messages.emplace_back(r);

      if (old_seed != draw::state.snapshot.map.seed)
        draw::state.inited = false;
      return 0;
    }
    else if (err == utils::RecvRet::failed)
    {
      cli_failed();
      return -1;
    }
    else if (err == utils::RecvRet::shutdown)
    {
      cli_failed(true);
      state.delay = -1;
      draw::state.inited = false;
      return -1;
    }
    return -1;
  }

  int TankClient::add_auto_tank(size_t lvl)
  {
    std::lock_guard l(online_mtx);
    std::string content = make_request("add_auto_tank", g::state.id, draw::state.visible_zone, lvl);
    if (cli->send(content) != 0)
    {
      cli_failed();
      return -1;
    }
    return 0;
  }

  int TankClient::run_command(const std::string& str)
  {
    std::lock_guard l(online_mtx);
    std::string content = make_request("run_command", g::state.id, str);
    if (cli->send(content) != 0)
    {
      cli_failed();
      return -1;
    }
    return 0;
  }

  TankClient::~TankClient()
  {
    delete cli;
  }

  int TankClient::get_port() const
  {
    return port;
  }

  std::string TankClient::get_host() const
  {
    return host;
  }
}
