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
#include "tank/game.h"
#include "tank/tank.h"
#include "tank/game_map.h"
#include "tank/globals.h"
#include "tank/bullet.h"
#include "tank/utils.h"
#include <map>
#include <set>
#include <list>
#include <functional>

namespace czh::g
{
  size_t next_bullet_id = 0;
}

namespace czh::tank
{
  Tank::Tank(const info::TankInfo& info_, map::Pos pos_)
    : info(info_), hp(info_.max_hp), pos(pos_), direction(map::Direction::UP),
      hascleared(false)
  {
    g::game_map.add_tank(this, pos);
  }

  void Tank::kill()
  {
    attacked(hp);
  }

  int Tank::up()
  {
    direction = map::Direction::UP;
    int ret = g::game_map.tank_up(pos);
    if (ret == 0)
    {
      pos.y++;
    }
    return ret;
  }

  int Tank::down()
  {
    direction = map::Direction::DOWN;
    int ret = g::game_map.tank_down(pos);
    if (ret == 0)
    {
      pos.y--;
    }
    return ret;
  }

  int Tank::left()
  {
    direction = map::Direction::LEFT;
    int ret = g::game_map.tank_left(pos);
    if (ret == 0)
    {
      pos.x--;
    }
    return ret;
  }

  int Tank::right()
  {
    direction = map::Direction::RIGHT;
    int ret = g::game_map.tank_right(pos);
    if (ret == 0)
    {
      pos.x++;
    }
    return ret;
  }

  int Tank::fire()
  {
    auto i = info.bullet;
    i.id = g::next_bullet_id++;
    g::bullets.emplace_back(
      new bullet::Bullet(i, info.id, get_pos(), get_direction()));
    int ret = g::game_map.add_bullet(g::bullets.back(), get_pos());
    return ret;
  }

  [[nodiscard]] bool Tank::is_auto() const
  {
    return info.type == info::TankType::AUTO;
  }

  [[nodiscard]] std::size_t Tank::get_id() const
  {
    return info.id;
  }

  std::string& Tank::get_name()
  {
    return info.name;
  }

  const std::string& Tank::get_name() const
  {
    return info.name;
  }

  [[nodiscard]] int Tank::get_hp() const { return hp; }

  [[nodiscard]] int& Tank::get_hp() { return hp; }

  [[nodiscard]] int Tank::get_max_hp() const { return info.max_hp; }

  [[nodiscard]] const info::TankInfo& Tank::get_info() const { return info; }

  [[nodiscard]] info::TankInfo& Tank::get_info() { return info; }

  [[nodiscard]] bool Tank::is_alive() const
  {
    return hp > 0;
  }

  [[nodiscard]] bool Tank::has_cleared() const
  {
    return hascleared;
  }

  void Tank::clear()
  {
    g::game_map.remove_status(map::Status::TANK, get_pos());
    hascleared = true;
  }

  map::Pos& Tank::get_pos()
  {
    return pos;
  }

  void Tank::attacked(int lethality_)
  {
    hp -= lethality_;
    if (hp < 0) hp = 0;
    if (hp > info.max_hp) hp = info.max_hp;
  }

  [[nodiscard]] const map::Pos& Tank::get_pos() const
  {
    return pos;
  }

  [[nodiscard]] map::Direction& Tank::get_direction()
  {
    return direction;
  }

  [[nodiscard]] const map::Direction& Tank::get_direction() const
  {
    return direction;
  }

  [[nodiscard]] info::TankType Tank::get_type() const
  {
    return info.type;
  }

  void Tank::revive(const map::Pos& newpos)
  {
    hp = info.max_hp;
    if (is_alive() && !hascleared) return;
    hascleared = false;
    pos = newpos;
    g::game_map.add_tank(this, pos);
  }

  AutoTankEvent get_pos_direction(const map::Pos& from, const map::Pos& to)
  {
    int x = from.x - to.x;
    int y = from.y - to.y;
    if (x > 0)
    {
      return AutoTankEvent::LEFT;
    }
    else if (x < 0)
    {
      return AutoTankEvent::RIGHT;
    }
    else if (y > 0)
    {
      return AutoTankEvent::DOWN;
    }
    return AutoTankEvent::UP;
  }

  Node Node::make_next(const map::Pos& p) const
  {
    return Node{
      .pos = p,
      .dest = dest,
      .last = pos,
      .G = G + 10,
      .F = G + 10 + static_cast<int>(get_distance(dest, p)) * 10
    };
  }

  std::vector<Node> Node::get_neighbors() const
  {
    if (G > map::MAP_DIVISION * 20) return {};

    static auto check = [](const map::Pos& p)
    {
      return !g::game_map.has(map::Status::WALL, p);
    };
    std::vector<Node> ret;

    map::Pos pos_up(pos.x, pos.y + 1);
    map::Pos pos_down(pos.x, pos.y - 1);
    map::Pos pos_left(pos.x - 1, pos.y);
    map::Pos pos_right(pos.x + 1, pos.y);
    if (check(pos_up))
    {
      ret.emplace_back(make_next(pos_up));
    }
    if (check(pos_down))
    {
      ret.emplace_back(make_next(pos_down));
    }
    if (check(pos_left))
    {
      ret.emplace_back(make_next(pos_left));
    }
    if (check(pos_right))
    {
      ret.emplace_back(make_next(pos_right));
    }
    return ret;
  }


  bool operator<(const Node& n1, const Node& n2)
  {
    return n1.pos < n2.pos;
  }

  bool is_fire_spot(int range, const map::Pos& pos, const map::Pos& target_pos, bool curr_at_pos)
  {
    if (pos == target_pos) return false;
    if (g::game_map.has(map::Status::WALL, pos) || (!curr_at_pos && g::game_map.has(map::Status::TANK, pos)))
      return false;
    int x = target_pos.x - pos.x;
    int y = target_pos.y - pos.y;
    if (x == 0 && std::abs(y) > 0 && std::abs(y) < range)
    {
      int a = y > 0 ? pos.y : target_pos.y;
      int b = y < 0 ? pos.y : target_pos.y;
      for (int i = a + 1; i < b; ++i)
      {
        map::Pos tmp = {pos.x, i};
        if (g::game_map.has(map::Status::WALL, tmp) || g::game_map.has(map::Status::TANK, tmp))
          return false;
      }
    }
    else if (y == 0 && std::abs(x) > 0 && std::abs(x) < range)
    {
      int a = x > 0 ? pos.x : target_pos.x;
      int b = x < 0 ? pos.x : target_pos.x;
      for (int i = a + 1; i < b; ++i)
      {
        map::Pos tmp = {i, pos.y};
        if (g::game_map.has(map::Status::WALL, tmp) || g::game_map.has(map::Status::TANK, tmp))
          return false;
      }
    }
    else
      return false;
    return true;
  }

  // void debug_mark_point(const map::Pos& pos, const std::string& str, int c)
  // {
  //   term::move_cursor({
  //     static_cast<size_t>((pos.x - g::visible_zone.x_min) * 2),
  //     static_cast<size_t>(g::visible_zone.y_max - pos.y - 1)
  //   });
  //   term::output(utils::color_256_bg(str, c));
  //   term::flush();
  // }

  std::vector<map::Pos> find_route_between(map::Pos src, map::Pos dest,
                                           const std::function<bool(const map::Pos&)>& pred)
  {
    std::vector<map::Pos> ret;
    std::multimap<int, Node> open;
    std::map<map::Pos, Node> close;

    Node beg{
      .pos = src, .dest = dest, .last = src,
      .G = 0, .F = 0 + static_cast<int>(get_distance(dest, src)) * 10
    };

    open.insert({beg.F, beg});
    while (!open.empty())
    {
      auto it = open.begin();
      auto curr = close.insert({it->second.pos, it->second}).first->second;
      open.erase(it);

      for (auto neighbors = curr.get_neighbors(); auto& node : neighbors)
      {
        if (close.contains(node.pos))
          continue;
        auto oit = std::find_if(open.begin(), open.end(),
                                [&node](auto&& p) { return p.second.pos == node.pos; });
        if (oit == open.end())
        {
          open.insert({node.F, node});
        }
        else
        {
          if (int a = oit->second.G - (node.G + 10); a > 0) // less G
          {
            oit->second.G = node.G + 10;
            oit->second.last = node.pos;
            int F = oit->second.F - a;
            auto n = open.extract(oit);
            n.key() = F;
            open.insert(std::move(n));
          }
        }
      }
      auto result = std::find_if(open.begin(), open.end(),
                                 [&pred](auto&& r)
                                 {
                                   return pred(r.second.pos);
                                 });
      if (result != open.end())
      {
        auto np = result->second;
        while (np.pos != np.last)
        {
          ret.emplace_back(np.pos);
          np = close[np.last];
        }
        ret.emplace_back(np.pos);
        return ret;
      }
    }
    return ret;
  }


  int AutoTank::find_route()
  {
    auto target_pos = game::id_at(target_id)->get_pos();

    std::set<map::Pos> fire_spots;
    // X
    for (int i = target_pos.x - info.bullet.range; i <= target_pos.x + info.bullet.range; ++i)
    {
      map::Pos tmp(i, target_pos.y);
      if (is_fire_spot(info.bullet.range, tmp, target_pos, false))
      {
        if (tmp.x < target_pos.x)
        {
          for (; i < target_pos.x; ++i)
            fire_spots.insert(map::Pos{i, target_pos.y});
        }
        else
          fire_spots.insert(map::Pos{i, target_pos.y});
      }
      else
      {
        if (tmp.x > target_pos.x)
          break;
      }
    }

    // Y
    for (int i = target_pos.y - info.bullet.range; i <= target_pos.y + info.bullet.range; ++i)
    {
      map::Pos tmp(target_pos.x, i);
      // if spot A is in left and is good, then the spots between A and the dest is good.
      if (is_fire_spot(info.bullet.range, tmp, target_pos, false))
      {
        if (tmp.y < target_pos.y)
        {
          for (; i < target_pos.y; ++i)
            fire_spots.insert(map::Pos{target_pos.x, i});
        }
        else
          fire_spots.insert(map::Pos{target_pos.x, i});
      }
      // if spot A is in right and is bad, then the spots between A and the dest is bad.
      else
      {
        if (tmp.y > target_pos.y)
          break;
      }
    }

    if (fire_spots.empty()) return -1;

    auto dest = *std::min_element(fire_spots.begin(), fire_spots.end(),
                                  [this](auto&& a, auto&& b)
                                  {
                                    return map::get_distance(a, pos) < map::get_distance(b, pos);
                                  });

    route.clear();
    route_pos = 0;

    auto add_route = [this](const std::vector<map::Pos>& r)
    {
      for (int i = static_cast<int>(r.size() - 2); i >= 0; --i)
        route.emplace_back(get_pos_direction(r[i + 1], r[i]));
    };

    // See the division of map in game_map::generate()
    if (std::abs(dest.x - pos.x) > map::MAP_DIVISION || std::abs(dest.y - pos.y) > map::MAP_DIVISION)
    {
      auto transit_src = pos;
      auto transit_dest = dest;
      if (transit_dest.x > transit_src.x)
      {
        while (transit_src.x % map::MAP_DIVISION != 0)
          ++transit_src.x;

        while (transit_dest.x % map::MAP_DIVISION != 0)
          --transit_dest.x;
      }
      else
      {
        while (transit_src.x % map::MAP_DIVISION != 0)
          --transit_src.x;
        while (transit_dest.x % map::MAP_DIVISION != 0)
          ++transit_dest.x;
      }

      if (transit_dest.y > transit_src.y)
      {
        while (transit_src.y % map::MAP_DIVISION != 0)
          ++transit_src.y;
        while (transit_dest.y % map::MAP_DIVISION != 0)
          --transit_dest.y;
      }
      else
      {
        while (transit_src.y % map::MAP_DIVISION != 0)
          --transit_src.y;
        while (transit_dest.y % map::MAP_DIVISION != 0)
          ++transit_dest.y;
      }

      auto t1 = find_route_between(pos, transit_src,
                                   [&transit_src](const map::Pos& p) { return p == transit_src; });
      auto t2 = find_route_between(transit_dest, dest,
                                   [&dest](const map::Pos& p) { return p == dest; });
      if (t1.size() < 2 || t2.size() < 2) return -1;
      add_route(t1);

      if (transit_dest.x > transit_src.x)
      {
        for (int i = transit_src.x; i < transit_dest.x; ++i)
          route.emplace_back(AutoTankEvent::RIGHT);
      }
      else
      {
        for (int i = transit_dest.x; i < transit_src.x; ++i)
          route.emplace_back(AutoTankEvent::LEFT);
      }

      if (transit_dest.y > transit_src.y)
      {
        for (int i = transit_src.y; i < transit_dest.y; ++i)
          route.emplace_back(AutoTankEvent::UP);
      }
      else
      {
        for (int i = transit_dest.y; i < transit_src.y; ++i)
          route.emplace_back(AutoTankEvent::DOWN);
      }

      add_route(t2);
      return 0;
    }
    else
    {
      auto r = find_route_between(pos, dest,
                                  [&fire_spots](const map::Pos& p) { return fire_spots.contains(p); });
      if (r.size() < 2) return -1;
      add_route(r);
      return 0;
    }
    return -1;
  }

  int AutoTank::set_target(std::size_t id)
  {
    target_id = id;
    auto ret = find_route();
    has_good_target = ret == 0;
    return ret;
  }

  bool AutoTank::is_target_good() const
  {
    return has_good_target;
  }

  size_t AutoTank::get_target_id() const
  {
    return target_id;
  }

  void AutoTank::generate_random_route()
  {
    route.clear();
    route_pos = 0;
    auto check = [this](map::Pos from, map::Pos to)
    {
      if (from.x == to.x)
      {
        if (from.y > to.y)
          std::swap(from, to);
        for (int i = from.y; i <= to.y; ++i)
        {
          map::Pos p{from.x, i};
          if (g::game_map.has(map::Status::WALL, p) || g::game_map.has(map::Status::TANK, p))
            return false;
        }
      }
      else if (from.y == to.y)
      {
        if (from.x > to.x)
          std::swap(from, to);
        for (int i = from.x; i <= to.x; ++i)
        {
          map::Pos p{i, from.y};
          if (g::game_map.has(map::Status::WALL, p) || g::game_map.has(map::Status::TANK, p))
            return false;
        }
      }
      return true;
    };
    AutoTankEvent e = AutoTankEvent::END;
    int sz = 7;
    while (sz >= 1)
    {
      std::vector<AutoTankEvent> avail;
      if (check({pos.x, pos.y + 1}, {pos.x, pos.y + sz}))
        avail.emplace_back(AutoTankEvent::UP);
      if (check({pos.x, pos.y - 1}, {pos.x, pos.y - sz}))
        avail.emplace_back(AutoTankEvent::DOWN);
      if (check({pos.x - 1, pos.y}, {pos.x - sz, pos.y}))
        avail.emplace_back(AutoTankEvent::LEFT);
      if (check({pos.x + 1, pos.y}, {pos.x + sz, pos.y}))
        avail.emplace_back(AutoTankEvent::RIGHT);
      if (avail.empty())
      {
        --sz;
        continue;
      }
      else if (avail.size() == 1)
      {
        e = avail[0];
        break;
      }
      else
      {
        e = avail[utils::randnum<size_t>(0, avail.size())];
        break;
      }
    }
    if (e != AutoTankEvent::END)
      route.insert(route.end(), sz, e);
  }

  void AutoTank::attacked(int lethality_)
  {
    Tank::attacked(lethality_);
    generate_random_route();
  }

  void AutoTank::react()
  {
    if (++gap_count < info.gap) return;
    gap_count = 0;

    auto target_ptr = game::id_at(target_id);
    bool good_fire_spot = target_ptr != nullptr && target_ptr->is_alive()
                          && is_fire_spot(info.bullet.range, pos, target_ptr->get_pos(), true);
    has_good_target = good_fire_spot;
    // If arrived and not in good spot, then find route.
    if (route_pos >= route.size() && !good_fire_spot)
    {
      has_good_target = false;
      for (int i = get_pos().x - 15; i < get_pos().x + 15; ++i)
      {
        for (int j = get_pos().y - 15; j < get_pos().y + 15; ++j)
        {
          if (i == get_pos().x && j == get_pos().y) continue;

          if (g::game_map.at(i, j).has(map::Status::TANK))
          {
            auto t = g::game_map.at(i, j).get_tank();
            utils::tank_assert(t != nullptr);
            if (t->is_alive())
            {
              target_id = t->get_id();
              if (find_route() == 0)
              {
                has_good_target = true;
                goto find_a_target;
              }
            }
          }
        }
      }
    find_a_target:
      if (route_pos >= route.size()) // still no route
      {
        generate_random_route();
        has_good_target = false;
      }
    }

    if (good_fire_spot)
    {
      // no need to move
      gap_count = info.gap - 5;
      route_pos = 0;
      route.clear();
      // correct direction
      int x = get_pos().x - target_ptr->get_pos().x;
      int y = get_pos().y - target_ptr->get_pos().y;
      if (x > 0)
      {
        direction = map::Direction::LEFT;
      }
      else if (x < 0)
      {
        direction = map::Direction::RIGHT;
      }
      else if (y < 0)
      {
        direction = map::Direction::UP;
      }
      else if (y > 0)
      {
        direction = map::Direction::DOWN;
      }
      fire();
    }
    else
    {
      if (route_pos >= route.size()) return;
      auto w = route[route_pos++];
      int ret = -1;
      switch (w)
      {
        case AutoTankEvent::UP:
          ret = up();
          break;
        case AutoTankEvent::DOWN:
          ret = down();
          break;
        case AutoTankEvent::LEFT:
          ret = left();
          break;
        case AutoTankEvent::RIGHT:
          ret = right();
          break;
        default:
          break;
      }
      if (ret != 0)
      {
        --route_pos;
        fire();
      }
    }
  }
}
