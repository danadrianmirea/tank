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
#include "tank/globals.h"
#include "tank/bullet.h"
#include "tank/info.h"

namespace czh::bullet
{
  int Bullet::react()
  {
    int ret = -1;
    switch (direction)
    {
      case map::Direction::UP:
        ret = g::game_map.bullet_up(this, pos);
        if (ret != 0)
        {
          info.hp -= 1;
          direction = map::Direction::DOWN;
        }
        else
        {
          info.range -= 1;
          pos.y++;
        }
        break;
      case map::Direction::DOWN:
        ret = g::game_map.bullet_down(this, pos);
        if (ret != 0)
        {
          info.hp -= 1;
          direction = map::Direction::UP;
        }
        else
        {
          info.range -= 1;
          pos.y--;
        }
        break;
      case map::Direction::LEFT:
        ret = g::game_map.bullet_left(this, pos);
        if (ret != 0)
        {
          info.hp -= 1;
          direction = map::Direction::RIGHT;
        }
        else
        {
          info.range -= 1;
          pos.x--;
        }
        break;
      case map::Direction::RIGHT:
        ret = g::game_map.bullet_right(this, pos);
        if (ret != 0)
        {
          info.hp -= 1;
          direction = map::Direction::LEFT;
        }
        else
        {
          info.range -= 1;
          pos.x++;
        }
        break;
      default:
        break;
    }
    return ret;
  }

  std::string Bullet::get_text()
  {
    return "**";
  }

  [[nodiscard]] bool Bullet::is_alive() const
  {
    return info.hp > 0 && info.range > 0;
  }

  [[nodiscard]] std::size_t Bullet::get_tank() const
  {
    return from_tank_id;
  }

  void Bullet::kill()
  {
    info.hp = 0;
  }

  [[nodiscard]] const map::Pos& Bullet::get_pos() const
  {
    return pos;
  }

  map::Pos& Bullet::get_pos()
  {
    return pos;
  }

  [[nodiscard]] int Bullet::get_lethality() const
  {
    return info.lethality;
  }

  [[nodiscard]] size_t Bullet::get_id() const
  {
    return info.id;
  }
}
