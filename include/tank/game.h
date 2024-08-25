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
#ifndef TANK_GAME_H
#define TANK_GAME_H
#pragma once

#include "game_map.h"
#include "message.h"
#include "tank.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <set>
#include <utility>

namespace czh::g
{
  struct UserData
  {
    size_t user_id{0};
    std::set<map::Pos> map_changes;
    std::vector<msg::Message> messages;
    std::chrono::steady_clock::time_point last_update;
    std::string ip;
    bool active{false};

    // for some commands
    map::Zone visible_zone;
  };

  enum class Mode
  {
    NATIVE,
    SERVER,
    CLIENT,
  };

  enum class Page
  {
    GAME,
    STATUS,
    MAIN,
    HELP,
    NOTIFICATION
  };

  struct GameState
  {
    std::atomic<bool> running; // Tank Command: pause continue
    std::atomic<bool> suspend; // CTRL-Z
    Mode mode;
    Page page;
    std::map<size_t, UserData> users;
    size_t id;
    size_t next_id;
    size_t next_bullet_id;
    std::map<std::size_t, tank::Tank *> tanks;
    std::list<bullet::Bullet *> bullets;
    std::vector<std::pair<std::size_t, tank::NormalTankEvent>> events;
  };

  extern GameState state;
  extern std::mutex mainloop_mtx;
  extern std::mutex tank_reacting_mtx;

  std::optional<map::Pos> get_available_pos(const map::Zone &zone);

  tank::Tank *id_at(size_t id);

  void revive(std::size_t id, const map::Zone &zone, size_t from_id);

  std::size_t add_auto_tank(std::size_t lvl, const map::Zone &zone, size_t from_id);

  std::size_t add_auto_tank(std::size_t lvl, const map::Pos &pos, size_t from_id);

  std::size_t add_tank(const map::Pos &pos, size_t from_id);

  std::size_t add_tank(const map::Zone &zone, size_t from_id);

  void clear_death();

  void mainloop();

  void tank_react(std::size_t id, tank::NormalTankEvent event);

  void quit();
} // namespace czh::g
#endif
