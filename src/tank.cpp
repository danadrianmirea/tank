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
#include <variant>

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
    g::bullets.emplace_back(
      new bullet::Bullet(info.bullet, info.id, get_pos(), get_direction()));
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

  [[nodiscard]] int Node::get_F(const map::Pos& dest) const
  {
    return G + static_cast<int>(get_distance(dest, pos)) * 10;
  }

  int& Node::get_G()
  {
    return G;
  }

  map::Pos& Node::get_last()
  {
    return last;
  }

  [[nodiscard]] const map::Pos& Node::get_pos() const
  {
    return pos;
  }

  [[nodiscard]] bool Node::is_root() const
  {
    return root;
  }

  std::vector<Node> Node::get_neighbors() const
  {
    if (G + 10 > 100) return {};
    std::vector<Node> ret;

    map::Pos pos_up(pos.x, pos.y + 1);
    map::Pos pos_down(pos.x, pos.y - 1);
    map::Pos pos_left(pos.x - 1, pos.y);
    map::Pos pos_right(pos.x + 1, pos.y);
    if (check(pos_up))
    {
      ret.emplace_back(pos_up, G + 10, pos);
    }
    if (check(pos_down))
    {
      ret.emplace_back(pos_down, G + 10, pos);
    }
    if (check(pos_left))
    {
      ret.emplace_back(pos_left, G + 10, pos);
    }
    if (check(pos_right))
    {
      ret.emplace_back(pos_right, G + 10, pos);
    }
    return ret;
  }

  bool Node::check(const map::Pos& pos)
  {
    return !g::game_map.has(map::Status::WALL, pos) && !g::game_map.has(map::Status::TANK, pos);
  }

  bool operator<(const Node& n1, const Node& n2)
  {
    return n1.get_pos() < n2.get_pos();
  }

  bool is_fire_spot(int range, const map::Pos& pos, const map::Pos& target_pos)
  {
    if (pos == target_pos) return false;
    int x = target_pos.x - pos.x;
    int y = target_pos.y - pos.y;
    if (x == 0 && std::abs(y) > 0 && std::abs(y) < range)
    {
      int a = y > 0 ? pos.y : target_pos.y;
      int b = y < 0 ? pos.y : target_pos.y;
      for (int i = a + 1; i < b; ++i)
      {
        map::Pos tmp = {pos.x, i};
        if (g::game_map.has(map::Status::WALL, tmp)
            || g::game_map.has(map::Status::TANK, tmp))
        {
          return false;
        }
      }
    }
    else if (y == 0 && std::abs(x) > 0 && std::abs(x) < range)
    {
      int a = x > 0 ? pos.x : target_pos.x;
      int b = x < 0 ? pos.x : target_pos.x;
      for (int i = a + 1; i < b; ++i)
      {
        map::Pos tmp = {i, pos.y};
        if (g::game_map.has(map::Status::WALL, tmp)
            || g::game_map.has(map::Status::TANK, tmp))
        {
          return false;
        }
      }
    }
    else
    {
      return false;
    }
    return true;
  }

  int AutoTank::find_route()
  {
    auto target_pos = game::id_at(target_id)->get_pos();
    std::multimap<int, Node> open;
    std::map<map::Pos, Node> close;
    // fire_line
    std::set<map::Pos> fire_spots;
    // X
    for (int i = target_pos.x - info.bullet.range; i <= target_pos.x + info.bullet.range; ++i)
    {
      map::Pos tmp(i, target_pos.y);
      if (is_fire_spot(info.bullet.range, tmp, target_pos))
      {
        if (tmp.x < target_pos.x)
        {
          for (int j = i; j < target_pos.x; ++j)
            fire_spots.insert(map::Pos{j, target_pos.y});
          i = target_pos.x;
        }
        else
        {
          for (int j = target_pos.x + 1; j < target_pos.x + info.bullet.range; ++j)
            fire_spots.insert(map::Pos{j, target_pos.y});
          break;
        }
      }
    }

    // Y
    for (int i = target_pos.y - info.bullet.range; i <= target_pos.y + info.bullet.range; ++i)
    {
      map::Pos tmp(target_pos.x, i);
      if (is_fire_spot(info.bullet.range, tmp, target_pos))
      {
        if (tmp.y < target_pos.y)
        {
          for (int j = i; j < target_pos.y; ++j)
            fire_spots.insert(map::Pos{target_pos.x, j});
          i = target_pos.y;
        }
        else
        {
          for (int j = target_pos.y + 1; j < target_pos.y + info.bullet.range; ++j)
            fire_spots.insert(map::Pos{target_pos.x, j});
          break;
        }
      }
    }

    if (fire_spots.empty()) return -1;
    auto dest = *std::min_element(fire_spots.begin(), fire_spots.end(),
                                             [this](auto&& a, auto&& b)
                                             {
                                               return map::get_distance(a, pos) < map::get_distance(b, pos);
                                             });

    Node beg(get_pos(), 0, {0, 0}, true);
    open.insert({beg.get_F(dest), beg});
    while (!open.empty())
    {
      auto it = open.begin();
      auto curr = close.insert({it->second.get_pos(), it->second});
      open.erase(it);
      auto neighbors = curr.first->second.get_neighbors();
      for (auto& node : neighbors)
      {
        if (close.contains(node.get_pos()))
          continue;
        auto oit = std::find_if(open.begin(), open.end(),
                                [&node](auto&& p)
                                {
                                  return p.second.get_pos() == node.get_pos();
                                });
        if (oit == open.end())
        {
          open.insert({node.get_F(dest), node});
        }
        else
        {
          if (oit->second.get_G() > node.get_G() + 10) // less G
          {
            oit->second.get_G() = node.get_G() + 10;
            oit->second.get_last() = node.get_pos();
            int F = oit->second.get_F(dest);
            auto n = open.extract(oit);
            n.key() = F;
            open.insert(std::move(n));
          }
        }
      }
      auto itt = std::find_if(open.begin(), open.end(),
                              [&fire_spots](auto&& p) -> bool
                              {
                                return fire_spots.contains(p.second.get_pos());
                              });
      if (itt != open.end()) //found
      {
        route.clear();
        route_pos = 0;
        auto& np = itt->second;
        while (!np.is_root() && np.get_pos() != np.get_last())
        {
          route.insert(route.begin(), get_pos_direction(close[np.get_last()].get_pos(), np.get_pos()));
          np = close[np.get_last()];
        }
        return 0;
      }
    }
    return -1;
  }

  void AutoTank::set_target(std::size_t id)
  {
    target_id = id;
    has_good_target = find_route() == 0;
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
    auto check = [this](map::Pos p)
    {
      return !g::game_map.has(map::Status::WALL, p) && !g::game_map.has(map::Status::TANK, p);
    };
    auto p = pos;
    int i = 0;
    while (route.size() < 10 && i++ < 10)
    {
      map::Pos pos_up(p.x, p.y + 1);
      map::Pos pos_down(p.x, p.y - 1);
      map::Pos pos_left(p.x - 1, p.y);
      map::Pos pos_right(p.x + 1, p.y);
      switch (utils::randnum<int>(0, 4))
      {
        case 0:
          if (check(pos_up))
          {
            p = pos_up;
            route.insert(route.end(), 5, AutoTankEvent::UP);
          }
          break;
        case 1:
          if (check(pos_down))
          {
            p = pos_down;
            route.insert(route.end(), 5, AutoTankEvent::DOWN);
          }
          break;
        case 2:
          if (check(pos_left))
          {
            p = pos_left;
            route.insert(route.end(), 5, AutoTankEvent::LEFT);
          }
          break;
        case 3:
          if (check(pos_right))
          {
            p = pos_right;
            route.insert(route.end(), 5, AutoTankEvent::RIGHT);
          }
          break;
      }
    }
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
                          && is_fire_spot(info.bullet.range, pos, target_ptr->get_pos());
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
                has_good_target = true;
              else
                continue;
              break;
            }
          }
        }
      }
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
      switch (w)
      {
        case AutoTankEvent::UP:
          up();
          break;
        case AutoTankEvent::DOWN:
          down();
          break;
        case AutoTankEvent::LEFT:
          left();
          break;
        case AutoTankEvent::RIGHT:
          right();
          break;
        default:
          break;
      }
    }
  }

  Tank* build_tank(const TankData& data)
  {
    if (data.is_auto())
    {
      auto ret = new AutoTank(data.info, data.pos);
      ret->hp = data.hp;
      ret->direction = data.direction;
      ret->hascleared = data.hascleared;

      auto& d = std::get<AutoTankData>(data.data);
      ret->target_id = d.target_id;

      ret->route = d.route;
      ret->route_pos = d.route_pos;

      ret->gap_count = d.gap_count;
      ret->has_good_target = d.has_good_target;
      return ret;
    }
    else
    {
      auto ret = new NormalTank(data.info, data.pos);
      ret->hp = data.hp;
      ret->direction = data.direction;
      ret->hascleared = data.hascleared;
      //auto& d = std::get<map::NormalTankData>(data.data);
      return ret;
    }
    return nullptr;
  }

  TankData get_tank_data(const Tank* t)
  {
    TankData ret;
    ret.info = t->info;
    ret.pos = t->pos;
    ret.hp = t->hp;
    ret.direction = t->direction;
    ret.hascleared = t->hascleared;
    if (t->is_auto())
    {
      auto tank = dynamic_cast<const AutoTank*>(t);
      AutoTankData data;

      data.target_id = tank->target_id;

      data.route = tank->route;
      data.route_pos = tank->route_pos;

      data.gap_count = tank->gap_count;
      data.has_good_target = tank->has_good_target;
      ret.data.emplace<AutoTankData>(data);
    }
    else
    {
      // auto tank = dynamic_cast<tank::NormalTank *>(t);
      NormalTankData data;
      ret.data.emplace<NormalTankData>(data);
    }
    return ret;
  }
}
