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

  struct NormalTankData
  {
  };

  struct AutoTankData
  {
    std::size_t target_id;
    std::vector<AutoTankEvent> route;
    std::size_t route_pos;
    int gap_count;
    bool has_good_target;
  };

  struct TankData
  {
    info::TankInfo info;
    int hp;
    map::Pos pos;
    map::Direction direction;
    bool hascleared;

    std::variant<NormalTankData, AutoTankData> data;

    [[nodiscard]] bool is_auto() const
    {
      return data.index() == 1;
    }
  };

  Tank* build_tank(const TankData& data);

  TankData get_tank_data(const Tank*);

  class Tank
  {
    friend Tank* build_tank(const TankData& data);

    friend TankData get_tank_data(const Tank*);

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
    friend Tank* build_tank(const TankData& data);

    friend TankData get_tank_data(const Tank*);

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

  class Node
  {
  private:
    map::Pos pos;
    map::Pos last;
    int G;
    bool root;

  public:
    Node() : G(0), root(false)
    {
    }

    Node(map::Pos pos_, int G_, const map::Pos& last_, bool root_ = false)
      : pos(pos_), last(last_), G(G_), root(root_)
    {
    }

    Node(const Node& node) = default;

    [[nodiscard]] int get_F(const map::Pos& dest) const;

    int& get_G();

    map::Pos& get_last();

    [[nodiscard]] const map::Pos& get_pos() const;

    [[nodiscard]] bool is_root() const;

    [[nodiscard]] std::vector<Node> get_neighbors() const;

  private:
    static bool check(const map::Pos& pos);
  };

  bool operator<(const Node& n1, const Node& n2);

  bool is_fire_spot(int range, const map::Pos& pos, const map::Pos& target_pos);

  class AutoTank : public Tank
  {
    friend Tank* build_tank(const TankData& data);

    friend TankData get_tank_data(const Tank*);

  private:
    std::size_t target_id;

    std::vector<AutoTankEvent> route;
    std::size_t route_pos;

    int gap_count;

    bool has_good_target;

  public:
    AutoTank(const info::TankInfo& info_, map::Pos pos_)
      : Tank(info_, pos_), target_id(0), route_pos(0), gap_count(0), has_good_target(false)
    {
    }

    ~AutoTank() override = default;

    void set_target(std::size_t id);

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
