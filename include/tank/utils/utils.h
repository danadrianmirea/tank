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
#ifndef TANK_UTILS_H
#define TANK_UTILS_H
#pragma once

#include "wide_char_width.h"
#include "debug.h"

#include <string_view>
#include <string>
#include <random>
#include <type_traits>
#include <ranges>

namespace czh::utils
{
  template<typename T>
  T randnum(T a, T b) // [a, b)
  {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<T> u(a, b - 1);
    return u(gen);
  }

  template<typename BeginIt, typename EndIt>
  concept ItRange =
      requires(BeginIt begin_it, EndIt end_it)
      {
        { ++begin_it };
        { *begin_it };
        requires std::is_same_v<std::decay_t<decltype(*begin_it)>, std::string_view>;
        { begin_it != end_it };
      };
  template<typename T>
  concept Container =
      requires(T value)
      {
        { value.begin() };
        { value.end() };
        requires ItRange<decltype(value.begin()), decltype(value.end())>;
      };

  template<typename T>
    requires (!std::is_same_v<std::string, std::decay_t<T> >) &&
             (!std::is_same_v<const char*, std::decay_t<T> >)
  std::string to_str(T&& a)
  {
    return std::to_string(std::forward<T>(a));
  }

  std::string setw(size_t w, std::string s);

  // Xterm 256 color
  // https://www.ditig.com/publications/256-colors-cheat-sheet
  std::string color_256_fg(const std::string& str, int color);

  std::string color_256_bg(const std::string& str, int color);

  //  struct RGB
  //  {
  //    int r;
  //    int g;
  //    int b;
  //  };

  //std::string color_rgb_fg(const std::string &str, const RGB& rgb);
  //std::string color_rgb_bg(const std::string &str, const RGB& rgb);

  template<typename T>
  concept Integer = std::is_integral_v<T>;

  template<Integer I>
  I numlen(I num)
  {
    I len = 0;
    for (; num > 0; ++len)
      num /= 10;
    return len;
  }

  inline std::string to_str(const std::string& a)
  {
    return a;
  }

  inline std::string to_str(const char*& a)
  {
    return {a};
  }

  inline std::string to_str(char a)
  {
    return {1, a};
  }

  inline bool begin_with(const std::string& a, const std::string& b)
  {
    if (a.size() < b.size()) return false;
    for (size_t i = 0; i < b.size(); ++i)
    {
      if (a[i] != b[i])
      {
        return false;
      }
    }
    return true;
  }

  template<typename... R>
  size_t display_width(R&&... r)
  {
    static constexpr auto utf8_chunk = [](auto c1, auto c2) { return (0b11000000 & c2) == 0b10000000; };
    std::string_view sv{std::forward<R>(r)...};
    if (sv.empty()) return 0;
    auto a = sv
             | std::views::chunk_by([](char c1, char c2) { return c2 != '\x1b'; })
             | std::views::transform(
               [](auto&& s) -> size_t
               {
                 std::string_view sv;
                 if (*s.begin() == '\x1b')
                 {
                   sv = std::string_view{
                     s
                     | std::views::drop_while([](char c) { return c != 'm'; })
                     | std::views::drop(1)
                   };
                 }
                 else
                   sv = std::string_view{s};

                 if (sv.empty()) return 0;

                 auto&& rng = sv
                              | std::views::chunk_by(utf8_chunk)
                              | std::views::transform([](auto&& c) -> size_t
                              {
                                std::string_view sv{c};
                                uint32_t wc{0};
                                if (sv.size() == 1)
                                  wc = sv[0];
                                else if (sv.size() == 2)
                                {
                                  wc = sv[0] & 0b00011111;
                                  wc <<= 6;
                                  wc |= sv[1] & 0b00111111;
                                }
                                else if (sv.size() == 3)
                                {
                                  wc = sv[0] & 0b00001111;
                                  wc <<= 6;
                                  wc |= sv[1] & 0b00111111;
                                  wc <<= 6;
                                  wc |= sv[2] & 0b00111111;
                                }
                                else if (sv.size() == 4)
                                {
                                  wc = sv[0] & 0b00001111;
                                  wc <<= 6;
                                  wc |= sv[1] & 0b00111111;
                                  wc <<= 6;
                                  wc |= sv[2] & 0b00111111;
                                  wc <<= 6;
                                  wc |= sv[3] & 0b00111111;
                                }
                                else
                                  dbg::tank_assert(false, "Invalid UTF-8 string.");
                                int w = wide_char_width(wc);
                                if (w < 0)
                                  dbg::tank_assert(false, "Invalid UTF-8 string.");
                                return static_cast<size_t>(w);
                              });
                 auto ret = std::ranges::fold_left_first(rng, std::plus{});
                 dbg::tank_assert(ret.has_value(), "Invalid UTF-8 string.");
                 return ret.value();
               });

    auto ret = std::ranges::fold_left_first(a, std::plus{});
    dbg::tank_assert(ret.has_value(), "Invalid UTF-8 string.");
    return static_cast<size_t>(ret.value());
  }

  inline size_t display_width(const std::string& str)
  {
    return display_width(str.cbegin(), str.cend());
  }

  template<typename... Args>
  size_t display_width_all(Args&&... args)
  {
    return (... + display_width(std::forward<Args>(args)));
  }

  inline std::string setw(size_t w, std::string s)
  {
    auto sz = display_width(s);
    if (sz >= w)
      return s;
    s.insert(s.end(), w - sz, ' ');
    return s;
  }

  inline std::string color_256_fg(const std::string& str, int color)
  {
    return "\x1b[38;5;" + std::to_string(color) + "m" + str + "\x1b[0m";
  }

  inline std::string color_256_bg(const std::string& str, int color)
  {
    return "\x1b[48;5;" + std::to_string(color) + "m" + str + "\x1b[0m";
  }

  //  std::string color_rgb_fg(const std::string &str, const RGB& rgb)
  //  {
  //    return "\x1b[38;2;" + std::to_string(rgb.r) + ";"
  //           + std::to_string(rgb.g) + ";"
  //           + std::to_string(rgb.b) + "m"
  //           + str + "\x1b[0m";
  //  }
  //  std::string color_rgb_bg(const std::string &str, const RGB& rgb)
  //  {
  //    return "\x1b[48;2;" + std::to_string(rgb.r) + ";"
  //           + std::to_string(rgb.g) + ";"
  //           + std::to_string(rgb.b) + "m"
  //           + str + "\x1b[0m";
  //  }
}
#endif
