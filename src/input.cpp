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
#include "tank/globals.h"
#include "tank/game.h"
#include "tank/utils.h"
#include "tank/term.h"

#include <string>
#include <vector>
#include <algorithm>
#include <chrono>

namespace czh::g
{
  bool typing_command = false;
  std::string cmd_line{};
  size_t cmd_pos = 0;
  std::pair<size_t, size_t> visible_cmd_line{0, 0};
  std::vector<std::string> history{};
  size_t history_pos = 0;
  cmd::Hints hint{};
  size_t hint_pos = 0;
  std::chrono::high_resolution_clock::time_point last_press = std::chrono::high_resolution_clock::now();
  input::Input last_input_value{};
  input::LongPressMode long_press_mode = input::LongPressMode::Off;
  long long_pressing_threshold = 80000;
}

namespace czh::input
{
  bool is_special_key(int c)
  {
    return (c >= 0 && c <= 6) || (c >= 8 && c <= 14) || c == 16 || c == 20 || c == 21 || c == 23 || c == 26 ||
           c == 27 || c == 127;
  }

  void get_hint()
  {
    g::hint.clear();
    g::hint_pos = 0;
    std::vector<std::string> tokens;
    std::string temp;
    auto it_str = g::cmd_line.cbegin();
    while (it_str < g::cmd_line.cend())
    {
      while (it_str < g::cmd_line.cend() && std::isspace(*it_str))
        ++it_str;
      while (!std::isspace(*it_str) && it_str < g::cmd_line.cend())
        temp += *it_str++;
      tokens.emplace_back(temp);
      temp.clear();
    }

    if (tokens.size() == 1)
    {
      // command hint
      auto its = utils::find_all_if(g::commands.cbegin(), g::commands.cend(),
                                    [&tokens](auto&& f) { return utils::begin_with(f.cmd, tokens[0]); });
      for (auto& it : its)
        g::hint.emplace_back(it->cmd.substr(tokens[0].size()), true);
      return;
    }
    else if (tokens.size() > 1)
    {
      auto it = std::find_if(g::commands.cbegin(), g::commands.cend(),
                             [a = tokens[0]](auto&& f) { return utils::begin_with(f.cmd, a); });
      if (it != g::commands.end())
      {
        if (tokens.size() - 2 < it->hint_providers.size())
        {
          if (tokens.back().empty())
          {
            g::hint = it->hint_providers[tokens.size() - 2](tokens[tokens.size() - 2]);
            return;
          }
          else
          {
            auto h = it->hint_providers[tokens.size() - 2](tokens[tokens.size() - 2]);
            for (auto& r : h)
            {
              if (r.applicable && utils::begin_with(r.hint, tokens.back()))
                g::hint.emplace_back(r.hint.substr(tokens.back().size()), true);
            }
            return;
          }
        }
      }
    }

    // history hint
    auto its = utils::find_all_if(g::history.cbegin(), g::history.cend(),
                                  [](auto&& f) { return utils::begin_with(f, g::cmd_line); });
    for (auto& it : its)
      g::hint.emplace_back(it->substr(g::cmd_line.size()), true);
  }

  void pos_left()
  {
    auto& [beg, end] = g::visible_cmd_line;
    if (g::cmd_pos > beg) return;
    beg = g::cmd_pos;
    end = (std::min)(beg + g::screen_width - 2, g::cmd_line.size());
  }

  void pos_right()
  {
    auto& [beg, end] = g::visible_cmd_line;
    if (g::cmd_pos <= end) return;
    end = g::cmd_pos;
    beg = end > g::screen_width - 2 ? end - g::screen_width + 2 : 0;
  }

  void get_visible_cmd_line()
  {
    auto& [beg, end] = g::visible_cmd_line;
    if (g::cmd_line.size() <= g::screen_width - 2)
    {
      beg = 0;
      end = g::screen_width - 2;
    }
    else
    {
      if (g::cmd_pos < g::screen_width - 2)
      {
        beg = 0;
        end = g::screen_width - 2;
      }
      else
      {
        end = g::cmd_pos;
        beg = end - g::screen_width + 2;
      }
    }
  }

  void cmdline_refresh(bool with_hint = true)
  {
    // move to begin and clear the cmd_line
    term::move_cursor({0, g::screen_height - 1});
    term::show_cursor();
    term::output("\x1b[K");
    // the current cmd_line

    auto color = [](const std::string& s) { return utils::color_256_fg(s, 208); };

    // Too long, disable hint
    if (g::hint.empty() || g::cmd_line.size() + g::hint[g::hint_pos].hint.size() > g::screen_width - 2)
      with_hint = false;

    if (g::cmd_line.size() <= g::screen_width - 1)
    {
      term::output(color("/"), g::cmd_line);
      // hint
      if (with_hint && !g::hint.empty())
        term::output("\x1b[2m", g::hint[g::hint_pos].hint, "\x1b[0m");

      term::move_cursor({g::cmd_pos + 1, g::screen_height - 1});
    }
    else // still too long, split
    {
      const auto& [beg, end] = g::visible_cmd_line;
      if (beg == 0 && end == 0)
        get_visible_cmd_line();
      if (beg == 0)
        term::output(color("/"));
      else
        term::output(color("<"));
      size_t sz = end - beg;
      utils::tank_assert(beg + sz - 1 < g::cmd_line.size());
      term::output(g::cmd_line.substr(beg, sz));
      if (end - beg == g::screen_width - 2 && end != g::cmd_line.size())
        term::output(color(">"));
      term::move_cursor({g::cmd_pos - beg + 1, g::screen_height - 1});
    }
  }

  void edit_refresh_line(bool with_hint)
  {
    std::lock_guard<std::mutex> l(g::drawing_mtx);
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
    if (g::hint.empty()) return;
    ++g::hint_pos;
    if (g::hint_pos >= g::hint.size())
      g::hint_pos = 0;
    edit_refresh_line(true);
  }

  void move_to_beginning()
  {
    if (g::cmd_pos == 0) return;
    g::cmd_pos = 0;
    pos_left();
    edit_refresh_line();
  }

  void move_to_end(bool apply_hint = true)
  {
    if (g::cmd_pos == g::cmd_line.size() && g::hint.empty()) return;
    bool refresh = false;

    if (apply_hint && !g::hint.empty() && g::hint[g::hint_pos].applicable)
    {
      g::cmd_line += g::hint[g::hint_pos].hint;
      g::cmd_line += " ";
      get_hint();
      refresh = true;
    }

    auto origin_width = g::cmd_pos;
    g::cmd_pos = g::cmd_line.size();
    pos_right();
    if (refresh) edit_refresh_line();
  }

  void move_to_word_beginning()
  {
    auto origin = g::cmd_pos;
    if (g::cmd_line[g::cmd_pos - 1] == ' ')
    {
      --g::cmd_pos;
    }
    // curr is not space
    while (g::cmd_pos > 0 && g::cmd_line[g::cmd_pos] == ' ')
    {
      --g::cmd_pos;
    }
    // prev is space or begin
    while (g::cmd_pos > 0 && g::cmd_line[g::cmd_pos - 1] != ' ')
    {
      --g::cmd_pos;
    }
    pos_left();
    edit_refresh_line();
  }

  void move_to_word_end()
  {
    auto origin = g::cmd_pos;
    // curr is not space
    while (g::cmd_pos < g::cmd_line.size() && g::cmd_line[g::cmd_pos] == ' ')
    {
      ++g::cmd_pos;
    }
    // next is space or end
    while (g::cmd_pos < g::cmd_line.size() && g::cmd_line[g::cmd_pos] != ' ')
    {
      ++g::cmd_pos;
    }
    pos_right();
    edit_refresh_line();
  }

  void move_left()
  {
    if (g::cmd_pos > 0)
    {
      auto origin = g::cmd_pos;
      --g::cmd_pos;
    }
    pos_left();
    edit_refresh_line();
  }

  void move_right()
  {
    if (g::cmd_pos < g::cmd_line.size())
    {
      auto origin = g::cmd_pos;
      ++g::cmd_pos;
    }
    pos_right();
    edit_refresh_line();
  }

  void edit_delete()
  {
    if (g::cmd_pos >= g::cmd_line.size()) return;
    g::cmd_line.erase(g::cmd_pos, 1);
    get_hint();
    edit_refresh_line();
  }

  void edit_backspace()
  {
    if (g::cmd_pos == 0) return;
    g::cmd_line.erase(g::cmd_pos - 1, 1);
    --g::cmd_pos;
    get_hint();
    edit_refresh_line();
  }

  void edit_delete_next_word()
  {
    if (g::cmd_pos == g::cmd_line.size() - 1) return;
    auto i = g::cmd_pos;
    // skip space
    while (i < g::cmd_line.size() && g::cmd_line[i] == ' ')
    {
      ++i;
    }
    // find end
    while (i < g::cmd_line.size() && g::cmd_line[i] != ' ')
    {
      ++i;
    }
    g::cmd_line.erase(g::cmd_pos + 1, i - g::cmd_pos);
    get_hint();
    edit_refresh_line();
  }

  void edit_history_helper(bool prev)
  {
    if (g::history_pos == g::history.size() - 1)
    {
      g::history.back() = g::cmd_line;
    }
    auto next_history = [prev]() -> int
    {
      auto origin = g::history_pos;
      while (g::history[origin] == g::history[g::history_pos])
      // skip same command
      {
        if (prev)
        {
          if (g::history_pos != 0)
          {
            --g::history_pos;
          }
          else
          {
            return -1;
          }
        }
        else
        {
          if (g::history_pos != g::history.size() - 1)
          {
            ++g::history_pos;
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
    g::cmd_line = g::history[g::history_pos];
    g::cmd_pos = g::cmd_line.size();
    get_hint();
    edit_refresh_line();
    move_to_end(false);
  }

  void edit_up()
  {
    edit_history_helper(true);
  }

  void edit_down()
  {
    edit_history_helper(false);
  }

  void edit_left()
  {
    move_left();
  }

  void edit_right()
  {
    move_right();
  }

  Input get_raw_input()
  {
    if (g::typing_command)
    {
      while (true)
      {
        int buf = g::keyboard.getch();
        if (is_special_key(buf))
        {
          auto key = static_cast<SpecialKey>(buf);
          if (key == SpecialKey::TAB)
          {
            if (g::hint.size() == 1)
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
              g::history.pop_back();
              return Input::KEY_CTRL_C;
              continue;
              break;
            case SpecialKey::CTRL_Z:
              g::history.pop_back();
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
              g::output_inited = false;
              term::clear();
              break;
            case SpecialKey::LINE_FEED:
            case SpecialKey::ENTER:
              if (g::cmd_line.empty())
              {
                g::history.pop_back();
              }
              else
              {
                g::history.back() = g::cmd_line;
              }
              edit_refresh_line(false);
              return Input::COMMAND;

              break;
            case SpecialKey::CTRL_N:
              edit_down();
              break;
            case SpecialKey::CTRL_P:
              edit_up();
              break;
            case SpecialKey::CTRL_T:
              if (g::cmd_pos != 0)
              {
                auto tmp = g::cmd_line[g::cmd_pos];
                g::cmd_line[g::cmd_pos] = g::cmd_line[g::cmd_pos - 1];
                g::cmd_line[g::cmd_pos - 1] = tmp;
              }
              break;
            case SpecialKey::CTRL_U:
              break;
            case SpecialKey::CTRL_W:
            {
              if (g::cmd_pos == 0) break;
              auto origin_pos = g::cmd_pos;
              while (g::cmd_pos > 0 && g::cmd_line[g::cmd_pos - 1] == ' ')
              {
                g::cmd_pos--;
              }
              while (g::cmd_pos > 0 && g::cmd_line[g::cmd_pos - 1] != ' ')
              {
                g::cmd_pos--;
              }
              g::cmd_line.erase(g::cmd_pos, origin_pos - g::cmd_pos);
              break;
            }
            case SpecialKey::ESC: // Escape Sequence
              char seq[3];
              std::cin.read(seq, 1);
            // esc ?
              if (seq[0] != '[' && seq[0] != 'O')
              {
                switch (seq[0])
                {
                  case 'd':
                    edit_delete_next_word();
                    break;
                  case 'b':
                    move_to_word_beginning();
                    break;
                  case 'f':
                    move_to_word_end();
                    break;
                }
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
                      edit_delete();
                    }
                    else if (seq[2] == ';')
                    {
                      std::cin.read(seq, 2);
                      if (seq[0] == '5' && seq[1] == 'C')
                      {
                        move_to_word_end();
                      }
                      if (seq[0] == '5' && seq[1] == 'D')
                      {
                        move_to_word_beginning();
                      }
                    }
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
                      case 'd':
                        edit_delete_next_word();
                        break;
                      case '1':
                        move_to_beginning();
                        break;
                      case '4':
                        move_to_end();
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
                  }
                }
              }
              break;
            case SpecialKey::BACKSPACE:
            case SpecialKey::CTRL_H:
              edit_backspace();
              break;
            default:
              //msg::warn(g::user_id, "Ignored unrecognized key '" + std::string(1, buf) + "'.");
              continue;
              break;
          }
          edit_refresh_line();
        }
        else if (buf == 0xe0)
        {
          buf = g::keyboard.getch();
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
          }
        }
        else
        {
          g::cmd_line.insert(g::cmd_pos++, 1, static_cast<char>(buf));
          pos_right();
          get_hint();
          edit_refresh_line();
        }
      }
    }
    else
    {
      while (true)
      {
        int buf = g::keyboard.getch();
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
            case SpecialKey::ENTER:
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
                  }
                }
              }
              break;
            default:
              //msg::warn(g::user_id, "Ignored unrecognized key '" + std::string(1, buf) + "'.");
              continue;
              break;
          }
        }
        else if (buf == 0xe0)
        {
          buf = g::keyboard.getch();
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
    if (g::typing_command || g::curr_page != g::Page::GAME)
      return get_raw_input();

    while (g::long_press_mode == LongPressMode::On)
    {
      while (!g::keyboard.kbhit())
      {
        auto d = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() - g::last_press);
        if (d.count() > g::long_pressing_threshold)
        {
          g::long_press_mode = LongPressMode::Off;
          return Input::LP_END;
        }
      }
      Input raw = get_raw_input();
      if (g::last_input_value != raw)
      {
        g::long_press_mode = LongPressMode::Off;
        return Input::LP_END;
      }
      g::last_input_value = raw;
      g::last_press = std::chrono::high_resolution_clock::now();
    }

    Input raw = get_raw_input();
    Input ret = raw;

    auto now = std::chrono::high_resolution_clock::now();
    auto d = std::chrono::duration_cast<std::chrono::microseconds>(now - g::last_press);
    if (raw == Input::UP || raw == Input::DOWN || raw == Input::LEFT || raw == Input::RIGHT || raw == Input::KEY_SPACE)
    {
      if (g::last_input_value == raw && d.count() < g::long_pressing_threshold)
      {
        if (g::long_press_mode == LongPressMode::Off)
        {
          ret = static_cast<Input>(static_cast<int>(raw) + 5);
          g::long_press_mode = LongPressMode::On;
        }
      }
    }
    g::last_input_value = raw;
    g::last_press = now;
    return ret;
  }
}
