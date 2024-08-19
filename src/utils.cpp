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
#include "tank/utils.h"
#include "tank/game.h"
#include <regex>

namespace czh::utils
{
  bool is_ip(const std::string& s)
  {
    std::regex ipv4(R"(^(?:(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)($|(?!\.$)\.)){4}$)");
    std::regex ipv6("^(?:(?:[\\da-fA-F]{1,4})($|(?!:$):)){8}$");
    return std::regex_search(s, ipv4) || std::regex_search(s, ipv6);
  }

  bool is_port(const int p)
  {
    return p > 0 && p < 65536;
  }

  bool is_valid_id(const int id)
  {
    return game::id_at(id) != nullptr;
  }

  bool is_alive_id(const int id)
  {
    if (!is_valid_id(id)) return false;
    return game::id_at(id)->is_alive();
  }

  bool is_valid_id(const std::string& s)
  {
    if (s.empty()) return false;
    if (is_integer(s) && s[0] != '-')
    {
      size_t a;
      try
      {
        a = std::stoull(s);
      }
      catch (...)
      {
        return false;
      }
      return game::id_at(a) != nullptr;
    }
    return false;
  }

  bool is_alive_id(const std::string& s)
  {
    if (!is_valid_id(s)) return false;
    return game::id_at(std::stoull(s))->is_alive();
  }

  bool is_integer(const std::string& r)
  {
    if (r[0] != '+' && r[0] != '-' && !std::isdigit(r[0]))
      return false;

    for (size_t i = 1; i < r.size(); ++i)
    {
      if (!std::isdigit(r[i]))
        return false;
    }
    return true;
  }

  void tank_assert(bool b, const std::string& detail_)
  {
    if (!b)
    {
      throw std::runtime_error(detail_);
    }
  }

  std::string to_str(const std::string& a)
  {
    return a;
  }

  std::string to_str(const char*& a)
  {
    return {a};
  }

  std::string to_str(char a)
  {
    return {1, a};
  }

  bool begin_with(const std::string& a, const std::string& b)
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

  size_t escape_code_len(const std::string::const_iterator& beg, const std::string::const_iterator& end)
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

  size_t escape_code_len(const std::string& str)
  {
    return escape_code_len(str.cbegin(), str.cend());
  }

  std::string setw(size_t w, std::string s)
  {
    auto sz = escape_code_len(s);
    if(sz >= w)
      return s;
    s.insert(s.end(), w - sz, ' ');
    return s;
  }

  std::string color_256_fg(const std::string& str, int color)
  {
    return "\x1b[38;5;" + std::to_string(color) + "m" + str + "\x1b[0m";
  }

  std::string color_256_bg(const std::string& str, int color)
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
