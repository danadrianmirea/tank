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
#include "bullet.h"
#include <utility>
#include <variant>
#include <functional>

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
    UP, DOWN, LEFT, RIGHT, FIRE, PASS
  };

  class Tank
  {
    friend class archive::Archiver;
  protected:
    info::TankInfo info;
    int hp;
    map::Pos pos;
    map::Direction direction;
    bool hascleared;

  public:
    Tank(const info::TankInfo& info_, map::Pos pos_);

    virtual ~Tank() = default;

    void kill();

    int up();

    int down();

    int left();

    int right();

    int fire();

    [[nodiscard]] bool is_auto() const;

    [[nodiscard]] std::size_t get_id() const;

    std::string& get_name();

    [[nodiscard]] const std::string& get_name() const;

    [[nodiscard]] int get_hp() const;

    [[nodiscard]] int& get_hp();

    [[nodiscard]] int get_max_hp() const;

    [[nodiscard]] bool is_alive() const;

    [[nodiscard]] bool has_cleared() const;

    void clear();

    map::Pos& get_pos();

    virtual void attacked(int lethality_);

    [[nodiscard]] const map::Pos& get_pos() const;

    [[nodiscard]] map::Direction& get_direction();

    [[nodiscard]] const map::Direction& get_direction() const;

    [[nodiscard]] info::TankType get_type() const;

    [[nodiscard]] const info::TankInfo& get_info() const;

    [[nodiscard]] info::TankInfo& get_info();

    void revive(const map::Pos& newpos);
  };

  class NormalTank : public Tank
  {
    friend class archive::Archiver;
  private:
    NormalTankEvent auto_event;
    bool auto_driving;

  public:
    NormalTank(const info::TankInfo& info_, map::Pos pos_)
      : Tank(info_, pos_), auto_event(NormalTankEvent::UP), auto_driving(false)
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

  AutoTankEvent get_pos_direction(const map::Pos& from, const map::Pos& to);

  struct Node
  {
    map::Pos pos;
    map::Pos dest;
    map::Pos last;
    int G;
    int F;

    [[nodiscard]] std::vector<Node> get_neighbors() const;
  private:
    Node make_next(const map::Pos& p) const;
  };

  bool operator<(const Node& n1, const Node& n2);

  bool is_fire_spot(int range, const map::Pos& pos, const map::Pos& target_pos, bool curr_at_pos);

  std::vector<map::Pos> find_route_between(map::Pos src, map::Pos dest,
    const std::function<bool(const map::Pos&)>& pred);

  class AutoTank : public Tank
  {
    friend class archive::Archiver;
  private:
    std::size_t target_id;

    std::vector<AutoTankEvent> route;
    std::size_t route_pos;

    int gap_count;

    bool has_good_target;

  public:
    AutoTank(const info::TankInfo& info_, map::Pos pos_)
      : Tank(info_, pos_), target_id(0), route_pos(0), gap_count(0), has_good_target(false)
    {}

    ~AutoTank() override = default;

    int set_target(std::size_t id);

    bool is_target_good() const;

    size_t get_target_id() const;

    void react();

    void attacked(int lethality_) override;

  private:
    void generate_random_route();

    [[nodiscard]]int find_route();
  };
}
#endif
