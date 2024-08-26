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
#ifndef TANK_GAME_MAP_H
#define TANK_GAME_MAP_H
#pragma once

#include <vector>
#include <list>
#include <algorithm>
#include <random>
#include <map>

namespace czh::ar
{
  class Archiver;
}

namespace czh::tank
{
  class Tank;
}

namespace czh::bullet
{
  class Bullet;
}

namespace czh::map
{
  // WARNING:
  // Large MAP_DIVISION will slow down route finding, especially on unreachable point.
  constexpr uint32_t MAP_DIVISION = 36;

  enum class Status
  {
    WALL, TANK, BULLET, END
  };

  enum class Direction
  {
    UP, DOWN, LEFT, RIGHT, END
  };

  class Pos
  {
  public:
    int x = 0;
    int y = 0;

  public:
    bool operator==(const Pos& pos) const;

    bool operator!=(const Pos& pos) const;
  };

  bool operator<(const Pos& pos1, const Pos& pos2);

  std::size_t get_distance(const map::Pos& from, const map::Pos& to);

  struct Zone // [X min, X max)   [Y min, Y max)
  {
    int x_min{0};
    int x_max{0};
    int y_min{0};
    int y_max{0};

    [[nodiscard]] bool contains(int i, int j) const;

    [[nodiscard]] bool contains(const Pos& p) const;

    [[nodiscard]] Zone bigger_zone(int i) const;
  };

  class Map;
  class Point;

  class Point
  {
    friend class ar::Archiver;
    friend class Map;

  private:
    bool generated;
    bool temporary;
    std::vector<Status> statuses;

    tank::Tank* tank;
    std::vector<bullet::Bullet*> bullets;

  public:
    Point() : generated(false), temporary(true), tank(nullptr)
    {
    }

    Point(const std::string&, std::vector<Status> s) : generated(true), temporary(true),
                                                       statuses(std::move(s)), tank(nullptr)
    {
    }

    [[nodiscard]] bool is_generated() const;

    [[nodiscard]] bool is_temporary() const;

    [[nodiscard]] bool is_empty() const;

    [[nodiscard]] tank::Tank* get_tank() const;

    [[nodiscard]] const std::vector<bullet::Bullet*>& get_bullets() const;

    void add_status(const Status& status, void*);

    void remove_status(const Status& status);

    void remove_all_statuses();

    [[nodiscard]] bool has(const Status& status) const;

    [[nodiscard]] std::size_t count(const Status& status) const;
  };

  const Point& generate(const Pos& i, size_t seed);

  const Point& generate(int x, int y, size_t seed);

  class Map
  {
    friend class ar::Archiver;

  private:
    std::map<Pos, Point> map;

  public:
    unsigned long long seed;

  public:
    Map();

    int tank_up(const Pos& pos);

    int tank_down(const Pos& pos);

    int tank_left(const Pos& pos);

    int tank_right(const Pos& pos);

    int bullet_up(bullet::Bullet* b, const Pos& pos);

    int bullet_down(bullet::Bullet* b, const Pos& pos);

    int bullet_left(bullet::Bullet* b, const Pos& pos);

    int bullet_right(bullet::Bullet* b, const Pos& pos);

    int add_tank(tank::Tank*, const Pos& pos);

    int add_bullet(bullet::Bullet*, const Pos& pos);

    void remove_status(const Status& status, const Pos& pos);

    [[nodiscard]] bool has(const Status& status, const Pos& pos) const;

    [[nodiscard]] size_t count(const Status& status, const Pos& pos) const;

    int fill(const Zone& zone, const Status& status = Status::END);

    [[nodiscard]] const Point& at(const Pos& i) const;

    [[nodiscard]] const Point& at(int x, int y) const;

  private:
    int tank_move(const Pos& pos, int direction);

    int bullet_move(bullet::Bullet*, const Pos& pos, int direction);
  };

  extern Map map;
  extern const Point empty_point;
  extern const Point wall_point;
}
#endif
