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
#ifndef TANK_BULLET_H
#define TANK_BULLET_H
#pragma once

#include "game_map.h"

namespace czh::ar
{
  class Archiver;
}

namespace czh::bullet
{
  class Bullet
  {
    friend class ar::Archiver;

  private:
    size_t id;
    size_t from_tank_id;
    map::Direction direction;
    int hp;
    int lethality;
    int range;
  public:
    map::Pos pos;

  public:
    Bullet(size_t id_, size_t from_tank_id_, map::Pos pos_, map::Direction direction_,
           int hp_, int lethality_, int range_)
      : id(id_), from_tank_id(from_tank_id_), direction(direction_),
        hp(hp_), lethality(lethality_), range(range_), pos(pos_)
    {
    }

    int react();

    std::string get_text();

    [[nodiscard]] bool is_alive() const;

    [[nodiscard]] size_t get_tank() const;

    void kill();

    [[nodiscard]] int get_lethality() const;

    [[nodiscard]] size_t get_id() const;
  };
}
#endif
