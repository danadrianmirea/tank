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
#ifndef TANK_TANK_H
#define TANK_TANK_H
#pragma once

#include "game_map.h"
#include <utility>
#include <functional>

namespace czh::ar
{
  class Archiver;
}

namespace czh::tank
{
  enum class NormalTankEvent
  {
    UP, DOWN, LEFT, RIGHT, FIRE,
    UP_AUTO, DOWN_AUTO, LEFT_AUTO, RIGHT_AUTO, FIRE_AUTO,
    AUTO_OFF,
  };

  enum class AutoTankEvent
  {
    UP, DOWN, LEFT, RIGHT, FIRE, END
  };

  class Tank
  {
    friend class ar::Archiver;

  protected:
    size_t id;
    bool hascleared;

  public:
    bool is_auto;
    std::string name;
    int max_hp;
    int hp;
    map::Pos pos;
    map::Direction direction;

    int bullet_hp;
    int bullet_lethality;
    int bullet_range;

  public:
    Tank(bool is_auto_, size_t id_, std::string name_, int max_hp_, map::Pos pos_,
         int bullet_hp_, int bullet_lethality_, int bullet_range_)
      : id(id_), hascleared(false), is_auto(is_auto_),
        name(std::move(name_)), max_hp(max_hp_), pos(pos_),
        direction(map::Direction::UP), hp(max_hp_),
        bullet_hp(bullet_hp_), bullet_lethality(bullet_lethality_), bullet_range(bullet_range_)
    {
      map::map.add_tank(this, pos);
    }

    virtual ~Tank() = default;

    void kill();

    int up();

    int down();

    int left();

    int right();

    int fire() const;

    [[nodiscard]] std::size_t get_id() const;

    [[nodiscard]] bool is_alive() const;

    [[nodiscard]] bool has_cleared() const;

    void clear();

    virtual void attacked(int lethality_);

    void revive(const map::Pos& newpos);
  };

  class NormalTank : public Tank
  {
    friend class ar::Archiver;

  private:
    NormalTankEvent auto_event;
    bool auto_driving;

  public:
    NormalTank(size_t id_, std::string name_, int max_hp_, map::Pos pos_,
               int bullet_hp_, int bullet_lethality_, int bullet_range_)
      : Tank(false, id_, std::move(name_), max_hp_, pos_, bullet_hp_, bullet_lethality_, bullet_range_),
        auto_event(NormalTankEvent::UP), auto_driving(false)
    {
    }

    ~NormalTank() override = default;

    void start_auto_drive(NormalTankEvent e)
    {
      auto_event = e;
      auto_driving = true;
    }

    void stop_auto_drive()
    {
      auto_driving = false;
    }

    [[nodiscard]] NormalTankEvent get_auto_event() const
    {
      return auto_event;
    }

    [[nodiscard]] bool is_auto_driving() const
    {
      return auto_driving;
    }
  };

  struct Node
  {
    map::Pos pos;
    map::Pos dest;
    map::Pos last;
    int G{0};
    int F{0};

    [[nodiscard]] std::vector<Node> get_neighbors() const;

  private:
    [[nodiscard]] Node make_next(const map::Pos& p) const;
  };

  bool operator<(const Node& n1, const Node& n2);

  class AutoTank : public Tank
  {
    friend class ar::Archiver;

  public:
    int gap;

  private:
    std::size_t target_id;
    std::vector<AutoTankEvent> route;
    std::size_t route_pos;
    int gap_count;
    bool has_good_target;

  public:
    AutoTank(size_t id_, std::string name_, int max_hp_, map::Pos pos_, int gap_,
             int bullet_hp_, int bullet_lethality_, int bullet_range_)
      : Tank(true, id_, std::move(name_), max_hp_, pos_, bullet_hp_, bullet_lethality_, bullet_range_),
        gap(gap_), target_id(0), route_pos(0), gap_count(0), has_good_target(false)
    {
    }

    ~AutoTank() override = default;

    [[nodiscard]] int set_target(std::size_t id);

    [[nodiscard]] bool is_target_good() const;

    [[nodiscard]] size_t get_target_id() const;

    void react();

    void attacked(int lethality_) override;

  private:
    void generate_random_route();

    [[nodiscard]] int find_route();
  };
}
#endif
