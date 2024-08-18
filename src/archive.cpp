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
#include "tank/archive.h"

#include <tank/utils.h>

#include "tank/globals.h"

namespace czh::archive
{
  bullet::Bullet* Archiver::load_bullet(const BulletArchive& data)
  {
    auto ret = new bullet::Bullet(data.info, data.from_tank_id, data.pos, data.direction);
    return ret;
  }

  BulletArchive Archiver::archive_bullet(const bullet::Bullet* b)
  {
    return BulletArchive
    {
      .pos = b->pos,
      .direction = b->direction,
      .from_tank_id = b->from_tank_id,
      .info = b->info
    };
  }

  tank::Tank* Archiver::load_tank(const TankArchive& data)
  {
    if (data.is_auto)
    {
      auto ret = new tank::AutoTank(data.info, data.pos);
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
      auto ret = new tank::NormalTank(data.info, data.pos);
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
      .info = t->info,
      .hp = t->hp,
      .pos = t->pos,
      .direction = t->direction,
      .hascleared = t->hascleared
    };

    if (t->is_auto())
    {
      ret.is_auto = true;
      auto tank = dynamic_cast<const tank::AutoTank*>(t);
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
    for (auto& r : archive.map)
    {
      const auto& pa = r.second;
      map::Point p;
      p.generated = pa.generated;
      p.temporary = pa.temporary;
      p.statuses = pa.statuses;

      if(pa.has_tank)
      {
        p.tank = tanks.at(pa.tank);
        utils::tank_assert(p.tank != nullptr);
      }

      for (auto& b : pa.bullets)
      {
        for (auto& x : bullets)
        {
          if (x->get_id() == b)
          {
            p.bullets.emplace_back(x);
            utils::tank_assert(x != nullptr);
            break;
          }
        }
      }
      ret.map[r.first] = p;
    }
    return ret;
  }

  MapArchive Archiver::archive_map(const map::Map& map)
  {
    MapArchive ret;
    for (auto& r : map.map)
    {
      const auto& point = r.second;
      PointArchive pa
      {
        .generated = point.generated,
        .temporary = point.temporary,
        .statuses = point.statuses
      };
      if(point.tank != nullptr)
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
    return ret;
  }

  Archive archive()
  {
    Archive ret{
      .user_id = g::user_id,
      .next_id = g::next_id,
      .tick = g::tick,
      .msg_ttl = g::msg_ttl,
      .game_map = Archiver::archive_map(g::game_map),
      .seed = g::seed,
      .history = g::history,
      .long_pressing_threshold = g::long_pressing_threshold,
      .unsafe_mode = g::unsafe_mode,
      .tank_focus = g::tank_focus,
      .style = g::style
    };

    for (auto& r : g::tanks)
      ret.tanks.emplace_back(Archiver::archive_tank(r.second));

    for (auto& r : g::bullets)
      ret.bullets.emplace_back(Archiver::archive_bullet(r));
    return ret;
  }

  void load(const Archive& archive)
  {
    g::user_id = archive.user_id;
    g::next_id = archive.next_id;
    g::tick = archive.tick;
    g::msg_ttl = archive.msg_ttl;
    g::seed = archive.seed;
    g::history = archive.history;
    g::long_pressing_threshold = archive.long_pressing_threshold;
    g::unsafe_mode = archive.unsafe_mode;
    g::tank_focus = archive.tank_focus;
    g::style = archive.style;

    g::tanks.clear();
    for (auto& r : archive.tanks)
    {
      g::tanks[r.info.id] = Archiver::load_tank(r);
    }

    g::bullets.clear();
    for (auto& r : archive.bullets)
    {
      g::bullets.emplace_back(Archiver::load_bullet(r));
    }

    g::game_map = Archiver::load_map(archive.game_map, g::tanks, g::bullets);
  }
}
