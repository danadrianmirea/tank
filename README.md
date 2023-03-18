<h2 align="center">
Tank
</h2> 

<p align="center">
<strong>A cross-platform C++ Game.</strong>
</p>

### Example
![Start](examples/start-example.png)
![Game](examples/game-example.png)
![Status](examples/status-example.png)
![Help](examples/help-example.png)

### Tutorial
#### Key
- Move: WASD
- Attack: space
- All tanks' status: 'o' or 'O'
- Command: '/'

#### Rules:
User's Tank:
- HP: 500
- Lethality: 50   

Auto Tank:  
- HP: (11 - level) * 10
- Lethality: (11 - level) 
- The higher level, the faster it moves and attack.

#### Command
##### help [page]
- Get help.
- Use 'Enter' to return game.
##### quit
- Quit Tank.

##### reshape [width, height]
- Reshape the game map to the given size.
- Default to reshape to the screen's size

##### clear_maze
- Clear all the walls in the game map.

##### fill [Status] [A x,y] [B x,y]
- Status: [0] Empty [1] Wall
- Fill the area from A to B as the given Status.
- B defaults to the same as A
- e.g.  fill 1 0 0 10 10

##### tp [A id] [B id](or [B x,y])
- Teleport A to B
- A should be alive, and there should be space around B.
- e.g.  tp 0 1   |  tp 0 1 1

##### revive [A id]
- Revive A.
- Default to revive all tanks.

##### summon [n] [level]
- Summon n tanks with the given level.
- e.g. summon 50 10

##### kill [A id]
- Kill A.
- Default to kill all tanks.

##### clear [A id]
- Clear A.(auto tank only)
- Default to clear all auto tanks.
###### clear death
- Clear all the died auto tanks
##### Note:
Clear is to delete rather than to kill.
So can't clear the user's tank and cleared tanks can't be revived.
And the bullets of the cleared tank will also be cleared.

##### set [A id] [key] [value]
- Set A's attribute below:
- max_hp (int): Max hp of A. This will take effect when A is revived.
- hp (int): HP of A. This takes effect immediately but won't last when A is revived.
- target (id, int): Auto Tank's target. Target should be alive.
- name (string): Name of A.
###### set [A id] bullet [key] [value]
- hp (int): HP of A's bullet.
- Whe a bullet hits the wall, its hp decreases by one. That means it will bounce HP times.
- lethality (int): Lethality of A's bullet. This can be a negative number, in which case hp will be added.
- range (int): Range of A's bullet.
- e.g. set 0 max_hp 1000  |  set 0 bullet lethality 10
### Dependencies

- Requires C++ 20