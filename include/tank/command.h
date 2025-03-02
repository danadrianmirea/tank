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
#ifndef TANK_COMMAND_H
#define TANK_COMMAND_H
#pragma once

#include "input.h"
#include "utils/type_list.h"
#include <variant>
#include <string>
#include <vector>
#include <tuple>
#include <functional>
#include <optional>
#include <stdexcept>
#include <set>

namespace czh::cmd
{
  namespace helper
  {
    bool is_ip(const std::string& s);

    bool is_port(int s);

    bool is_valid_id(int s);

    bool is_alive_id(int s);
  }

  struct CommandInfo
  {
    std::string cmd;
    std::string args;
    std::vector<input::HintProvider> hint_providers;
  };

  extern const std::set<std::string> remote_cmds;
  extern const std::vector<cmd::CommandInfo> commands;

  namespace details
  {
    using ArgTList = utils::TypeList<std::string, int, bool>;
    using Arg = decltype(as_variant(ArgTList{}));

    template<typename T>
      requires(utils::contains_v<T, ArgTList>)
    T arg_get(const Arg& a)
    {
      if (a.index() != utils::index_of_v<T, ArgTList>)
      {
        throw std::runtime_error("Get wrong type.");
      }
      return std::get<T>(a);
    }

    template<typename... Args>
    auto make_index()
    {
      return std::vector<size_t>{
        {
          utils::index_of_v<Args, ArgTList>
        }...
      };
    }

    template<typename Func, typename List, std::size_t... index>
    auto call_with_args_impl(Func&& func, const std::vector<Arg>& v, std::index_sequence<index...>)
    {
      auto tmp = std::make_tuple(v[index]...);
      auto args = std::apply([](auto&&... elems)
      {
        return std::make_tuple(arg_get<utils::index_at_t<index, List> >(elems)...);
      }, tmp);
      return func(std::get<index>(args)...);
    }

    template<typename List, std::size_t... index>
    auto args_get_impl(const std::vector<Arg>& v, std::index_sequence<index...>)
    {
      auto tmp = std::make_tuple(v[index]...);
      auto args = std::apply([](auto&&... elems)
      {
        return std::make_tuple(arg_get<utils::index_at_t<index, List> >(elems)...);
      }, tmp);
      return args;
    }

    template<typename Func>
    struct get_func_args
    {
    };

    template<typename... Args>
    struct get_func_args<std::function<bool(Args...)> >
    {
      using args = utils::TypeList<std::remove_cvref_t<Args>...>;
    };

    template<typename T>
    using get_func_args_t = typename get_func_args<T>::args;
  }

  template<typename... Args, typename Func>
  auto call_with_args(Func&& func, const std::vector<details::Arg>& v)
  {
    if (v.size() != sizeof...(Args))
    {
      throw std::runtime_error("Invalid call.");
    }
    return details::call_with_args_impl<Func, utils::TypeList<Args...> >
        (std::forward<Func>(func), v, std::make_index_sequence<sizeof...(Args)>());
  }

  template<typename List>
  auto args_get(const std::vector<details::Arg>& v)
  {
    if (v.size() != utils::size_of_v<List>)
    {
      throw std::runtime_error("Invalid get.");
    }
    return details::args_get_impl<List>(v, std::make_index_sequence<utils::size_of_v<List> >());
  }

  template<typename... Args>
  bool args_is(const std::vector<details::Arg>& v)
  {
    static const auto expected = details::make_index<Args...>();
    if (expected.size() != v.size()) return false;
    for (size_t i = 0; i < v.size(); ++i)
    {
      if (expected[i] != v[i].index())
      {
        return false;
      }
    }
    return true;
  }

  struct CmdCall
  {
    bool good;
    std::string name;
    std::vector<details::Arg> args;
    std::vector<std::string> error;

    [[nodiscard]] bool is(const std::string& n) const
    {
      return name == n;
    }

    template<typename Func>
    auto get_if(Func&& r) const
    {
      using FuncType = decltype(std::function(std::forward<Func>(r)));
      return check(std::function(std::forward<Func>(r)))
               ? std::make_optional(args_get<details::get_func_args_t<FuncType> >(args))
               : std::nullopt;
    }

    bool assert(bool a, const std::string& err)
    {
      if (!a)
        error.emplace_back(err);
      return a;
    }

  private:
    template<typename... Args>
    bool check(std::function<bool(Args...)>&& r) const
    {
      return args_is<std::remove_cvref_t<Args>...>(args) && call_with_args<std::remove_cvref_t<Args>...>(r, args);
    }
  };

  CmdCall parse(const std::string& cmd);

  void run_command(size_t userid, const std::string& str);
}
#endif //TANK_COMMAND_H
