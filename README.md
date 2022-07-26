# tank
一个Windows/Linux C++小游戏
## 示例
![server](https://gitee.com/cmvy2020/tank/raw/main/examples/tank.png)
## 规则
- 玩家使用WSAD控制蓝色Tank
- 按l键生成随机Level的Auto Tank
- 按b键生成高HP，强攻击力但低移速、攻速的Auto Tank
- 按p键复活
- Level越高HP越低,移速和攻速越高
- Auto Tank会自动寻找目标并攻击
- Auto Tank会随机选择目标(Tank或Auto Tank)，在目标死亡前不会更改目标
- 每次开局随机生成迷宫
## 编译
```
mkdir build && cd build 
cmake .. && make
./tank
```