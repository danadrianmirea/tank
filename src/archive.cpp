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

#include "tank/tank.h"
#include "tank/bullet.h"
#include "tank/game_map.h"
#include "tank/input.h"
#include "tank/archive.h"
#include "tank/utils/utils.h"
#include "tank/utils/debug.h"

namespace czh::ar
{
  bullet::Bullet* Archiver::load_bullet(const BulletArchive& data)
  {
    auto ret = new bullet::Bullet(data.id, data.from_tank_id, data.pos,
                                  data.direction, data.hp, data.lethality, data.range);
    return ret;
  }

  BulletArchive Archiver::archive_bullet(const bullet::Bullet* b)
  {
    return BulletArchive
    {
      .id = b->id,
      .from_tank_id = b->from_tank_id,
      .pos = b->pos,
      .direction = b->direction,
      .hp = b->hp,
      .lethality = b->lethality,
      .range = b->range
    };
  }

  tank::Tank* Archiver::load_tank(const TankArchive& data)
  {
    if (data.is_auto)
    {
      auto ret = new tank::AutoTank(data.id, data.name, data.max_hp, data.pos, data.gap,
                                    data.bullet_hp, data.bullet_lethality, data.bullet_range);
      ret->hp = data.hp;
      ret->direction = data.direction;
      ret->hascleared = data.hascleared;
      ret->target_id = data.target_id;
      ret->route = data.route;
      ret->route_pos = data.route_pos;
      ret->gap_count = data.gap_count;
      ret->has_good_target = data.has_good_target;
      return ret;
    }
    else
    {
      auto ret = new tank::NormalTank(data.id, data.name, data.max_hp, data.pos,
                                      data.bullet_hp, data.bullet_lethality, data.bullet_range);
      ret->hp = data.hp;
      ret->direction = data.direction;
      ret->hascleared = data.hascleared;
      return ret;
    }
    return nullptr;
  }

  TankArchive Archiver::archive_tank(const tank::Tank* t)
  {
    TankArchive ret
    {
      .id = t->id,
      .hascleared = t->hascleared,
      .name = t->name,
      .max_hp = t->max_hp,
      .hp = t->hp,
      .is_auto = t->is_auto,
      .pos = t->pos,
      .direction = t->direction,
      .bullet_hp = t->bullet_hp,
      .bullet_lethality = t->bullet_lethality,
      .bullet_range = t->bullet_range
    };

    if (t->is_auto)
    {
      ret.is_auto = true;
      auto tank = dynamic_cast<const tank::AutoTank*>(t);
      ret.gap = tank->gap;
      ret.target_id = tank->target_id;
      ret.route = tank->route;
      ret.route_pos = tank->route_pos;
      ret.gap_count = tank->gap_count;
      ret.has_good_target = tank->has_good_target;
    }
    return ret;
  }

  map::Map Archiver::load_map(const MapArchive& archive,
                              const std::map<size_t, tank::Tank*>& tanks, const std::list<bullet::Bullet*>& bullets)
  {
    map::Map ret;
    for (const auto& r : archive.map)
    {
      const auto& pa = r.second;
      map::Point p;
      p.generated = pa.generated;
      p.temporary = pa.temporary;
      p.statuses = pa.statuses;

      if (pa.has_tank)
      {
        p.tank = tanks.at(pa.tank);
        dbg::tank_assert(p.tank != nullptr);
      }

      for (auto& b : pa.bullets)
      {
        for (auto& x : bullets)
        {
          if (x->get_id() == b)
          {
            p.bullets.emplace_back(x);
            break;
          }
        }
      }
      ret.map[r.first] = p;
    }
    ret.seed = archive.seed;
    return ret;
  }

  MapArchive Archiver::archive_map(const map::Map& map)
  {
    MapArchive ret;
    for (const auto& r : map.map)
    {
      const auto& point = r.second;
      PointArchive pa
      {
        .generated = point.generated,
        .temporary = point.temporary,
        .statuses = point.statuses
      };
      if (point.tank != nullptr)
      {
        pa.has_tank = true;
        pa.tank = point.tank->get_id();
      }
      else
        pa.has_tank = false;

      for (auto& b : point.bullets)
        pa.bullets.emplace_back(b->get_id());

      ret.map[r.first] = pa;
    }
    ret.seed = map.seed;
    return ret;
  }

  Archive archive()
  {
    Archive ret{
      // game state
      .users = g::state.users,
      .user_id = g::state.id,
      .next_id = g::state.next_id,

      // draw state
      .focus = draw::state.focus,
      .style = draw::state.style,

      // map
      .game_map = Archiver::archive_map(map::map),

      // input state
      .history = input::state.history,

      // config
      .config = cfg::config
    };

    for (const auto& r : g::state.tanks | std::views::values)
      ret.tanks.emplace_back(Archiver::archive_tank(r));

    for (const auto& r : g::state.bullets)
      ret.bullets.emplace_back(Archiver::archive_bullet(r));
    return ret;
  }

  void load(const Archive& archive)
  {
    // game state
    g::state.users = archive.users;
    g::state.id = archive.user_id;
    g::state.next_id = archive.next_id;

    // draw state
    draw::state.focus = archive.focus;
    draw::state.style = archive.style;

    // map
    Archiver::archive_map(map::map) = archive.game_map;

    // input state
    input::state.history = archive.history;

    // config
    cfg::config = archive.config;

    g::state.tanks.clear();
    for (const auto& r : archive.tanks)
    {
      g::state.tanks[r.id] = Archiver::load_tank(r);
    }

    g::state.bullets.clear();
    for (const auto& r : archive.bullets)
    {
      g::state.bullets.emplace_back(Archiver::load_bullet(r));
    }

    map::map = Archiver::load_map(archive.game_map, g::state.tanks, g::state.bullets);
  }
}
