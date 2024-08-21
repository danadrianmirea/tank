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

  void tank_assert(bool b, const std::string& detail_ = "Assertion failed.");

  template<typename T>
    requires (!std::is_same_v<std::string, std::decay_t<T> >) &&
             (!std::is_same_v<const char*, std::decay_t<T> >)
  std::string to_str(T&& a)
  {
    return std::to_string(std::forward<T>(a));
  }

  std::string to_str(const std::string& a);

  std::string to_str(const char*& a);

  std::string to_str(char a);

  template<typename T, typename... Args>
  std::string join(char, T&& arg)
  {
    return to_str(arg);
  }

  template<typename T, typename... Args>
  std::string join(char delim, T&& arg, Args&&... args)
  {
    return to_str(arg) + delim + join(delim, std::forward<Args>(args)...);
  }

  template<typename... Args>
  std::string join(char delim, Args&&... args)
  {
    return join(delim, std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
  std::string contact(T&& arg)
  {
    return to_str(arg);
  }

  template<typename T, typename... Args>
  std::string contact(T&& arg, Args&&... args)
  {
    return to_str(arg) + contact(std::forward<Args>(args)...);
  }

  template<typename... Args>
  std::string contact(Args&&... args)
  {
    return contact(std::forward<Args>(args)...);
  }

  bool begin_with(const std::string& a, const std::string& b);

  bool is_ip(const std::string& s);

  bool is_port(const int p);

  bool is_valid_id(const int id);

  bool is_alive_id(const int id);

  bool is_valid_id(const std::string& s);

  bool is_alive_id(const std::string& s);

  bool is_integer(const std::string& r);


  size_t display_width(const std::string::const_iterator& beg, const std::string::const_iterator& end);

  size_t display_width(const std::string& str);

  template<typename... Args>
  size_t display_width(const std::string& str, Args&&... args)
  {
    return display_width(str) + display_width(std::forward<Args>(args)...);
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
}
#endif
