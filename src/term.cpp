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
#include "tank/term.h"

#include <cstdio>
#include <iostream>

namespace czh::term
{
  KeyBoard keyboard;

  KeyBoard::KeyBoard() :
      keyboard_mode(0), initial_settings(), new_settings()
#ifdef CZH_TANK_KEYBOARD_MODE_1
      ,
      peek_character(0)
#endif
  {
    init();
  }

  void KeyBoard::init()
  {
#if defined(CZH_TANK_KEYBOARD_MODE_0)
    keyboard_mode = 0;
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(handle, &initial_settings);
    auto curr_mode = initial_settings;
    curr_mode &=
        ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE);
    SetConsoleMode(handle, curr_mode);
#elif defined(CZH_TANK_KEYBOARD_MODE_1)
    keyboard_mode = 1;
    tcgetattr(0, &initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
    peek_character = -1;
#endif
    output("\x1b[?1049h");
    flush();
  }

  KeyBoard::~KeyBoard() { deinit(); }

  void KeyBoard::deinit() const
  {
#if defined(CZH_TANK_KEYBOARD_MODE_0)
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(handle, initial_settings);
#elif defined(CZH_TANK_KEYBOARD_MODE_1)
    tcsetattr(0, TCSANOW, &initial_settings);
#endif
    show_cursor();
    output("\x1b[?1049l");
    flush();
  }

  int KeyBoard::kbhit()
  {
#if defined(CZH_TANK_KEYBOARD_MODE_0)
    return _kbhit();
#elif defined(CZH_TANK_KEYBOARD_MODE_1)
    unsigned char ch;
    if (peek_character != -1)
      return 1;
    new_settings.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
    int nread = static_cast<int>(read(0, &ch, 1));
    new_settings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new_settings);

    if (nread == 1)
    {
      peek_character = ch;
      return 1;
    }
    return 0;
#endif
  }

  int KeyBoard::getch()
  {
#if defined(CZH_TANK_KEYBOARD_MODE_0)
    return _getch();
#elif defined(CZH_TANK_KEYBOARD_MODE_1)
    char ch;
    if (peek_character != -1)
    {
      ch = static_cast<char>(peek_character);
      peek_character = -1;
    }
    else
    {
      read(0, &ch, 1);
    }
    return ch;
#endif
  }

  void move_cursor(const TermPos &pos) { output("\x1b[", pos.get_y() + 1, ";", pos.get_x() + 1, "f"); }

  void flush() { std::cout << std::flush; }

  std::size_t get_height()
  {
#if defined(CZH_TANK_KEYBOARD_MODE_0)
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(handle, &csbi);
    return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#elif defined(CZH_TANK_KEYBOARD_MODE_1)
    winsize w{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
#endif
  }

  std::size_t get_width()
  {
#if defined(CZH_TANK_KEYBOARD_MODE_0)
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(handle, &csbi);
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#elif defined(CZH_TANK_KEYBOARD_MODE_1)
    winsize w{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
#endif
  }

  void clear() { output("\x1b[2J"); }

  void hide_cursor() { output("\x1b[?25l"); }

  void show_cursor() { output("\x1b[?25h"); }
} // namespace czh::term
