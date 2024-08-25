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
#include "tank/game_map.h"
#include "tank/tank.h"
#include "tank/bullet.h"
#include "tank/broadcast.h"
#include "tank/utils/utils.h"
#include "tank/utils/debug.h"
#include <optional>
#include <mutex>
#include <vector>
#include <list>
#include <ranges>
#include <tank/drawing.h>
#include <tank/online.h>

namespace czh::g
{
  GameState state
  {
    .running = true,
    .suspend = false,
    .mode = Mode::NATIVE,
    .page = Page::MAIN,
    .users = {{0, g::UserData{.user_id = 0, .active = true}}},
    .id = 0,
    .next_id = 0,
    .next_bullet_id = 0
  };
  std::mutex mainloop_mtx;
  std::mutex tank_reacting_mtx;

  std::optional<map::Pos> get_available_pos(const map::Zone& zone)
  {
    std::vector<map::Pos> p;
    for (int i = zone.x_min; i < zone.x_max; ++i)
    {
      for (int j = zone.y_min; j < zone.y_max; ++j)
      {
        if (!map::map.has(map::Status::WALL, {i, j}) && !map::map.has(map::Status::TANK, {i, j}))
        {
          p.emplace_back(map::Pos{i, j});
        }
      }
    }
    if (p.empty())
    {
      return std::nullopt;
    }
    return p[utils::randnum<size_t>(0, p.size())];
  }

  tank::Tank* id_at(size_t id)
  {
    auto it = state.tanks.find(id);
    if (it == state.tanks.end()) return nullptr;
    return it->second;
  }

  std::size_t add_tank(const map::Pos& pos, size_t from_id)
  {
    if (map::map.has(map::Status::WALL, pos) || map::map.has(map::Status::TANK, pos))
    {
      bc::error(from_id, "No available space.");
      return 0;
    }

    state.tanks.insert({
      state.next_id, new tank::NormalTank(state.next_id, "Tank " + std::to_string(state.next_id),
                                       10000, pos, 1, 100, 60)
    });
    ++state.next_id;
    return state.next_id - 1;
  }

  std::size_t add_tank(const map::Zone& zone, size_t from_id)
  {
    auto pos = get_available_pos(zone);
    if (!pos.has_value())
    {
      bc::error(from_id, "No available space.");
      return 0;
    }
    return add_tank(*pos, from_id);
  }

  std::size_t add_auto_tank(std::size_t lvl, const map::Pos& pos, size_t from_id)
  {
    if (map::map.has(map::Status::WALL, pos) || map::map.has(map::Status::TANK, pos))
    {
      bc::error(from_id, "No available space.");
      return 0;
    }

    state.tanks.insert({
      state.next_id,
      new tank::AutoTank(state.next_id, "AutoTank " + std::to_string(state.next_id),
                         static_cast<int>(11 - lvl) * 150,
                         pos, static_cast<int>(10 - lvl), 1,
                         static_cast<int>(11 - lvl) * 15, 60)
    });
    ++state.next_id;
    return state.next_id - 1;
  }

  std::size_t add_auto_tank(std::size_t lvl, const map::Zone& zone, size_t from_id)
  {
    auto pos = get_available_pos(zone);
    if (!pos.has_value())
    {
      bc::error(from_id, "No available space.");
      return 0;
    }
    return add_auto_tank(lvl, *pos, from_id);
  }

  void revive(std::size_t id, const map::Zone& zone, size_t from_id)
  {
    auto pos = get_available_pos(zone);
    if (!pos.has_value())
    {
      bc::error(from_id, "No available space");
      return;
    }
    id_at(id)->revive(*pos);
    if (id == 0)
    {
      draw::state.focus = 0;
    }
  }

  [[nodiscard]] std::vector<std::size_t> get_alive()
  {
    std::vector<std::size_t> ret;
    for (std::size_t i = 0; i < state.tanks.size(); ++i)
    {
      if (state.tanks[i]->is_alive())
      {
        ret.emplace_back(i);
      }
    }
    return ret;
  }

  void clear_death()
  {
    for (auto it = state.bullets.begin(); it != state.bullets.end();)
    {
      if (!(*it)->is_alive())
      {
        map::map.remove_status(map::Status::BULLET, (*it)->pos);
        delete *it;
        it = state.bullets.erase(it);
      }
      else
      {
        ++it;
      }
    }

    for (auto& tank : state.tanks | std::views::values)
    {
      if (!tank->is_alive() && !tank->has_cleared())
        tank->clear();
    }
  }

  void tank_react(std::size_t id, tank::NormalTankEvent event)
  {
    if (!state.running) return;

    std::lock_guard l(tank_reacting_mtx);
    if (id_at(id)->is_alive())
    {
      state.events.emplace_back(id, event);
    }
  }

  void mainloop()
  {
    if (!state.running) return;

    std::lock_guard ml(mainloop_mtx);
    std::lock_guard dl(draw::drawing_mtx);

    //auto tank
    for (auto& tank : state.tanks | std::views::values)
    {
      dbg::tank_assert(tank != nullptr);
      if (tank->is_alive())
      {
        if (tank->is_auto)
          dynamic_cast<tank::AutoTank*>(tank)->react();
        else
        {
          auto n = dynamic_cast<tank::NormalTank*>(tank);
          if (n->is_auto_driving())
            state.events.emplace_back(tank->get_id(), n->get_auto_event());
        }
      }
    }

    //normal tank
    {
      std::lock_guard tl(tank_reacting_mtx);
      for (auto& r : state.events)
      {
        auto tank = dynamic_cast<tank::NormalTank*>(id_at(r.first));
        switch (r.second)
        {
          case tank::NormalTankEvent::UP:
            tank->up();
            break;
          case tank::NormalTankEvent::DOWN:
            tank->down();
            break;
          case tank::NormalTankEvent::LEFT:
            tank->left();
            break;
          case tank::NormalTankEvent::RIGHT:
            tank->right();
            break;
          case tank::NormalTankEvent::FIRE:
            tank->fire();
            break;
          case tank::NormalTankEvent::UP_AUTO:
            tank->start_auto_drive(tank::NormalTankEvent::UP);
            break;
          case tank::NormalTankEvent::DOWN_AUTO:
            tank->start_auto_drive(tank::NormalTankEvent::DOWN);
            break;
          case tank::NormalTankEvent::LEFT_AUTO:
            tank->start_auto_drive(tank::NormalTankEvent::LEFT);
            break;
          case tank::NormalTankEvent::RIGHT_AUTO:
            tank->start_auto_drive(tank::NormalTankEvent::RIGHT);
            break;
          case tank::NormalTankEvent::FIRE_AUTO:
            tank->start_auto_drive(tank::NormalTankEvent::FIRE);
            break;
          case tank::NormalTankEvent::AUTO_OFF:
            tank->stop_auto_drive();
            break;
        }
      }
      state.events.clear();
    }

    // bullet move
    for (auto& b : state.bullets)
    {
      if (b->is_alive())
        b->react();
    }

    for (auto& b : state.bullets)
    {
      if (!b->is_alive()) continue;

      if ((map::map.count(map::Status::BULLET, b->pos) > 1)
          || map::map.has(map::Status::TANK, b->pos))
      {
        int lethality = 0;
        int attacker = -1;
        auto bullets_instance = map::map.at(b->pos).get_bullets();
        dbg::tank_assert(!bullets_instance.empty());
        for (auto& bi : bullets_instance)
        {
          if (bi->is_alive())
            lethality += bi->get_lethality();
          bi->kill();
          attacker = static_cast<int>(bi->get_tank());
        }

        if (map::map.has(map::Status::TANK, b->pos))
        {
          if (auto tank = map::map.at(b->pos).get_tank(); tank != nullptr)
          {
            auto tank_attacker = id_at(attacker);
            dbg::tank_assert(tank_attacker != nullptr);
            if (tank->is_auto)
            {
              auto t = dynamic_cast<tank::AutoTank*>(tank);
              if (attacker != t->get_id())
              {
                int ret = t->set_target(attacker);
              }
            }
            tank->attacked(lethality);
            if (!tank->is_alive())
              bc::info(-1, "{} was killed by {}.", tank->name, tank_attacker->name);
          }
        }
      }
    }
    clear_death();
  }

  void quit()
  {
    for (auto it = state.tanks.begin(); it != state.tanks.end();)
    {
      delete it->second;
      it = state.tanks.erase(it);
    }
    if (state.mode == g::Mode::CLIENT)
    {
      online::cli.logout();
    }
    else if (state.mode == g::Mode::SERVER)
    {
      online::svr.stop();
    }
  }
}
