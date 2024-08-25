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
#ifndef TANK_DRAWING_H
#define TANK_DRAWING_H
#pragma once

#include "game_map.h"

#include <mutex>
#include <set>
#include <string>

namespace czh::draw
{
  // Xterm 256 color
  // https://www.ditig.com/publications/256-colors-cheat-sheet
  struct Style
  {
    int background;
    int wall;
    std::vector<int> tanks;
  };

  std::string colorify_text(size_t id, const std::string &str);

  std::string colorify_tank(size_t id, const std::string &str);

  struct PointView
  {
    map::Status status;
    int tank_id;
    std::string text;

    [[nodiscard]] bool is_empty() const;
  };

  bool operator<(const PointView &c1, const PointView &c2);

  struct MapView
  {
    std::map<map::Pos, PointView> view;
    size_t seed;

    [[nodiscard]] const PointView &at(const map::Pos &i) const;

    [[nodiscard]] const PointView &at(int x, int y) const;

    [[nodiscard]] bool is_empty() const;
  };

  struct TankView
  {
    size_t id{0};
    std::string name;
    int max_hp{0};
    int hp{0};
    bool is_auto{false};
    bool is_alive{false};
    map::Pos pos;
    map::Direction direction{map::Direction::END};
    int bullet_lethality{0};
    int gap{0};
    std::size_t target_id{0};
    bool has_good_target{false};
  };

  struct UserView
  {
    size_t user_id{0};
    std::string ip;
    bool active{false};
  };

  struct Snapshot
  {
    MapView map;
    std::map<size_t, TankView> tanks;
    std::set<map::Pos> changes;
    std::map<size_t, UserView> userinfo;
  };

  struct DrawingState
  {
    bool inited;
    size_t focus;
    std::vector<std::string> help_text;
    std::vector<std::string> status_text;
    size_t status_pos;
    size_t help_pos;
    size_t notification_pos;
    map::Zone visible_zone;
    std::size_t height;
    std::size_t width;
    int fps;
    Snapshot snapshot;
    std::chrono::steady_clock::time_point last_drawing;
    std::chrono::steady_clock::time_point last_message_displayed;
    Style style;
  };
  extern DrawingState state;
  extern std::mutex drawing_mtx;

  extern const PointView empty_point_view;
  extern const PointView wall_point_view;

  const PointView &generate(const map::Pos &i, size_t seed);

  const PointView &generate(int x, int y, size_t seed);

  PointView extract_point(const map::Pos &pos);

  MapView extract_map(const map::Zone &zone);

  std::map<size_t, TankView> extract_tanks();

  std::map<size_t, UserView> extract_userinfo();

  map::Zone get_visible_zone(size_t w, size_t h, size_t id);

  int update_snapshot();

  void draw();
} // namespace czh::draw
#endif // TANK_DRAWING_H
