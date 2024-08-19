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
#include "globals.h"

#include <chrono>

namespace czh::archive
{
  struct TankArchive
  {
    info::TankInfo info;
    int hp{0};
    map::Pos pos;
    map::Direction direction{map::Direction::END};
    bool hascleared{false};

    bool is_auto{false};

    // auto only
    std::size_t target_id{0};
    std::vector<tank::AutoTankEvent> route;
    std::size_t route_pos{0};
    int gap_count{0};
    bool has_good_target{0};
  };

  struct BulletArchive
  {
    map::Pos pos;
    map::Direction direction{map::Direction::END};
    std::size_t from_tank_id{0};
    info::BulletInfo info{};
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
  };

  struct Archive
  {
    // game setting
    std::map<size_t, g::UserData> userdata;
    size_t user_id;
    size_t next_id;
    std::chrono::milliseconds tick;
    std::chrono::milliseconds msg_ttl;
    // game
    std::vector<TankArchive> tanks;
    std::vector<BulletArchive> bullets;
    MapArchive game_map;
    unsigned long long seed;
    // command
    std::vector<std::string> history;
    long long_pressing_threshold;
    bool unsafe_mode;
    // drawing
    size_t tank_focus;
    drawing::Style style;
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
