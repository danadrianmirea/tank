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
#ifndef TANK_ARCHIVE_H
#define TANK_ARCHIVE_H
#pragma once

#include "tank.h"
#include "bullet.h"
#include "game_map.h"
#include "drawing.h"
#include "game.h"
#include "config.h"

#include <chrono>

namespace czh::ar
{
  struct TankArchive
  {
    size_t id{0};
    bool hascleared{false};
    std::string name;
    int max_hp{0};
    int hp{0};
    bool is_auto{false};
    map::Pos pos;
    map::Direction direction{map::Direction::END};
    int bullet_hp{0};
    int bullet_lethality{0};
    int bullet_range{0};

    // auto only
    int gap{0};
    std::size_t target_id{0};
    std::vector<tank::AutoTankEvent> route;
    size_t route_pos{0};
    int gap_count{0};
    bool has_good_target{false};
  };

  struct BulletArchive
  {
    size_t id{};
    size_t from_tank_id{};
    map::Pos pos;
    map::Direction direction{map::Direction::END};
    int hp{};
    int lethality{};
    int range{};
  };

  struct PointArchive
  {
    bool generated;
    bool temporary;
    std::vector<map::Status> statuses;

    bool has_tank{false};
    size_t tank;
    std::vector<size_t> bullets;
  };

  struct MapArchive
  {
    std::map<map::Pos, PointArchive> map;
    unsigned long long seed;
  };

  struct Archive
  {
    // game state
    std::map<size_t, g::UserData> users;
    size_t user_id;
    size_t next_id;
    std::vector<TankArchive> tanks;
    std::vector<BulletArchive> bullets;

    // draw state
    size_t focus;
    draw::Style style;

    // map
    MapArchive game_map;

    // input state
    std::vector<std::string> history;

    // config
    cfg::Config config;
  };

  Archive archive();

  void load(const Archive&);

  class Archiver
  {
    friend Archive archive();

    friend void load(const Archive&);

  private:
    static map::Map load_map(const MapArchive& archive,
                             const std::map<size_t, tank::Tank*>& tanks, const std::list<bullet::Bullet*>& bullets);

    static MapArchive archive_map(const map::Map&);

    static tank::Tank* load_tank(const TankArchive& data);

    static TankArchive archive_tank(const tank::Tank*);

    static bullet::Bullet* load_bullet(const BulletArchive& data);

    static BulletArchive archive_bullet(const bullet::Bullet*);
  };
}
#endif
