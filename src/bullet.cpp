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
#include "tank/bullet.h"

namespace czh::bullet
{
  int Bullet::react()
  {
    int ret = -1;
    switch (direction)
    {
      case map::Direction::UP:
        ret = map::map.bullet_up(this, pos);
        if (ret != 0)
        {
          hp -= 1;
          direction = map::Direction::DOWN;
        }
        else
        {
          range -= 1;
          pos.y++;
        }
        break;
      case map::Direction::DOWN:
        ret = map::map.bullet_down(this, pos);
        if (ret != 0)
        {
          hp -= 1;
          direction = map::Direction::UP;
        }
        else
        {
          range -= 1;
          pos.y--;
        }
        break;
      case map::Direction::LEFT:
        ret = map::map.bullet_left(this, pos);
        if (ret != 0)
        {
          hp -= 1;
          direction = map::Direction::RIGHT;
        }
        else
        {
          range -= 1;
          pos.x--;
        }
        break;
      case map::Direction::RIGHT:
        ret = map::map.bullet_right(this, pos);
        if (ret != 0)
        {
          hp -= 1;
          direction = map::Direction::LEFT;
        }
        else
        {
          range -= 1;
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
    return hp > 0 && range > 0;
  }

  [[nodiscard]] std::size_t Bullet::get_tank() const
  {
    return from_tank_id;
  }

  void Bullet::kill()
  {
    hp = 0;
  }

  [[nodiscard]] int Bullet::get_lethality() const
  {
    return lethality;
  }

  [[nodiscard]] size_t Bullet::get_id() const
  {
    return id;
  }
}
