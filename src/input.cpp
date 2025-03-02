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
#include "tank/input.h"
#include "tank/command.h"
#include "tank/config.h"
#include "tank/drawing.h"
#include "tank/game.h"
#include "tank/term.h"
#include "tank/utils/debug.h"
#include "tank/utils/utils.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <ranges>
#include <string>
#include <vector>

namespace czh::input
{
  InputState state{
    .typing_command = false,
    .pos = 0,
    .visible_range = {0, 0},
    .history_pos = 0,
    .hint_pos = 0,
    .last_press = std::chrono::high_resolution_clock::now(),
    .last_input_value = Input::UNEXPECTED,
    .is_long_pressing = false,
    .is_typing_string = 0
  };

  bool is_special_key(int c)
  {
    return (c >= 0 && c <= 6) || (c >= 8 && c <= 14) || c == 16 || c == 20 || c == 21 || c == 23 || c == 26 ||
           c == 27 || c == 127;
  }

  void get_hint()
  {
    state.hint.clear();
    state.hint_pos = 0;
    std::vector<std::string> tokens;
    std::string temp;

    auto it_str = state.line.cbegin();
    while (it_str < state.line.cend())
    {
      while (it_str < state.line.cend() && *it_str == ' ')
        ++it_str;
      while (it_str < state.line.cend() && *it_str != ' ')
        temp += *it_str++;
      tokens.emplace_back(temp);
      temp.clear();
    }

    if (tokens.size() == 1)
    {
      // command hint
      auto&& h = cmd::commands |
                 std::views::filter([&tokens](auto&& f) { return utils::begin_with(f.cmd, tokens[0]); }) |
                 std::views::transform([&tokens](auto&& info) { return info.cmd.substr(tokens[0].size()); });
      for (auto&& r : h)
        state.hint.emplace_back(r, true);
      return;
    }
    else if (tokens.size() > 1)
    {
      auto it = std::ranges::find_if(cmd::commands, [a = tokens[0]](auto&& f) { return utils::begin_with(f.cmd, a); });
      if (it != cmd::commands.end())
      {
        if (tokens.size() - 2 < it->hint_providers.size())
        {
          if (tokens.back().empty())
          {
            state.hint = it->hint_providers[tokens.size() - 2](tokens[tokens.size() - 2]);
            return;
          }
          else
          {
            auto h = it->hint_providers[tokens.size() - 2](tokens[tokens.size() - 2]);
            for (auto& r : h)
            {
              if (r.applicable && utils::begin_with(r.hint, tokens.back()))
                state.hint.emplace_back(r.hint.substr(tokens.back().size()), true);
            }
            return;
          }
        }
      }
    }

    // history hint
    auto&& h = state.history | std::views::filter([](auto&& f) { return utils::begin_with(f, state.line); }) |
               std::views::transform([](auto&& s) { return s.substr(state.line.size()); });
    for (auto&& r : h)
      state.hint.emplace_back(r, true);
  }

  void normalize_end()
  {
    auto& [beg, end] = state.visible_range;
    while (end < state.line.size() && (state.line[end] & 0b11000000) == 0b10000000)
      ++end;
    while (end < state.line.size() &&
           utils::display_width(state.line.begin() + static_cast<std::string::difference_type>(beg),
                                state.line.begin() + static_cast<std::string::difference_type>(end)) <
           draw::state.width - 2)
    {
      ++end;
      while ((state.line[end] & 0b11000000) == 0b10000000)
        ++end;
    }
    if (utils::display_width(state.line.begin() + static_cast<std::string::difference_type>(beg),
                             state.line.begin() + static_cast<std::string::difference_type>(end)) >
        draw::state.width - 2)
    {
      --end;
      while ((state.line[end] & 0b11000000) == 0b10000000)
        --end;
    }
  }

  void normalize_beg()
  {
    auto& [beg, end] = state.visible_range;
    while (beg > 0 && (state.line[beg] & 0b11000000) == 0b10000000)
      --beg;
    while (beg > 0 && utils::display_width(state.line.begin() + static_cast<std::string::difference_type>(beg),
                                           state.line.begin() + static_cast<std::string::difference_type>(end)) <
           draw::state.width - 2)
    {
      --beg;
      while ((state.line[beg] & 0b11000000) == 0b10000000)
        --beg;
    }
    if (utils::display_width(state.line.begin() + static_cast<std::string::difference_type>(beg),
                             state.line.begin() + static_cast<std::string::difference_type>(end)) >
        draw::state.width - 2)
    {
      ++beg;
      while ((state.line[beg] & 0b11000000) == 0b10000000)
        ++beg;
    }
  }

  void pos_left()
  {
    auto& [beg, end] = state.visible_range;
    if (state.pos > beg)
      return;

    beg = state.pos;
    end = (std::min)(beg + draw::state.width - 2, state.line.size());
    normalize_end();
  }

  void pos_right()
  {
    auto& [beg, end] = state.visible_range;
    if (state.pos <= end)
      return;
    end = state.pos;
    beg = end > draw::state.width - 2 ? end - draw::state.width + 2 : 0;
    normalize_beg();
  }

  void get_visible_range()
  {
    auto& [beg, end] = state.visible_range;
    if (utils::display_width(state.line) <= draw::state.width - 2)
    {
      beg = 0;
      end = draw::state.width - 2;
    }
    else
    {
      if (utils::display_width(state.line.begin(), state.line.begin() + static_cast<std::string::difference_type>(
                                                     state.pos)) < draw::state.width - 2)
      {
        beg = 0;
        end = draw::state.width - 2;
        normalize_end();
      }
      else
      {
        end = state.pos;
        beg = end > draw::state.width - 2 ? end - draw::state.width + 2 : 0;
        normalize_beg();
      }
    }
  }

  void update_cursor()
  {
    const auto& [beg, end] = state.visible_range;
    auto wd = utils::display_width(state.line.begin() + static_cast<std::string::difference_type>(beg),
                                   state.line.begin() + static_cast<std::string::difference_type>(state.pos));
    term::move_cursor({wd + 1, draw::state.height - 1});
  }

  std::string get_display_line(std::string str)
  {
    std::erase_if(str, [](char c) { return c == '\n' || c == '\r'; });
    return str;
  }

  void cmdline_refresh(bool with_hint = true)
  {
    // move to begin and clear the cmd_line
    term::move_cursor({0, draw::state.height - 1});
    term::show_cursor();
    term::output("\x1b[K");
    // the current cmd_line

    auto color = [](const std::string& s) { return utils::color_256_fg(s, 208); };

    // Too long, disable hint
    if (state.hint.empty() ||
        utils::display_width_all(state.line, state.hint[state.hint_pos].hint) > draw::state.width - 2)
      with_hint = false;

    if (utils::display_width(state.line) <= draw::state.width - 1)
    {
      state.visible_range = {0, 0};
      term::output(color("/"), get_display_line(state.line));
      // hint
      if (with_hint && !state.hint.empty())
        term::output("\x1b[2m", state.hint[state.hint_pos].hint, "\x1b[0m");
      update_cursor();
    }
    else // still too long, split
    {
      const auto& [beg, end] = state.visible_range;
      if (beg == 0 && end == 0)
        get_visible_range();
      if (beg == 0)
        term::output(color("/"));
      else
        term::output(color("<"));
      term::output(get_display_line(state.line.substr(beg, end - beg)));
      if (end != state.line.size())
        term::output(color(">"));
      update_cursor();
    }
  }

  void edit_refresh_line_lock(bool with_hint)
  {
    std::lock_guard l(draw::drawing_mtx);
    cmdline_refresh(with_hint);
    term::flush();
  }

  void edit_refresh_line_nolock(bool with_hint)
  {
    cmdline_refresh(with_hint);
    term::flush();
  }


  void next_hint()
  {
    if (state.hint.empty())
      return;
    ++state.hint_pos;
    if (state.hint_pos >= state.hint.size())
      state.hint_pos = 0;
    edit_refresh_line_lock(true);
  }

  void move_to_beginning()
  {
    std::lock_guard l(draw::drawing_mtx);
    if (state.pos == 0)
      return;
    state.pos = 0;
    pos_left();
    edit_refresh_line_nolock();
  }

  void move_to_end(bool apply_hint = true)
  {
    std::lock_guard l(draw::drawing_mtx);
    if (state.pos == state.line.size() && state.hint.empty())
      return;
    bool refresh = false;

    if (apply_hint && !state.hint.empty() && state.hint[state.hint_pos].applicable)
    {
      state.line += state.hint[state.hint_pos].hint;
      state.line += " ";
      get_hint();
      refresh = true;
    }

    state.pos = state.line.size();
    pos_right();
    if (refresh)
      edit_refresh_line_nolock();
  }

  void move_left()
  {
    std::lock_guard l(draw::drawing_mtx);
    if (state.pos == 0)
      return;
    --state.pos;
    while (state.pos > 0 && (state.line[state.pos] & 0b11000000) == 0b10000000)
      --state.pos;
    pos_left();
    edit_refresh_line_nolock();
  }

  void move_right()
  {
    std::lock_guard l(draw::drawing_mtx);
    if (state.pos == state.line.size())
      return;
    ++state.pos;
    while (state.pos < state.line.size() && (state.line[state.pos] & 0b11000000) == 0b10000000)
      ++state.pos;
    pos_right();
    edit_refresh_line_nolock();
  }

  void edit_delete()
  {
    std::lock_guard l(draw::drawing_mtx);
    if (state.pos == state.line.size())
      return;
    size_t i = state.pos + 1;
    while (i < state.line.size() && (state.line[i] & 0b11000000) == 0b10000000)
      ++i;
    auto offset = i - state.pos;
    state.line.erase(state.pos, offset);


    if (auto& [beg, end] = state.visible_range; beg != 0 || end != 0)
    {
      end -= offset;
      if (end == state.line.size())
      {
        beg = end > draw::state.width - 2 ? end - draw::state.width + 2 : 0;
        normalize_beg();
      }
      else
      {
        end = beg + draw::state.width - 2;
        normalize_end();
      }
    }

    get_hint();
    edit_refresh_line_nolock();
  }

  void edit_backspace()
  {
    std::lock_guard l(draw::drawing_mtx);
    if (state.pos == 0)
      return;
    int i = static_cast<int>(state.pos) - 1;
    while (i > 0 && (state.line[static_cast<size_t>(i)] & 0b11000000) == 0b10000000)
      --i;
    auto offset = state.pos - i;
    state.line.erase(i, offset);
    state.pos -= offset;


    if (auto& [beg, end] = state.visible_range; beg != 0 || end != 0)
    {
      end -= offset;
      beg = end > draw::state.width - 2 ? end - draw::state.width + 2 : 0;
      normalize_beg();
    }

    get_hint();
    edit_refresh_line_nolock();
  }

  void edit_history_helper(bool prev)
  {
    if (state.history_pos == state.history.size() - 1)
    {
      state.history.back() = state.line;
    }
    auto next_history = [prev]() -> int
    {
      auto origin = state.history_pos;
      while (state.history[origin] == state.history[state.history_pos])
      // skip same command
      {
        if (prev)
        {
          if (state.history_pos != 0)
          {
            --state.history_pos;
          }
          else
          {
            return -1;
          }
        }
        else
        {
          if (state.history_pos != state.history.size() - 1)
          {
            ++state.history_pos;
          }
          else
          {
            return -1;
          }
        }
      }
      return 0;
    };
    next_history();
    //
    {
      std::lock_guard l(draw::drawing_mtx);
      state.line = state.history[state.history_pos];
      state.pos = state.line.size();
      state.visible_range = {0, 0};
      get_hint();
    }
    edit_refresh_line_lock();
    move_to_end(false);
  }

  void edit_up() { edit_history_helper(true); }

  void edit_down() { edit_history_helper(false); }

  void edit_left() { move_left(); }

  void edit_right() { move_right(); }

  Input get_raw_input()
  {
    if (state.typing_command)
    {
      std::string codepoint_buf;
      size_t expected_size = 0;
      while (true)
      {
        int buf = term::keyboard.getch();
        if (is_special_key(buf))
        {
          auto key = static_cast<SpecialKey>(buf);
          if (key == SpecialKey::TAB)
          {
            if (state.hint.size() == 1)
              move_to_end();
            else
              next_hint();
            continue;
          }
          switch (key)
          {
            case SpecialKey::CTRL_A:
              move_to_beginning();
              break;
            case SpecialKey::CTRL_B:
              edit_left();
              break;
            case SpecialKey::CTRL_C:
              state.history.pop_back();
              return Input::KEY_CTRL_C;
              continue;
              break;
            case SpecialKey::CTRL_Z:
              state.history.pop_back();
              return Input::KEY_CTRL_Z;
              continue;
              break;
            case SpecialKey::CTRL_D:
              edit_delete();
              break;
            case SpecialKey::CTRL_E:
              move_to_end();
              break;
            case SpecialKey::CTRL_F:
              edit_right();
              break;
            case SpecialKey::CTRL_K:
              edit_delete();
              break;
            case SpecialKey::CTRL_L:
              draw::state.inited = false;
              term::clear();
              break;
            case SpecialKey::LINE_FEED:
            case SpecialKey::CARRIAGE_RETURN:
              if (state.is_typing_string == 0)
              {
                if (state.line.empty())
                  state.history.pop_back();
                else
                  state.history.back() = state.line;
                edit_refresh_line_lock(false);
                return Input::COMMAND;
              }
              break;
            case SpecialKey::CTRL_N:
              edit_down();
              break;
            case SpecialKey::CTRL_P:
              edit_up();
              break;
            case SpecialKey::CTRL_T:
              if (state.pos != 0)
              {
                auto tmp = state.line[state.pos];
                state.line[state.pos] = state.line[state.pos - 1];
                state.line[state.pos - 1] = tmp;
              }
              break;
            case SpecialKey::CTRL_U:
              break;
            case SpecialKey::CTRL_W:
            {
              if (state.pos == 0)
                break;
              auto origin_pos = state.pos;
              while (state.pos > 0 && state.line[state.pos - 1] == ' ')
              {
                state.pos--;
              }
              while (state.pos > 0 && state.line[state.pos - 1] != ' ')
              {
                state.pos--;
              }
              state.line.erase(state.pos, origin_pos - state.pos);
              break;
            }
            case SpecialKey::ESC: // Escape Sequence
              char seq[3];
              std::cin.read(seq, 1);
            // esc ?
              if (seq[0] == '[' || seq[0] == 'O')
              {
                std::cin.read(seq + 1, 1);
                // esc [
                if (seq[0] == '[')
                {
                  if (seq[1] >= '0' && seq[1] <= '9')
                  {
                    std::cin.read(seq + 2, 1);
                    if (seq[2] == '~' && seq[1] == '3')
                      edit_delete();
                  }
                  else
                  {
                    switch (seq[1])
                    {
                      case 'A':
                        edit_up();
                        break;
                      case 'B':
                        edit_down();
                        break;
                      case 'C':
                        edit_right();
                        break;
                      case 'D':
                        edit_left();
                        break;
                      case 'H':
                        move_to_beginning();
                        break;
                      case 'F':
                        move_to_end();
                        break;
                      case '1':
                        move_to_beginning();
                        break;
                      case '4':
                        move_to_end();
                        break;
                      default:
                        break;
                    }
                  }
                }
                // esc 0
                else if (seq[0] == 'O')
                {
                  switch (seq[1])
                  {
                    case 'A':
                      edit_up();
                      break;
                    case 'B':
                      edit_down();
                      break;
                    case 'C':
                      edit_right();
                      break;
                    case 'D':
                      edit_left();
                      break;
                    case 'H':
                      move_to_beginning();
                      break;
                    case 'F':
                      move_to_end();
                      break;
                    default:
                      break;
                  }
                }
              }
              break;
            case SpecialKey::BACKSPACE:
            case SpecialKey::CTRL_H:
              edit_backspace();
              break;
            default:
              continue;
              break;
          }
          edit_refresh_line_lock();
        }
        else if (buf == 0xe0)
        {
          buf = term::keyboard.getch();
          switch (buf)
          {
            case 72:
              edit_up();
              break;
            case 80:
              edit_down();
              break;
            case 75:
              edit_left();
              break;
            case 77:
              edit_right();
              break;
            case 71:
              move_to_beginning();
              break;
            case 79:
              move_to_end();
              break;
            case 8:
              edit_backspace();
              break;
            case 83:
              edit_delete();
              break;
            default:
              break;
          }
        }
        else
        {
          char ch = static_cast<char>(buf);
          if ((0b10000000 & ch) == 0b00000000)
          {
            if (ch == '\'')
            {
              if (state.is_typing_string == 1)
                state.is_typing_string = 0;
              else
                state.is_typing_string = 1;
            }
            else if (ch == '"')
            {
              if (state.is_typing_string == 2)
                state.is_typing_string = 0;
              else
                state.is_typing_string = 2;
            }

            std::lock_guard l(draw::drawing_mtx);
            state.line.insert(state.pos++, 1, ch);
            normalize_end();
            pos_right();
            get_hint();
            edit_refresh_line_nolock();
          }
          else
          {
            if ((0b11000000 & ch) != 0b10000000)
            {
              if (expected_size != 0 || !codepoint_buf.empty())
                return Input::UNEXPECTED;

              codepoint_buf = ch;
              if ((0b11111000 & ch) == 0b11110000)
                expected_size = 4;
              else if ((0b11110000 & ch) == 0b11100000)
                expected_size = 3;
              else if ((0b11100000 & ch) == 0b11000000)
                expected_size = 2;
              else
                return Input::UNEXPECTED;
            }
            else
            {
              if (expected_size == 0 || codepoint_buf.size() >= expected_size)
                return Input::UNEXPECTED;

              codepoint_buf += ch;

              if (codepoint_buf.size() == expected_size)
              {
                std::lock_guard l(draw::drawing_mtx);
                state.line.insert(state.line.begin() + static_cast<std::string::difference_type>(state.pos),
                                  codepoint_buf.begin(), codepoint_buf.end());
                state.pos += codepoint_buf.size();
                normalize_end();
                pos_right();
                codepoint_buf.clear();
                expected_size = 0;
                get_hint();
                edit_refresh_line_nolock();
              }
            }
          }
        }
      }
    }
    else
    {
      while (true)
      {
        int buf = term::keyboard.getch();
        if (is_special_key(buf))
        {
          auto key = static_cast<SpecialKey>(buf);
          if (key == SpecialKey::TAB)
          {
            continue;
          }
          switch (key)
          {
            case SpecialKey::CTRL_C:
              return Input::KEY_CTRL_C;
              break;
            case SpecialKey::CTRL_Z:
              return Input::KEY_CTRL_Z;
              break;
            case SpecialKey::LINE_FEED:
            case SpecialKey::CARRIAGE_RETURN:
              return Input::KEY_ENTER;
              break;
            case SpecialKey::CTRL_N:
              edit_down();
              break;
            case SpecialKey::CTRL_P:
              edit_up();
              break;
            case SpecialKey::ESC: // Escape Sequence
              char seq[3];
              std::cin.read(seq, 1);
            // esc ?
              if (seq[0] != '[' && seq[0] != 'O')
              {
                continue;
              }
              else
              {
                std::cin.read(seq + 1, 1);
                // esc [
                if (seq[0] == '[')
                {
                  if (seq[1] >= '0' && seq[1] <= '9')
                  {
                    std::cin.read(seq + 2, 1);
                    if (seq[2] == '~' && seq[1] == '3')
                    {
                      continue;
                    }
                    else if (seq[2] == ';')
                    {
                      std::cin.read(seq, 2);
                      continue;
                    }
                  }
                  else
                  {
                    switch (seq[1])
                    {
                      case 'A':
                        return Input::UP;
                        break;
                      case 'B':
                        return Input::DOWN;
                        break;
                      case 'C':
                        return Input::RIGHT;
                        break;
                      case 'D':
                        return Input::LEFT;
                        break;
                      default:
                        break;
                    }
                  }
                }
                // esc 0
                else if (seq[0] == 'O')
                {
                  switch (seq[1])
                  {
                    case 'A':
                      return Input::UP;
                      break;
                    case 'B':
                      return Input::DOWN;
                      break;
                    case 'C':
                      return Input::RIGHT;
                      break;
                    case 'D':
                      return Input::LEFT;
                      break;
                    default:
                      break;
                  }
                }
              }
              break;
            default:
              continue;
              break;
          }
        }
        else if (buf == 0xe0)
        {
          buf = term::keyboard.getch();
          switch (buf)
          {
            case 72:
              return Input::UP;
              break;
            case 80:
              return Input::DOWN;
              break;
            case 75:
              return Input::LEFT;
              break;
            case 77:
              return Input::RIGHT;
              break;
            default:
              break;
          }
        }
        else
        {
          if (buf == 'w' || buf == 'W')
          {
            return Input::UP;
          }
          else if (buf == 'a' || buf == 'A')
          {
            return Input::LEFT;
          }
          else if (buf == 's' || buf == 'S')
          {
            return Input::DOWN;
          }
          else if (buf == 'd' || buf == 'D')
          {
            return Input::RIGHT;
          }
          else if (buf == 'o' || buf == 'O')
          {
            return Input::KEY_O;
          }
          else if (buf == 'i' || buf == 'I')
          {
            return Input::KEY_I;
          }
          else if (buf == 'l' || buf == 'L')
          {
            return Input::KEY_L;
          }
          else if (buf == '/')
          {
            return Input::KEY_SLASH;
          }
          else if (buf == ' ')
          {
            return Input::KEY_SPACE;
          }
        }
      }
    }
    return Input::UNEXPECTED;
  }

  Input get_input()
  {
    if (state.typing_command || g::state.page != g::Page::GAME)
      return get_raw_input();

    while (state.is_long_pressing == true)
    {
      while (!term::keyboard.kbhit())
      {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() -
                                                                       state.last_press);
        if (d.count() > cfg::config.long_pressing_threshold)
        {
          state.is_long_pressing = false;
          return Input::LP_END;
        }
      }
      Input raw = get_raw_input();
      if (state.last_input_value != raw)
      {
        state.is_long_pressing = false;
        return Input::LP_END;
      }
      state.last_input_value = raw;
      state.last_press = std::chrono::high_resolution_clock::now();
    }

    Input raw = get_raw_input();
    Input ret = raw;

    auto now = std::chrono::high_resolution_clock::now();
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(now - state.last_press);
    if (raw == Input::UP || raw == Input::DOWN || raw == Input::LEFT || raw == Input::RIGHT || raw == Input::KEY_SPACE)
    {
      if (state.last_input_value == raw && d.count() < cfg::config.long_pressing_threshold)
      {
        if (state.is_long_pressing == false)
        {
          ret = static_cast<Input>(static_cast<int>(raw) + 5);
          state.is_long_pressing = true;
        }
      }
    }
    state.last_input_value = raw;
    state.last_press = now;
    return ret;
  }
} // namespace czh::input
