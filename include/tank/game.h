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

#include "tank.h"
#include <utility>
#include <optional>

#include "globals.h"

namespace czh::game
{
  std::optional<map::Pos> get_available_pos(const map::Zone& zone);
  
  tank::Tank *id_at(size_t id);
  
  void revive(std::size_t id, const map::Zone& zone = g::visible_zone, size_t from_id = g::user_id);
  
  std::size_t add_auto_tank(std::size_t lvl, const map::Zone& zone = g::visible_zone, size_t from_id = g::user_id);
  
  std::size_t add_auto_tank(std::size_t lvl, const map::Pos &pos, size_t from_id = g::user_id);
  
  std::size_t add_tank(const map::Pos &pos, size_t from_id = g::user_id);
  
  std::size_t add_tank(const map::Zone& zone = g::visible_zone, size_t from_id = g::user_id);
  
  void clear_death();
  
  void mainloop();
  
  void tank_react(std::size_t id, tank::NormalTankEvent event);
  
  void quit();
}
#endif