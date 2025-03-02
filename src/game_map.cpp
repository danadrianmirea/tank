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
#include "tank/game_map.h"
#include "tank/utils/debug.h"
#include "tank/utils/utils.h"
#include <ranges>
#include <vector>
#include "tank/game.h"

namespace czh::map
{
  Map map;
  const Point empty_point("used for empty point", {});
  const Point wall_point("used for wall point", {map::Status::WALL});


  void add_changes(const Pos &p)
  {
    for (auto &r : g::state.users | std::views::values)
      r.map_changes.insert(p);
  }

  Zone Zone::bigger_zone(int i) const { return {x_min - i, x_max + i, y_min - i, y_max + i}; }

  bool Zone::contains(int i, int j) const { return (i >= x_min && i < x_max && j >= y_min && j < y_max); }

  bool Zone::contains(const Pos &p) const { return contains(p.x, p.y); }

  bool Point::is_generated() const { return generated; }

  bool Point::is_temporary() const { return temporary; }

  bool Point::is_empty() const { return statuses.empty(); }

  tank::Tank *Point::get_tank() const
  {
    dbg::tank_assert(has(Status::TANK));
    return tank;
  }

  const std::vector<bullet::Bullet *> &Point::get_bullets() const
  {
    dbg::tank_assert(has(Status::BULLET));
    return bullets;
  }

  void Point::add_status(const Status &status, void *ptr)
  {
    statuses.emplace_back(status);
    if (ptr != nullptr)
    {
      switch (status)
      {
        case Status::BULLET:
          bullets.emplace_back(static_cast<bullet::Bullet *>(ptr));
          break;
        case Status::TANK:
          tank = static_cast<tank::Tank *>(ptr);
          break;
        default:
          break;
      }
    }
  }

  void Point::remove_status(const Status &status)
  {
    std::erase(statuses, status);
    switch (status)
    {
      case Status::BULLET:
        bullets.clear();
        break;
      case Status::TANK:
        tank = nullptr;
        break;
      default:
        break;
    }
  }

  void Point::remove_all_statuses()
  {
    statuses.clear();
    bullets.clear();
    tank = nullptr;
  }

  [[nodiscard]] bool Point::has(const Status &status) const
  {
    return std::ranges::find(statuses, status) != statuses.end();
  }

  [[nodiscard]] std::size_t Point::count(const Status &status) const { return std::ranges::count(statuses, status); }

  bool Pos::operator==(const Pos &pos) const { return (x == pos.x && y == pos.y); }

  bool Pos::operator!=(const Pos &pos) const { return !(*this == pos); }

  bool operator<(const Pos &pos1, const Pos &pos2)
  {
    if (pos1.x == pos2.x)
    {
      return pos1.y < pos2.y;
    }
    return pos1.x < pos2.x;
  }

  std::size_t get_distance(const Pos &from, const Pos &to) { return std::abs(from.x - to.x) + std::abs(from.y - to.y); }

  Map::Map() : seed(utils::randnum<unsigned long long>(1, 20)) {}

  int Map::tank_up(const Pos &pos) { return tank_move(pos, 0); }

  int Map::tank_down(const Pos &pos) { return tank_move(pos, 1); }

  int Map::tank_left(const Pos &pos) { return tank_move(pos, 2); }

  int Map::tank_right(const Pos &pos) { return tank_move(pos, 3); }

  int Map::bullet_up(bullet::Bullet *b, const Pos &pos) { return bullet_move(b, pos, 0); }

  int Map::bullet_down(bullet::Bullet *b, const Pos &pos) { return bullet_move(b, pos, 1); }

  int Map::bullet_left(bullet::Bullet *b, const Pos &pos) { return bullet_move(b, pos, 2); }

  int Map::bullet_right(bullet::Bullet *b, const Pos &pos) { return bullet_move(b, pos, 3); }


  int Map::add_tank(tank::Tank *t, const Pos &pos)
  {
    map[pos].add_status(Status::TANK, t);
    add_changes(pos);
    return 0;
  }

  int Map::add_bullet(bullet::Bullet *b, const Pos &pos)
  {
    auto &p = map[pos];
    if (p.has(Status::WALL))
      return -1;
    p.add_status(Status::BULLET, b);
    add_changes(pos);
    return 0;
  }

  void Map::remove_status(const Status &status, const Pos &pos)
  {
    map[pos].remove_status(status);
    if (map[pos].is_temporary() && map[pos].is_empty())
    {
      map.erase(pos);
    }
    add_changes(pos);
  }

  bool Map::has(const Status &status, const Pos &pos) const { return at(pos).has(status); }

  size_t Map::count(const Status &status, const Pos &pos) const { return at(pos).count(status); }


  const Point &generate(const Pos &i, size_t seed)
  {
    constexpr int magic = 9;

    // divide the map for quicker route finding
    if (i.x == 0 || i.y == 0 || i.x % MAP_DIVISION == 0 || i.y % MAP_DIVISION == 0)
      return empty_point;

    int a = i.x * (i.y / magic);
    if (a < 0)
      a = -a * 2;
    if (seed * a % 37 == 1)
      return wall_point;

    a = (i.x / magic) * i.y;
    if (a < 0)
      a = -a * 2;
    if (seed * a % 37 == 1)
      return wall_point;

    return empty_point;
  }

  const Point &generate(int x, int y, size_t seed) { return generate(Pos(x, y), seed); }

  const Point &Map::at(int x, int y) const { return at(Pos(x, y)); }

  const Point &Map::at(const Pos &i) const
  {
    if (map.contains(i))
    {
      return map.at(i);
    }
    return generate(i, seed);
  }

  int Map::fill(const Zone &zone, const Status &status)
  {
    for (int i = zone.x_min; i < zone.x_max; ++i)
    {
      for (int j = zone.y_min; j < zone.y_max; ++j)
      {
        Pos p(i, j);
        map[p].remove_all_statuses();
        if (status != Status::END)
        {
          map[p].add_status(status, nullptr);
        }
        map[p].temporary = false;
        add_changes(p);
      }
    }
    return 0;
  }

  int Map::tank_move(const Pos &pos, int direction)
  {
    Pos new_pos = pos;
    switch (direction)
    {
      case 0:
        new_pos.y++;
        break;
      case 1:
        new_pos.y--;
        break;
      case 2:
        new_pos.x--;
        break;
      case 3:
        new_pos.x++;
        break;
      default:
        break;
    }

    if (at(new_pos).has(Status::WALL))
      return -1;

    auto &new_point = map[new_pos];
    auto &old_point = map[pos];

    if (new_point.has(Status::TANK))
      return -1;
    new_point.add_status(Status::TANK, old_point.tank);
    old_point.remove_status(Status::TANK);
    if (old_point.is_temporary() && old_point.is_empty())
    {
      map.erase(pos);
    }
    add_changes(pos);
    add_changes(new_pos);
    return 0;
  }

  int Map::bullet_move(bullet::Bullet *b, const Pos &pos, int direction)
  {
    Pos new_pos = pos;
    switch (direction)
    {
      case 0:
        new_pos.y++;
        break;
      case 1:
        new_pos.y--;
        break;
      case 2:
        new_pos.x--;
        break;
      case 3:
        new_pos.x++;
        break;
      default:
        break;
    }

    if (at(new_pos).has(Status::WALL))
      return -1;

    auto &new_point = map[new_pos];
    auto &old_point = map[pos];
    bool ok = false;
    for (auto it = old_point.bullets.begin(); it != old_point.bullets.end();)
    {
      if (*it == b)
      {
        it = old_point.bullets.erase(it);
        ok = true;
        break;
      }
      else
      {
        ++it;
      }
    }
    dbg::tank_assert(ok);
    for (auto it = old_point.statuses.begin(); it != old_point.statuses.end();)
    {
      if (*it == Status::BULLET)
      {
        it = old_point.statuses.erase(it);
        break;
      }
      else
      {
        ++it;
      }
    }
    new_point.add_status(Status::BULLET, b);

    if (old_point.is_temporary() && old_point.is_empty())
    {
      map.erase(pos);
    }
    add_changes(pos);
    add_changes(new_pos);
    return 0;
  }
} // namespace czh::map
