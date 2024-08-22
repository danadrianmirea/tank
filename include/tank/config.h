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
#ifndef TANK_CONFIG_H
#define TANK_CONFIG_H
#pragma once

#include <chrono>

namespace czh::cfg
{
  struct Config
  {
    std::chrono::milliseconds tick;
    std::chrono::milliseconds msg_ttl;
    bool unsafe_mode;
    long long_pressing_threshold;
  };
  extern Config config;
}
#endif
