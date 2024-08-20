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
#include "tank/globals.h"
#include "tank/drawing.h"
#include "tank/utils.h"
#include "tank/serialization.h"

#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <string_view>
#include <chrono>
#include <utility>
#include <vector>

namespace czh::g
{
  online::TankServer online_server{};
  online::TankClient online_client{};
  int client_failed_attempts = 0;
  int delay = 0;
  std::mutex online_mtx;
}

namespace czh::online
{
#ifdef _WIN32
  WSADATA wsa_data;
  [[maybe_unused]] int wsa_startup_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  Thpool::Thpool(std::size_t size) : run(true) { add_thread(size); }

  Thpool::~Thpool()
  {
    run = false;
    cond.notify_all();
    for (auto& th : pool)
    {
      if (th.joinable())
      {
        th.join();
      }
    }
  }

  void Thpool::add_thread(std::size_t num)
  {
    for (std::size_t i = 0; i < num; i++)
    {
      pool.emplace_back(
        [this]
        {
          while (this->run)
          {
            Task task; {
              std::unique_lock<std::mutex> lock(this->th_mutex);
              this->cond.wait(lock, [this] { return !this->run || !this->tasks.empty(); });
              if (!this->run && this->tasks.empty()) return;
              task = std::move(this->tasks.front());
              this->tasks.pop();
            }
            task();
          }
        }
      );
    }
  }

  void Thpool::add_task(const std::function<void()>& func)
  {
    utils::tank_assert(run, "Can not add task on stopped Thpool"); {
      std::lock_guard<std::mutex> lock(th_mutex);
      tasks.emplace([func] { func(); });
    }
    cond.notify_one();
  }

  Addr::Addr() : addr(), len(sizeof(addr))
  {
    std::memset(&addr, 0, sizeof(addr));
  }

  Addr::Addr(struct sockaddr_in addr_, decltype(len) len_)
    : addr(addr_), len(len_)
  {
  }

  Addr::Addr(const std::string& ip, int port): addr(), len(0)
  {
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);
    len = sizeof(addr);
  }

  Addr::Addr(int port): addr(), len(0)
  {
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    len = sizeof(addr);
  }

  std::string Addr::ip() const
  {
#ifdef _WIN32
    std::string str(16, '\0');
    str = inet_ntoa(addr.sin_addr);
#else
    char buf[16];
    inet_ntop(AF_INET, &addr.sin_addr, buf, len);
    std::string str{buf};
#endif
    return str;
  }

  int Addr::port() const
  {
    return ntohs(addr.sin_port);
  }

  std::string Addr::to_string() const
  {
    return ip() + ":" + std::to_string(port());
  }

  bool check(bool a)
  {
    if (!a)
    {
      msg::error(g::user_id, strerror(errno));
    }
    return a;
  }

  TCPSocket::TCPSocket() : fd(-1)
  {
    init();
  }

  TCPSocket::TCPSocket(Socket_t fd_) : fd(fd_)
  {
  }

  TCPSocket::TCPSocket(TCPSocket&& soc) noexcept: fd(soc.fd)
  {
    soc.fd = -1;
  }

  TCPSocket::~TCPSocket()
  {
    if (fd != -1)
    {
#ifdef _WIN32
      closesocket(fd);
#else
      ::close(fd);
#endif
      fd = -1;
    }
  }

  std::tuple<TCPSocket, Addr> TCPSocket::accept() const
  {
    Addr addr;
#ifdef _WIN32
    return {std::move(TCPSocket{::accept(fd, reinterpret_cast<sockaddr *>(&addr.addr),
                                         reinterpret_cast<int *> (&addr.len))}), addr};
#else
    return {
      std::move(
        TCPSocket{::accept(fd, reinterpret_cast<sockaddr*>(&addr.addr), reinterpret_cast<socklen_t*>(&addr.len))}),
      addr
    };
#endif
  }

  Socket_t TCPSocket::get_fd() const { return fd; }

  int send_all_data(Socket_t sock, const char* buf, int size)
  {
    while (size > 0)
    {
#ifdef _WIN32
      int s = ::send(sock, buf, size, 0);
#else
      int s = static_cast<int>(::send(sock, buf, size, MSG_NOSIGNAL));
#endif
      if (!check(s >= 0)) return -1;
      size -= s;
      buf += s;
    }
    return 0;
  }

  int receive_all_data(Socket_t sock, char* buf, int size)
  {
    while (size > 0)
    {
      int r = static_cast<int>(::recv(sock, buf, size, 0));
      if (!check(r > 0)) return -1;
      size -= r;
      buf += r;
    }
    return 0;
  }

  int TCPSocket::send(const std::string& str) const
  {
    MsgHeader header{
      .magic = htonl(HEADER_MAGIC),
      .version = htons(PROTOCOL_VERSION),
      .content_length = htonl(str.size())
    };
    if (send_all_data(fd, reinterpret_cast<const char*>(&header), sizeof(MsgHeader)) != 0) return -1;
    if (send_all_data(fd, str.data(), static_cast<int>(str.size())) != 0) return -1;
    return 0;
  }

  std::optional<std::string> TCPSocket::recv() const
  {
    MsgHeader header{};
    std::string recv_result;
    if (receive_all_data(fd, reinterpret_cast<char*>(&header), sizeof(MsgHeader)) != 0)
    {
      return std::nullopt;
    }

    if (ntohl(header.magic) != HEADER_MAGIC || ntohs(header.version) != PROTOCOL_VERSION)
    {
      auto addr = get_peer_addr();
      if (!addr.has_value())
        msg::warn(g::user_id, "Ignore invalid data");
      else
        msg::warn(g::user_id, "Ignore invalid data from " + addr->to_string());

      return std::nullopt;
    }

    recv_result.resize(ntohl(header.content_length), 0);

    if (receive_all_data(fd, reinterpret_cast<char*>(recv_result.data()), static_cast<int>(recv_result.size())) != 0)
    {
      return std::nullopt;
    }

    return recv_result;
  }

  int TCPSocket::bind(Addr addr) const
  {
    if (!check(::bind(fd, reinterpret_cast<sockaddr*>(&addr.addr), addr.len) == 0))
    {
      return -1;
    }
    return 0;
  }

  int TCPSocket::listen() const
  {
    if (!check(::listen(fd, 0) != -1))
    {
      return -1;
    }
    return 0;
  }

  int TCPSocket::connect(Addr addr) const
  {
    if (!check(::connect(fd, reinterpret_cast<sockaddr*>(&addr.addr), addr.len) != -1))
    {
      return -1;
    }
    return 0;
  }

  std::optional<Addr> TCPSocket::get_peer_addr() const
  {
    struct sockaddr_in peer_addr{};
    decltype(Addr::len) peer_len;
    peer_len = sizeof(peer_addr);
    if (!check(getpeername(fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_len) != -1))
    {
      return std::nullopt;
    }
    return Addr{peer_addr, peer_len};
  }

  void TCPSocket::reset()
  {
    if (fd != -1)
    {
#ifdef _WIN32
      closesocket(fd);
#else
      ::close(fd);
#endif
    }
    init();
  }

  void TCPSocket::init()
  {
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof(on));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on));
    utils::tank_assert(fd != -1, "socket reset failed.");
  }

  Socket_t TCPSocket::release()
  {
    auto f = fd;
    fd = -1;
    return f;
  }

  Req::Req(const Addr& addr_, std::string content_)
    : addr(addr_), content(std::move(content_))
  {
  }

  const Addr& Req::get_addr() const { return addr; }

  const auto& Req::get_content() const { return content; }

  void Res::set_content(const std::string& c) { content = c; }

  const auto& Res::get_content() const { return content; }

  TCPServer::TCPServer() : running(false), thpool(16)
  {
  }

  TCPServer::TCPServer(std::function<void(const Req&, Res&)> router_)
    : running(false), router(std::move(router_)), thpool(16)
  {
  }

  void TCPServer::init(const std::function<void(const Req&, Res&)>& router_)
  {
    router = router_;
    running = false;
  }

  void TCPServer::start(int port)
  {
    running = true;
    TCPSocket socket;
    check(socket.bind(Addr{port}) == 0);
    check(socket.listen() == 0);
    //sockets.emplace_back(socket.get_fd());
    while (running)
    {
      auto tmp = socket.accept();
      auto& [clnt_socket_, clnt_addr] = tmp;
      utils::tank_assert(clnt_socket_.get_fd() != -1, "socket accept failed");
      auto fd = clnt_socket_.release();
      sockets.emplace_back(fd);
      thpool.add_task(
        [this, fd]
        {
          TCPSocket clnt_socket{fd};
          while (running)
          {
            auto request = clnt_socket.recv();
            if (!request.has_value() || *request == "quit") break;
            Res response;
            auto addr = clnt_socket.get_peer_addr();
            if (!addr.has_value())
            {
              break;
            }
            router(Req{*addr, *request}, response);
            if (!response.get_content().empty())
            {
              check(clnt_socket.send(response.get_content()) == 0);
            }
          }
        });
    }
  }

  void TCPServer::stop()
  {
    for (auto& r : sockets)
    {
#ifdef _WIN32
      shutdown(r, SD_BOTH);
#else
      ::shutdown(r, SHUT_RDWR);
#endif
    }
    sockets.clear();
    running = false;
  }

  TCPServer::~TCPServer()
  {
    stop();
  }

  TCPClient::~TCPClient()
  {
    if (disconnect() < 0)
      msg::error(g::user_id, strerror(errno));
  }

  int TCPClient::disconnect() const
  {
    return socket.send("quit");
  }


  int TCPClient::connect(const std::string& addr, int port) const
  {
    return socket.connect({addr, port});
  }

  int TCPClient::send(const std::string& str) const
  {
    return socket.send(str);
  }

  std::optional<std::string> TCPClient::recv() const
  {
    return socket.recv();
  }

  std::optional<std::string> TCPClient::send_and_recv(const std::string& str) const
  {
    int ret = socket.send(str);
    if (ret != 0) return std::nullopt;
    return socket.recv();
  }

  void TCPClient::reset()
  {
    socket.reset();
  }

  std::string make_request(const std::string& cmd)
  {
    return ser::serialize(cmd, ser::serialize(" "));
  }

  template<typename... Args>
  std::string make_request(const std::string& cmd, Args&&... args)
  {
    return ser::serialize(cmd, ser::serialize(std::forward<Args>(args)...));
  }

  template<typename... Args>
  std::string make_response(Args&&... args)
  {
    return ser::serialize(std::forward<Args>(args)...);
  }

  void TankServer::init()
  {
    if (svr != nullptr)
    {
      svr->stop();
      delete svr;
    }
    svr = new TCPServer{};
    svr->init([](const Req& req, Res& res)
    {
      auto [cmd, args] = ser::deserialize<std::string, std::string>(req.get_content());
      if (cmd == "tank_react")
      {
        auto [id, event] = ser::deserialize<size_t, tank::NormalTankEvent>(args);
        game::tank_react(id, event);
      }
      else if (cmd == "update")
      {
        auto [id, zone] = ser::deserialize<size_t, map::Zone>(args);
        auto beg = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> ml(g::mainloop_mtx);
        //std::lock_guard<std::mutex> dl(g::drawing_mtx);
        std::set<map::Pos> changes;
        for (auto& r : g::userdata[id].map_changes)
        {
          if (zone.contains(r))
          {
            changes.insert(r);
          }
        }
        auto d = std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::steady_clock::now() - beg);

        std::vector<msg::Message> msgs;
        for (auto it = g::userdata[id].messages.rbegin(); it < g::userdata[id].messages.rend(); ++it)
        {
          if (!it->read)
          {
            msgs.emplace_back(*it);
            it->read = true;
          }
          else // New messages are at the begin of the std::vector, so just break.
            break;
        }

        res.set_content(make_response(d.count(), drawing::extract_userinfo(),
                                      changes, drawing::extract_tanks(),
                                      msgs, drawing::extract_map(zone)));
        g::userdata[id].map_changes.clear();
        g::userdata[id].last_update = std::chrono::steady_clock::now();
      }
      else if (cmd == "register")
      {
        std::lock_guard<std::mutex> ml(g::mainloop_mtx);
        std::lock_guard<std::mutex> dl(g::drawing_mtx);
        auto id = game::add_tank();
        g::userdata[id] = g::UserData{
          .user_id = id,
          .ip = req.get_addr().ip()
        };
        g::userdata[id].last_update = std::chrono::steady_clock::now();
        g::userdata[id].active = true;
        msg::info(-1, req.get_addr().ip() + " registered as " + std::to_string(id));
        res.set_content(make_response(id));
        if (g::curr_page == g::Page::STATUS)
          g::output_inited = false;
      }
      else if (cmd == "deregister")
      {
        auto id = ser::deserialize<size_t>(args);
        std::lock_guard<std::mutex> ml(g::mainloop_mtx);
        std::lock_guard<std::mutex> dl(g::drawing_mtx);
        msg::info(-1, req.get_addr().ip() + " (" + std::to_string(id) + ") deregistered.");
        g::tanks[id]->kill();
        g::tanks[id]->clear();
        delete g::tanks[id];
        g::tanks.erase(id);
        g::userdata.erase(id);
      }
      else if (cmd == "login")
      {
        auto id = ser::deserialize<size_t>(args);
        std::lock_guard<std::mutex> ml(g::mainloop_mtx);
        std::lock_guard<std::mutex> dl(g::drawing_mtx);
        auto tank = game::id_at(id);
        if (tank == nullptr || tank->is_auto())
        {
          res.set_content(make_response(-1, "No such user."));
          return;
        }
        else if (tank->is_alive())
        {
          res.set_content(make_response(-1, "Already logined."));
          return;
        }
        msg::info(-1, req.get_addr().ip() + " (" + std::to_string(id) + ") logined.");
        game::revive(id);
        g::userdata[id].last_update = std::chrono::steady_clock::now();
        g::userdata[id].active = true;
        res.set_content(make_response(0, "Success."));
      }
      else if (cmd == "logout")
      {
        auto id = ser::deserialize<size_t>(args);
        std::lock_guard<std::mutex> ml(g::mainloop_mtx);
        std::lock_guard<std::mutex> dl(g::drawing_mtx);
        msg::info(-1, req.get_addr().ip() + " (" + std::to_string(id) + ") logout.");
        g::tanks[id]->kill();
        g::tanks[id]->clear();
        g::userdata[id].active = false;
      }
      else if (cmd == "add_auto_tank")
      {
        auto [id, zone, lvl] = ser::deserialize<size_t, map::Zone, size_t>(args);
        std::lock_guard<std::mutex> ml(g::mainloop_mtx);
        std::lock_guard<std::mutex> dl(g::drawing_mtx);
        game::add_auto_tank(lvl, zone, id);
      }
      else if (cmd == "run_command")
      {
        auto [id, command] = ser::deserialize<size_t, std::string>(args);
        cmd::run_command(id, command);
      }
    });
  }

  void TankServer::start(int port_)
  {
    port = port_;
    std::thread th{
      [this] { svr->start(port); }
    };
    th.detach();
  }

  void TankServer::stop()
  {
    svr->stop();
    delete svr;
    svr = nullptr;
  }

  TankServer::~TankServer()
  {
    delete svr;
  }

  int TankServer::get_port() const
  {
    return port;
  }


  std::optional<size_t> TankClient::connect(const std::string& addr_, int port_)
  {
    std::lock_guard<std::mutex> l(g::online_mtx);
    host = addr_;
    port = port_;
    if (cli->connect(addr_, port_) != 0)
    {
      cli->reset();
      return std::nullopt;
    }

    std::string content = make_request("register");
    auto ret = cli->send_and_recv(content);
    if (!ret.has_value())
    {
      cli->reset();
      return std::nullopt;
    }
    auto id = ser::deserialize<size_t>(*ret);
    return id;
  }

  int TankClient::reconnect(const std::string& addr_, int port_, size_t id)
  {
    std::lock_guard<std::mutex> l(g::online_mtx);
    host = addr_;
    port = port_;
    if (cli->connect(addr_, port_) != 0)
    {
      cli->reset();
      return -1;
    }

    std::string content = make_request("login", id);
    auto res = cli->send_and_recv(content);
    if (!res.has_value())
    {
      cli->reset();
      return -1;
    }
    auto [i, msg] = ser::deserialize<int, std::string>(*res);
    if (i != 0)
    {
      msg::error(g::user_id, msg);
      return -1;
    }
    return 0;
  }

  void TankClient::disconnect()
  {
    std::lock_guard<std::mutex> l(g::online_mtx);
    std::string content = make_request("logout", g::user_id);
    if (cli->send(content) < 0)
      msg::error(g::user_id, strerror(errno));
    if (cli->disconnect() < 0)
      msg::error(g::user_id, strerror(errno));
    cli->reset();
    delete cli;
    cli = nullptr;
  }

  int TankClient::tank_react(tank::NormalTankEvent e) const
  {
    std::lock_guard<std::mutex> l(g::online_mtx);
    std::string content = make_request("tank_react", g::user_id, e);
    return cli->send(content);
  }

  int TankClient::update() const
  {
    std::lock_guard<std::mutex> l(g::online_mtx);
    auto beg = std::chrono::steady_clock::now();
    std::string content = make_request("update", g::user_id, g::visible_zone.bigger_zone(10));
    auto ret = cli->send_and_recv(content);
    if (!ret.has_value())
    {
      g::delay = -1;
      g::output_inited = false;
      return -1;
    }
    int delay;
    auto old_seed = g::snapshot.map.seed;
    std::vector<msg::Message> msgs;
    std::tie(delay, g::snapshot.userinfo, g::snapshot.changes, g::snapshot.tanks, msgs, g::snapshot.map)
        = ser::deserialize<
          decltype(delay),
          decltype(g::snapshot.userinfo),
          decltype(g::snapshot.changes),
          decltype(g::snapshot.tanks),
          decltype(msgs),
          decltype(g::snapshot.map)>(*ret);
    int curr_delay = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>
                       (std::chrono::steady_clock::now() - beg).count()) - delay;
    g::delay = static_cast<int>((g::delay + 0.1 * curr_delay) / 1.1);

    for (auto& r : msgs) // reverse
      g::userdata[g::user_id].messages.emplace_back(r);

    if (old_seed != g::snapshot.map.seed)
      g::output_inited = false;
    return 0;
  }

  int TankClient::add_auto_tank(size_t lvl) const
  {
    std::lock_guard<std::mutex> l(g::online_mtx);
    std::string content = make_request("add_auto_tank", g::user_id, g::visible_zone, lvl);
    return cli->send(content);
  }

  int TankClient::run_command(const std::string& str) const
  {
    std::lock_guard<std::mutex> l(g::online_mtx);
    std::string content = make_request("run_command", g::user_id, str);
    return cli->send(content);
  }

  TankClient::~TankClient()
  {
    delete cli;
  }

  void TankClient::init()
  {
    if (cli != nullptr)
    {
      if (cli->disconnect() < 0)
        msg::error(g::user_id, strerror(errno));
      delete cli;
    }
    cli = new TCPClient{};
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
