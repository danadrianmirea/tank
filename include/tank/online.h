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
#ifndef TANK_ONLINE_H
#define TANK_ONLINE_H
#pragma once

#include "tank.h"
#include "broadcast.h"
#include "utils/network.h"

#include <string>
#include <mutex>
#include <optional>

namespace czh::online
{
  class TankServer
  {
  private:
    utils::TCPServer* svr{};
    std::thread th;
    int port{};
    //UDPSocket* udp;
  public:
    TankServer() = default;

    ~TankServer();

    void start(int);

    void stop();

    [[nodiscard]] int get_port() const;

    void reset();
  };

  class TankClient
  {
  private:
    std::string host;
    int port{0};
    utils::TCPClient* cli{nullptr};
    //UDPSocket* udp;
  public:
    TankClient() = default;

    ~TankClient();

    std::optional<size_t> signup(const std::string& addr_, int port_);

    int login(const std::string& addr_, int port_, size_t id);

    void logout();

    [[nodiscard]] int tank_react(tank::NormalTankEvent e);

    [[nodiscard]] int update();

    [[nodiscard]] int add_auto_tank(size_t l);

    [[nodiscard]] int run_command(const std::string& str);

    [[nodiscard]] int get_port() const;

    [[nodiscard]] std::string get_host() const;

  private:
    void cli_failed(bool shutdown = false);
  };

  struct OnlineState
  {
    std::string error;
    int delay; // ms
  };

  extern OnlineState state;
  extern TankServer svr;
  extern TankClient cli;
  extern std::mutex online_mtx;
}
#endif
