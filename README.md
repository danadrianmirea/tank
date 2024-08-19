<h2 align="center">
Tank
</h2> 

<p align="center">
<strong>An Online Command-Line Game</strong>
</p>

### Intro

In Tank, you will take control of a powerful tank in a maze, showcasing your strategic skills on the infinite map and
overcome unpredictable obstacles. You can play solo or team up with friends.

### Example

![Game](examples/game-example.png)
![Status](examples/status-example.png)

### Tutorial

#### Control

- Move: WASD or direction keys
- Attack: space
- Status: 'o' or 'O'
- Notification: 'i' or 'I'
- Command: '/'

#### Tank

User's Tank:

- HP: 10000, ATK: 100

Auto Tank:

- HP: (11 - lvl) * 150, ATK: (11 - lvl) * 15
- The higher level the tank is, the faster it moves.

#### Command

help [line]

- Get this help.
- Use 'Enter' to return game.

notification

- Show Notification page.

notification read

- Set all messages as read.

notification clear

- Clear all messages.

notification clear read

- Clear read messages.

status

- show Status page.

quit

- Quit Tank.

pause

- Pause.

continue

- Continue.

save [filename]

- Save the game to a file.

load [filename]

- load the game from a file.

Note:  
Normally `save` and `load` can only be executed by the server itself, but you can use 'set unsafe true' to get around
it.
Notice that it is dangerous to let remote user access to your filesystem.

fill [Status] [A x,y] [B x,y optional]

- Status: [0] Empty [1] Wall
- Fill the area from A to B as the given Status.
- B defaults to the same as A
- e.g. fill 1 0 0 10 10 | fill 1 0 0

tp [A id] ([B id] or [B x,y])

- Teleport A to B
- A should be alive, and there should be space around B.
- e.g. tp 0 1 | tp 0 1 1

revive [A id optional]

- Revive A.
- Default to revive all tanks.

summon [n] [level]

- Summon n tanks with the given level.
- e.g. summon 50 10

kill [A id optional]

- Kill A.
- Default to kill all tanks.

clear [A id optional]

- Clear A.(only Auto Tank)
- Default to clear all auto tanks.

clear death

- Clear all the died Auto Tanks  
  Note:
  Clear is to delete rather than to kill, so the cleared tank can't revive. And the bullets of the cleared tank will
  also be cleared.

set [A id] [key] [value]

- Set A's attribute below:
- max_hp (int): Max hp of A. This will take effect when A is revived.
- hp (int, < max_hp): hp of A.
- target (id, int): Auto Tank's target. Target should be alive.
- name (string): Name of A.

set [A id] bullet [key] [value]

- hp (int): hp of A's bullet.
- lethality (int): lethality of A's bullet. (negative to increase hp)
- range (int): range of A's bullet.
- e.g. set 0 max_hp 1000 | set 0 bullet lethality 10  
  Note:
  When a bullet hits the wall, its hp decreases by one. That means it can bounce "hp - 1" times.

set tick [tick]

- tick (int, milliseconds): minimum time of the game's(or server's) mainloop.

set msgTTL [ttl]

- TTL (int, milliseconds): a message's time to live.

set longPressTH [threshold]

- threshold (int, microseconds): long pressing threshold.

set seed [seed]

- seed (int): the game map's seed.

set unsafe [bool]

- `true` or `false`.

WARNING:  
This will make the remote user accessible to your filesystem (through `save`, `load`).

tell [A id optional] [msg]

- Send a message to A.
- id defaults to be -1, in which case all the players will receive the message.
- msg (string): the message's content.

observe [A id]

- Observe A.

server start [port]

- Start Tank Server.
- port (int): the server's port.

server stop

- Stop Tank Server.

connect [ip] [port] (as [id])

- Connect to Tank Server.
- ip (string): the server's IP.
- port (int): the server's port.
- id (int, optional): login as the remote user id.

disconnect

- Disconnect from the Server.

### Build

Requires C++ 20

#### CMake

```shell
mkdir build && cd build
cmake .. && make
```

#### G++

```shell
g++ src/* -I include -lpthread -O2 -std=c++20 -o tank
```
