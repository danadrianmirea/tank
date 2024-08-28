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
#ifndef TANK_INPUT_H
#define TANK_INPUT_H
#pragma once

#include <string>
#include <utility>
#include <chrono>
#include <string>
#include <functional>
#include <mutex>

namespace czh::input
{
  enum class Input
  {
    UNEXPECTED,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    KEY_SPACE,
    // long press event
    LP_UP_BEGIN,
    LP_DOWN_BEGIN,
    LP_LEFT_BEGIN,
    LP_RIGHT_BEGIN,
    LP_KEY_SPACE_BEGIN,
    LP_END,

    KEY_O,
    KEY_L,
    KEY_I,
    KEY_SLASH,
    KEY_CTRL_C,
    KEY_CTRL_Z,
    KEY_ENTER,
    COMMAND
  };

  enum class SpecialKey
  {
    CTRL_A = 1,
    CTRL_B = 2,
    CTRL_C = 3,
    CTRL_D = 4,
    CTRL_E = 5,
    CTRL_F = 6,

    CTRL_H = 8,
    TAB = 9,
    LINE_FEED = 10,
    CTRL_K = 11,
    CTRL_L = 12,
    CARRIAGE_RETURN = 13,
    CTRL_N = 14,

    CTRL_P = 16,

    CTRL_T = 20,
    CTRL_U = 21,

    CTRL_W = 23,

    CTRL_Z = 26,
    ESC = 27,

    BACKSPACE = 127
  };

  struct Hint
  {
    std::string hint;
    bool applicable;
  };

  using Hints = std::vector<Hint>;
  using HintProvider = std::function<Hints(const std::string&)>;

  struct InputState
  {
    bool typing_command;
    std::string line;
    size_t pos;
    std::pair<size_t, size_t> visible_range;
    std::vector<std::string> history;
    size_t history_pos;
    Hints hint;
    size_t hint_pos;
    std::chrono::high_resolution_clock::time_point last_press;
    Input last_input_value;
    bool is_long_pressing;
    int is_typing_string; // 0 for not, 1 for ', 2 for "
  };

  extern InputState state;

  void edit_refresh_line_lock(bool with_hint = true);

  void edit_refresh_line_nolock(bool with_hint = true);

  void update_cursor();

  Input get_input();
}
#endif
