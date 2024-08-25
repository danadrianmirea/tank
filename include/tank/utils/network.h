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
#ifndef TANK_NETWORK_H
#define TANK_NETWORK_H
#pragma once

#ifdef _WIN32

#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")
#else

#include "debug.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#endif

#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <future>
#include <memory>

namespace czh::utils
{
  constexpr uint32_t HEADER_MAGIC = 0x18273645;
  constexpr uint32_t SHUTDOWN_MAGIC = HEADER_MAGIC + 6;
  constexpr uint16_t PROTOCOL_VERSION = 2;

#ifdef _WIN32
  WSADATA wsa_data;
  [[maybe_unused]] int wsa_startup_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  class Thpool
  {
  private:
    using Task = std::function<void()>;
    std::vector<std::thread> pool;
    std::queue<Task> tasks;
    std::atomic<bool> running;
    std::mutex thpool_mtx;
    std::condition_variable cond;

  public:
    explicit Thpool(size_t size) : running(true) { add_thread(size); }

    ~Thpool()
    {
      running = false;
      cond.notify_all();
      for (auto& th : pool)
      {
        if (th.joinable())
          th.join();
      }
    }

    void add_task(const std::function<void()>& func)
    {
      dbg::tank_assert(running, "Can not add task on stopped Thpool");
      //
      {
        std::lock_guard lock(thpool_mtx);
        tasks.emplace([func] { func(); });
      }
      cond.notify_one();
    }

    void add_thread(std::size_t num)
    {
      for (std::size_t i = 0; i < num; i++)
      {
        pool.emplace_back(
          [this]
          {
            while (running)
            {
              Task task;
              //
              {
                std::unique_lock lock(thpool_mtx);
                cond.wait(lock, [this] { return !running || !tasks.empty(); });
                if (!running) return;
                if (tasks.empty()) continue;
                task = std::move(tasks.front());
                tasks.pop();
              }
              task();
            }
          }
        );
      }
    }
  };

  struct MsgHeader
  {
    uint32_t magic;
    uint16_t version;
    uint32_t content_length;
  };

#ifdef _WIN32
  using Socket_t = SOCKET;
#else
  using Socket_t = int;
#endif

  inline int receive_all(Socket_t sock, char* buf, ssize_t size)
  {
    while (size > 0)
    {
      auto r = ::recv(sock, buf, size, MSG_WAITALL);
      if (r <= 0) return -1;
      size -= r;
      buf += r;
    }
    return 0;
  }

  inline int send_all(Socket_t sock, const char* buf, size_t size)
  {
    while (size > 0)
    {
#ifdef _WIN32
      int s = ::send(sock, buf, size, 0);
#else
      int s = static_cast<int>(::send(sock, buf, size, MSG_NOSIGNAL));
#endif
      if (s < 0) return -1;
      size -= s;
      buf += s;
    }
    return 0;
  }

  inline int send_packet(Socket_t sock, const std::string& content)
  {
    MsgHeader header{
      .magic = htonl(HEADER_MAGIC),
      .version = htons(PROTOCOL_VERSION),
      .content_length = htonl(content.size())
    };
    if (send_all(sock, reinterpret_cast<const char*>(&header), sizeof(MsgHeader)) != 0)
      return -1;
    if (send_all(sock, content.data(), static_cast<int>(content.size())) != 0)
      return -1;
    return 0;
  }

  inline int send_shutdown_packet(Socket_t sock)
  {
    MsgHeader header{
      .magic = htonl(SHUTDOWN_MAGIC),
      .version = htons(PROTOCOL_VERSION),
      .content_length = htonl(0)
    };
    if (send_all(sock, reinterpret_cast<const char*>(&header), sizeof(MsgHeader)) != 0)
      return -1;
    return 0;
  }

  inline std::optional<std::string> get_peer_ip(Socket_t fd)
  {
    struct sockaddr_in peer_addr{};
#ifdef _WIN32
    int peer_len;
#else
    socklen_t peer_len;
#endif
    peer_len = sizeof(peer_addr);
    if (getpeername(fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_len) != 0)
      return std::nullopt;

#ifdef _WIN32
    std::string str(16, '\0');
    str = inet_ntoa(peer_addr.sin_addr);
#else
    char buf[16];
    inet_ntop(AF_INET, &peer_addr.sin_addr, buf, peer_len);
    std::string str{buf};
#endif
    return str;
  }

  enum class RecvRet
  {
    shutdown = -3,
    invalid = -2,
    failed = -1,
    ok = 0
  };

  inline std::tuple<RecvRet, std::string> receive_packet(Socket_t sock)
  {
    MsgHeader header{};
    if (receive_all(sock, reinterpret_cast<char*>(&header), sizeof(MsgHeader)) != 0)
      return {RecvRet::failed, ""};

    if (ntohs(header.version) != PROTOCOL_VERSION)
      return {RecvRet::invalid, ""};

    if (ntohl(header.magic) != HEADER_MAGIC)
    {
      if (ntohl(header.magic) == SHUTDOWN_MAGIC)
        return {RecvRet::shutdown, ""};
      else
        return {RecvRet::invalid, ""};
    }

    std::string ret(ntohl(header.content_length), '\0');

    if (receive_all(sock, reinterpret_cast<char*>(ret.data()), static_cast<int>(ret.size())) != 0)
      return {RecvRet::failed, ""};

    return {RecvRet::ok, ret};
  }

  inline void tank_close(Socket_t fd)
  {
#ifdef _WIN32
    closesocket(fd);
#else
    ::close(fd);
#endif
  }

  inline void tank_shutdown(Socket_t fd)
  {
#ifdef _WIN32
    shutdown(fd, SD_BOTH);
#else
    ::shutdown(fd, SHUT_RDWR);
#endif
  }

  class TCPServer
  {
  private:
    bool running;
    Thpool pool;
    Socket_t listening_socket;
    std::set<Socket_t> free_sockets;
    std::mutex free_mtx;
    std::function<std::string(Socket_t, const std::string&)> router;
    std::function<void(Socket_t)> on_closed_unexpectedly;
    std::function<void(Socket_t)> on_closed;

  public:
    explicit TCPServer(const std::function<std::string(Socket_t, const std::string&)>& router_,
                       const std::function<void(Socket_t)>& on_closed_,
                       const std::function<void(Socket_t)>& on_closed_unexpectedly_)
      : running(true), pool(8), listening_socket(-1), router(router_),
        on_closed(on_closed_), on_closed_unexpectedly(on_closed_unexpectedly_)
    {
    }

    void stop() { running = false; }

    void bind_and_listen(int port)
    {
      listening_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (listening_socket < 0)
        throw std::runtime_error(std::format("socket(): {}", strerror(errno)));
      int on = 1;
      setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
      setsockopt(listening_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(port);

      if (::bind(listening_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
      {
        tank_close(listening_socket);
        throw std::runtime_error(std::format("bind(): {}", strerror(errno)));
      }

      if (::listen(listening_socket, 5) != 0)
      {
        tank_close(listening_socket);
        throw std::runtime_error(std::format("listen(): {}", strerror(errno)));
      }
    }

    void start()
    {
      free_sockets.insert(listening_socket);
      timeval timeout{};
      timeout.tv_sec = 0;
      timeout.tv_usec = 100;
      while (true)
      {
        fd_set sockets;
        FD_ZERO(&sockets);
        int maxfd;
        //
        {
          std::lock_guard l(free_mtx);
          for (const auto& r : free_sockets)
            FD_SET(r, &sockets);
          maxfd = *std::prev(std::end(free_sockets));
        }
        int in_socket = ::select(maxfd + 1, &sockets,NULL,NULL, &timeout);
        if (in_socket < 0)
        {
          tank_close(listening_socket);
          throw std::runtime_error(std::format("select(): {}", strerror(errno)));
        }
        else if (in_socket > 0)
        {
          for (int eventfd = 0; eventfd <= maxfd; ++eventfd)
          {
            if (FD_ISSET(eventfd, &sockets))
            {
              if (eventfd == listening_socket)
              {
                sockaddr_in client_addr{};
                socklen_t addrlen = sizeof(client_addr);
                int client_socket = ::accept(listening_socket, reinterpret_cast<struct sockaddr*>(&client_addr),
                                             &addrlen);
                if (client_socket < 0)
                  continue;

                int keepalive = 1;
                int keepidle = 3;
                int keepcnt = 3;
                int keepintvl = 1;
                setsockopt(client_socket, SOL_SOCKET,SO_KEEPALIVE, &keepalive, sizeof(keepalive));
                setsockopt(client_socket, SOL_TCP,TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
                setsockopt(client_socket, SOL_TCP,TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
                setsockopt(client_socket, SOL_TCP,TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));

                //
                {
                  std::lock_guard l(free_mtx);
                  free_sockets.emplace(client_socket);
                }
              }
              else
              {
                char buf[1];
                //
                {
                  std::lock_guard l(free_mtx);
                  free_sockets.erase(eventfd);
                }
                if (auto ret = ::recv(eventfd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
                  (ret < 0 && errno != EAGAIN) || ret == 0)
                {
                  on_closed_unexpectedly(eventfd);
                  tank_close(eventfd);
                }
                else
                {
                  pool.add_task([this, eventfd]
                  {
                    MsgHeader header{};
                    auto [err, content] = receive_packet(eventfd);
                    if (err == RecvRet::ok)
                    {
                      auto res = router(eventfd, content);
                      if (!res.empty())
                        send_packet(eventfd, res);
                      //
                      {
                        std::lock_guard l(free_mtx);
                        free_sockets.emplace(eventfd);
                      }
                    }
                    else if (err == RecvRet::failed)
                    {
                      on_closed_unexpectedly(eventfd);
                      tank_close(eventfd);
                    }
                    else if (err == RecvRet::shutdown)
                    {
                      on_closed(eventfd);
                      tank_shutdown(eventfd);
                      tank_close(eventfd);
                    }
                  });
                }
              }
            }
          }
        }
        if (!running) break;
      } // while

      {
        std::lock_guard l(free_mtx);
        for (const Socket_t& fd : free_sockets)
        {
          if (fd == listening_socket)
            tank_close(listening_socket);
          else
          {
            send_shutdown_packet(fd);
            tank_shutdown(fd);
            tank_close(fd);
          }
        }
        free_sockets.clear();
      }
    }
  };

  class TCPClient
  {
  private:
    Socket_t sock;

  public:
    TCPClient() = default;

    void init()
    {
      sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (sock < 0)
        throw std::runtime_error(std::format("socket(): {}", strerror(errno)));
      int on = 1;
      setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof(on));
      setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on));
    }

    [[nodiscard]] int connect(const std::string& ip, int port) const
    {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = inet_addr(ip.c_str());
      addr.sin_port = htons(port);
      return ::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    }

    ~TCPClient()
    {
      disconnect();
    }

    void disconnect() const
    {
      send_shutdown_packet(sock);
      tank_shutdown(sock);
      tank_close(sock);
    }

    [[nodiscard]] int send(const std::string& str) const
    {
      return send_packet(sock, str);
    }

    [[nodiscard]] std::tuple<RecvRet, std::string> recv() const
    {
      auto ret = receive_packet(sock);
      auto [err, content] = ret;
      if (err == RecvRet::shutdown || err == RecvRet::failed)
      {
        tank_shutdown(sock);
        tank_close(sock);
      }
      return ret;
    }

    [[nodiscard]] std::tuple<RecvRet, std::string> send_and_recv(const std::string& str) const
    {
      int ret = send(str);
      if (ret != 0)
        return {RecvRet::failed, ""};
      return recv();
    }
  };
}
#endif
