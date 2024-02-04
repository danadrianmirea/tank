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
#ifndef TANK_GLOBALS_H
#define TANK_GLOBALS_H

#include "game_map.h"
#include "game.h"
#include "tank.h"
#include <functional>
#include <string>
#include <mutex>
#include <chrono>
#include <list>

namespace czh::g
{
  // game.cpp
  extern int keyboard_mode;
  extern std::chrono::milliseconds tick;
  extern std::mutex mainloop_mtx;
  extern map::Map game_map;
  extern std::map<std::size_t, tank::Tank *> tanks;
  extern std::list<bullet::Bullet *> bullets;
  extern std::vector<std::pair<std::size_t, tank::NormalTankEvent>> normal_tank_events;
  extern game::Page curr_page;
  extern size_t help_page;
  extern size_t next_id;
  extern std::vector<std::string> history;
  extern std::string cmd_string;
  extern size_t history_pos;
  extern size_t cmd_string_pos;
  
  // renderer.cpp
  extern bool output_inited;
  extern size_t tank_focus;
  extern map::Zone render_zone;
  extern std::mutex render_mtx;
  extern std::set<map::Change> render_changes;
  extern std::size_t screen_height;
  extern std::size_t screen_width;
  
  // game_map.cpp
  extern map::Point empty_point;
  extern map::Point wall_point;
}
#endif