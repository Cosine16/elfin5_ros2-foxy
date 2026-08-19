# cos_ws — Elfin5 机械臂开发仓库

在华沿（Huayan）Elfin5（E05-1195_Pro，6 轴 5kg）官方 ROS2 支持包的基础上，
搭建 Gazebo 仿真 + MoveIt2 运动规划环境，并承载后续自研应用。

## 运行环境

- 主机：Windows 11 专业版 + Docker Desktop（WSL2）
- 容器：Ubuntu 20.04 + ROS2 **Foxy** + Gazebo 11 + MoveIt2 2.2.3
- GUI：通过 X11 转发（`DISPLAY`）显示 Gazebo / RViz

## 目录结构

```
cos_ws/
├── elfin_ws/              # ROS2 (colcon) 工作空间 —— 主要开发场所
│   ├── src/elfin_robot/   # 华沿官方 ROS2 包（仿真最小集，5 个包）
│   │   ├── elfin_description/      # URDF / meshes
│   │   ├── elfin_robot_msgs/       # 服务/消息定义
│   │   ├── elfin_basic_api/        # 后台 API + Elfin Control Panel GUI
│   │   └── elfin5/
│   │       ├── elfin5_ros2_gazebo/   # Gazebo 仿真（world / xacro / controller 配置）
│   │       └── elfin5_ros2_moveit2/  # MoveIt2 配置与 launch
│   └── build_all.sh       # 一键清理 + 构建（路径自适应）
├── scripts/
│   ├── start_sim.sh       # 启动仿真（有终端模拟器开窗口，容器内自动转后台进程）
│   ├── start_sim.py       # 同上（多机型版本，依赖 gnome-terminal 等）
│   ├── wait_for_ros.sh    # 等待某个 ROS2 service/node 出现
│   └── legacy/            # 实机（EtherCAT）时代的脚本与诊断文档，接真机时再用
├── windows_sdk/           # 华沿 C++ SDK（Windows 主机侧，MinGW/DLL，与容器无关）
└── document/              # 华沿官方 GitHub 文档（API 说明、MoveIt 插件教程等）
```

## 快速开始（仿真）

```sh
# 1. 构建工作空间（首次或改了代码后）
~/cos_ws/elfin_ws/build_all.sh

# 2. 启动 Gazebo + MoveIt2 + RViz（容器内自动以后台进程运行）
~/cos_ws/scripts/start_sim.sh            # 日志在 ~/cos_ws/elfin_ws/log/sim/
~/cos_ws/scripts/start_sim.sh stop       # 停止后台仿真

# 3. 如需 Elfin Control Panel GUI
~/cos_ws/scripts/start_sim.sh gazebo
```

官方仿真命令（等价手动方式，见 `document/README _cn.md`）：

```sh
source ~/cos_ws/elfin_ws/install/setup.bash
ros2 launch elfin5_ros2_moveit2 elfin5.launch.py          # Gazebo + MoveIt2 + RViz
ros2 launch elfin_basic_api fake_elfin_gui.launch.py      # Control Panel（仿真用 fake 版）
```

## 说明

- `elfin_ws/src/elfin_robot/` 是官方包的仿真子集；实机所需的
  `elfin_robot_bringup` / `elfin_ethercat_driver` 等包不在当前仓库中，
  相关脚本已归档至 `scripts/legacy/`。
- `windows_sdk/` 在 Windows 主机上用 MinGW 编译运行，通过 TCP（`192.168.10.10:10003`）
  直连控制柜，不经过 ROS，也不要放进 colcon 工作空间。
- 容器内没有 gnome-terminal，`start_sim.sh` 会自动切换为后台进程模式；
  在带桌面环境的 Linux 上运行则自动打开多个终端窗口。
