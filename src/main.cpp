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

#include "tank/broadcast.h"
#include "tank/command.h"
#include "tank/config.h"
#include "tank/drawing.h"
#include "tank/game.h"
#include "tank/input.h"
#include "tank/online.h"
#include "tank/tank.h"
#include "tank/term.h"
#include "tank/utils/utils.h"

#ifdef _WIN32
#include <timeapi.h>
#include <windef.h>
#pragma comment(lib, "winmm")
#endif

#ifdef SIGCONT
#include <csignal>
#endif

#include <chrono>
#include <string>
#include <thread>
#include <vector>
using namespace czh;

void react(tank::NormalTankEvent event)
{
  if (draw::state.snapshot.tanks[g::state.id].is_alive)
  {
    if (g::state.mode == g::Mode::CLIENT)
    {
      int ret = online::cli.tank_react(event);
    }
    else
    {
      g::tank_react(g::state.id, event);
    }
  }
}

#ifdef SIGCONT
void sighandler(int)
{
  term::keyboard.init();
  draw::state.inited = false;
  g::state.suspend = false;
}
#endif

int main()
{
#ifdef _WIN32
  TIMECAPS tc;
  dbg::tank_assert(timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR);
  const UINT wTimerRes = (std::min)((std::max)(tc.wPeriodMin, static_cast<UINT>(1)), tc.wPeriodMax);
  timeBeginPeriod(wTimerRes);
#endif

#ifdef SIGCONT
  signal(SIGCONT, sighandler);
#endif

  std::thread game_thread(
    []
    {
      while (true)
      {
        std::chrono::high_resolution_clock::time_point beg = std::chrono::high_resolution_clock::now();
        if (g::state.mode == g::Mode::NATIVE || g::state.mode == g::Mode::SERVER)
          g::mainloop();

        auto ret = draw::update_snapshot();
        if (ret == 0)
          draw::draw();

        std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        if (cfg::config.tick > cost)
        {
          std::this_thread::sleep_for(cfg::config.tick - cost);
        }
      }
    });
  g::add_tank(map::Pos{0, 0}, 0);
  while (true)
  {
    input::Input i = input::get_input();
    if (g::state.page == g::Page::GAME)
    {
      switch (i)
      {
        case input::Input::UP:
          react(tank::NormalTankEvent::UP);
          break;
        case input::Input::DOWN:
          react(tank::NormalTankEvent::DOWN);
          break;
        case input::Input::LEFT:
          react(tank::NormalTankEvent::LEFT);
          break;
        case input::Input::RIGHT:
          react(tank::NormalTankEvent::RIGHT);
          break;
        case input::Input::LP_UP_BEGIN:
          react(tank::NormalTankEvent::UP_AUTO);
          break;
        case input::Input::LP_DOWN_BEGIN:
          react(tank::NormalTankEvent::DOWN_AUTO);
          break;
        case input::Input::LP_LEFT_BEGIN:
          react(tank::NormalTankEvent::LEFT_AUTO);
          break;
        case input::Input::LP_RIGHT_BEGIN:
          react(tank::NormalTankEvent::RIGHT_AUTO);
          break;
        case input::Input::LP_KEY_SPACE_BEGIN:
          react(tank::NormalTankEvent::FIRE_AUTO);
          break;
        case input::Input::LP_END:
          react(tank::NormalTankEvent::AUTO_OFF);
          break;
        case input::Input::KEY_SPACE:
          react(tank::NormalTankEvent::FIRE);
          break;
        case input::Input::KEY_O:
          g::state.page = g::Page::STATUS;
          draw::state.inited = false;
          break;
        case input::Input::KEY_I:
          g::state.page = g::Page::NOTIFICATION;
          draw::state.inited = false;
          break;
        case input::Input::KEY_L:
        {
          if (g::state.mode == g::Mode::CLIENT)
          {
            int ret = online::cli.add_auto_tank(utils::randnum<int>(1, 11));
          }
          else
          {
            std::lock_guard ml(g::mainloop_mtx);
            std::lock_guard dl(draw::drawing_mtx);
            g::add_auto_tank(utils::randnum<int>(1, 11), draw::state.visible_zone, 0);
          }
        }
        break;
        default:
          break;
      }
    }
    else if (g::state.page == g::Page::HELP)
    {
      switch (i)
      {
        case input::Input::UP:
          if (draw::state.help_pos != 0)
          {
            draw::state.help_pos--;
            draw::state.inited = false;
          }
          break;
        case input::Input::DOWN:
          if (draw::state.help_pos < draw::state.help_text.size() - 1)
          {
            draw::state.help_pos++;
            draw::state.inited = false;
          }
          break;
        default:
          break;
      }
    }
    else if (g::state.page == g::Page::STATUS)
    {
      switch (i)
      {
        case input::Input::UP:
          if (draw::state.status_pos != 0)
          {
            draw::state.status_pos--;
            draw::state.inited = false;
          }
          break;
        case input::Input::DOWN:
          if (draw::state.status_pos < draw::state.snapshot.tanks.size() - 1)
          {
            draw::state.status_pos++;
            draw::state.inited = false;
          }
          break;
        case input::Input::KEY_O:
          g::state.page = g::Page::GAME;
          draw::state.inited = false;
          break;
        default:
          break;
      }
    }
    else if (g::state.page == g::Page::NOTIFICATION)
    {
      switch (i)
      {
        case input::Input::UP:
          if (draw::state.notification_pos != 0)
          {
            draw::state.notification_pos--;
            draw::state.inited = false;
          }
          break;
        case input::Input::DOWN:
          if (draw::state.notification_pos < draw::state.notification_text.size() - 1)
          {
            draw::state.notification_pos++;
            draw::state.inited = false;
          }
          break;
        case input::Input::KEY_I:
          g::state.page = g::Page::GAME;
          draw::state.inited = false;
          break;
        default:
          break;
      }
    }
    switch (i)
    {
      case input::Input::KEY_SLASH:
        input::state.typing_command = true;
        input::state.line.clear();
        input::state.visible_line = {0, 0};
        input::state.pos = 0;
        input::state.hint.clear();
        input::state.hint_pos = 0;
        input::state.history.emplace_back("");
        input::state.history_pos = input::state.history.size() - 1;
        input::edit_refresh_line_lock();
        if (auto c = input::get_input(); c == input::Input::COMMAND)
        {
          cmd::run_command(g::state.id, input::state.line);
        }
        input::state.typing_command = false;
        break;
      case input::Input::KEY_ENTER:
        g::state.page = g::Page::GAME;
        draw::state.inited = false;
        break;
      case input::Input::KEY_CTRL_C:
      {
        std::lock_guard ml(g::mainloop_mtx);
        std::lock_guard dl(draw::drawing_mtx);
        g::quit();
#ifdef _WIN32
        timeEndPeriod(wTimerRes);
#endif
        std::exit(0);
      }
      break;
#ifdef SIGCONT
      case input::Input::KEY_CTRL_Z:
      {
        std::lock_guard ml(g::mainloop_mtx);
        std::lock_guard dl(draw::drawing_mtx);
        if (g::state.mode == g::Mode::CLIENT)
        {
          online::cli.logout();
          g::state.id = 0;
          draw::state.focus = g::state.id;
          draw::state.inited = false;
          g::state.mode = g::Mode::NATIVE;
        }
        else if (g::state.mode == g::Mode::SERVER)
        {
          online::svr.stop();
          for (auto& id : g::state.users | std::views::keys)
          {
            if (id == 0)
              continue;
            g::state.tanks[id]->kill();
            g::state.tanks[id]->clear();
            delete g::state.tanks[id];
            g::state.tanks.erase(id);
          }
          g::state.users = {{0, g::state.users[0]}};
          g::state.mode = g::Mode::NATIVE;
        }
        g::state.suspend = true;
        term::keyboard.deinit();
        raise(SIGSTOP);
      }
      break;
#endif
      default:
        break;
    }
  }
}
