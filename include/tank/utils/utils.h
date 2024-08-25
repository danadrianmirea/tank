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

#include <string_view>
#include <string>
#include <random>
#include <type_traits>
#include <regex>

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

  template<Container T>
  T split(std::string_view str, char delim)
  {
    T ret;
    size_t first = 0;
    while (first < str.size())
    {
      const auto second = str.find_first_of(delim, first);
      if (first != second)
      {
        ret.insert(ret.end(), str.substr(first, second - first));
      }
      if (second == std::string_view::npos)
      {
        break;
      }
      first = second + 1;
    }
    return ret;
  }

  template<Container T>
  T split(std::string_view str, std::string_view delims)
  {
    T ret;
    size_t first = 0;
    while (first < str.size())
    {
      const auto second = str.find_first_of(delims, first);
      if (first != second)
      {
        ret.insert(ret.end(), str.substr(first, second - first));
      }
      if (second == std::string_view::npos)
      {
        break;
      }
      first = second + 1;
    }
    return ret;
  }

  template<typename It, typename UnaryPred>
  constexpr std::vector<It> find_all_if(It first, It last, UnaryPred p)
  {
    std::vector<It> ret;
    for (; first != last; ++first)
    {
      if (p(*first))
        ret.insert(ret.end(), first);
    }
    return ret;
  }

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

  inline size_t display_width(const std::string::const_iterator& beg, const std::string::const_iterator& end)
  {
    size_t ret = 0;
    std::string n;
    for (auto it = beg; it < end; ++it)
    {
      if (*it == '\x1b')
      {
        while (it < end && *it != 'm') ++it;
        continue;
      }
      ++ret;
      n += *it;
    }
    return ret;
  }

  inline size_t display_width(const std::string& str)
  {
    return display_width(str.cbegin(), str.cend());
  }

  template<typename... Args>
  size_t display_width(const std::string& str, Args&&... args)
  {
    return display_width(str) + display_width(std::forward<Args>(args)...);
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
