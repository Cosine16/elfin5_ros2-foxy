# cos_ws — Elfin5 机械臂开发仓库

在华沿（Huayan）Elfin5（E05-1195_Pro，6 轴 5kg）官方 ROS2 支持包的基础上，
搭建 Gazebo 仿真 + MoveIt2 运动规划环境，并承载后续自研应用。

## 运行环境

- 主机：Windows 11 专业版 + Docker Desktop（WSL2），NVIDIA GTX1050
- 容器：Ubuntu 20.04 + ROS2 **Foxy** + Gazebo 11 + MoveIt2 2.2.3
- GUI：通过 Windows 侧 X server（VcXsrv `-ac -wgl` / X410 Native OpenGL）显示 Gazebo / RViz
- 生产环境：裸机 Ubuntu 20.04 LTS（安装脚本见下文）

## 开发容器（推荐方式）

环境由 `Dockerfile` 固化，容器可随时删了重建；代码通过挂载放在宿主机上。

```sh
# Windows 侧，本仓库根目录：
docker compose build      # 首次或 Dockerfile 变更后
docker compose up -d      # 启动后用 VS Code “附加到正在运行的容器” 进入开发
docker compose down       # 停止并删除容器（代码在挂载目录，不受影响）
```

- `init: true` 解决了容器无 init 导致僵尸进程堆积的问题
- `gpus: all` 直通 GTX1050（需 Windows 侧 NVIDIA 驱动）；配合 `VcXsrv -wgl` 渲染走 GPU，
  显著降低 gzclient/RViz 的 CPU 占用（gzserver 物理仿真始终是 CPU 负载）
- 验证 GPU 是否生效：容器内 `glxinfo -B | grep renderer`
- 详细说明见 `docker-compose.yml` 头部注释

## 生产环境（裸机 Ubuntu 20.04 LTS）

```sh
git clone git@github.com:Cosine16/cos_ws.git ~/cos_ws
~/cos_ws/scripts/setup_ubuntu2004.sh    # 装 ROS2 Foxy/Gazebo/MoveIt2 依赖并构建
```

`setup_ubuntu2004.sh` 与 `Dockerfile` 共用同一份依赖清单，两边同步维护。

## 目录结构

```
cos_ws/
├── Dockerfile             # 开发容器环境（依赖清单与生产脚本共用）
├── docker-compose.yml     # 一键起容器：init 回收僵尸、GPU 直通、代码挂载
├── elfin_ws/              # ROS2 (colcon) 工作空间 —— 主要开发场所
│   ├── src/elfin_robot/   # 华沿官方 ROS2 包
│   │   ├── elfin_description/      # URDF / meshes
│   │   ├── elfin_robot_msgs/       # 服务/消息定义
│   │   ├── elfin_basic_api/        # 后台 API + Elfin Control Panel GUI
│   │   ├── elfin5/
│   │   │   ├── elfin5_ros2_gazebo/   # Gazebo 仿真（world / xacro / controller 配置）
│   │   │   └── elfin5_ros2_moveit2/  # MoveIt2 配置与 launch（仿真/真机入口都在这里）
│   │   ├── soem_ros2/              # SOEM EtherCAT 主站库封装（实机）
│   │   ├── elfin_ethercat_driver/  # EtherCAT 驱动节点（实机）
│   │   ├── elfin_ros_control/      # ros2_control 硬件接口插件（实机）
│   │   └── elfin_robot_bringup/    # 实机参数（网口/从站/标定）与驱动 launch
│   ├── src/cos_shape/     # 自研应用：末端轨迹形状演示（当前为圆周运动 + 调参面板）
│   └── build_all.sh       # 一键清理 + 构建（路径自适应）
├── scripts/
│   ├── start_sim.sh       # 启动仿真（有终端模拟器开窗口，容器内自动转后台进程）
│   ├── start_sim.py       # 同上（多机型版本，依赖 gnome-terminal 等）
│   ├── wait_for_ros.sh    # 等待某个 ROS2 service/node 出现
│   ├── setup_ubuntu2004.sh # 生产裸机（Ubuntu 20.04）依赖安装 + 构建
│   └── legacy/            # 实机（EtherCAT）脚本与诊断文档（start_real.py / calib_zero.py 等）
├── windows_sdk/           # 华沿 C++ SDK（Windows 主机侧，MinGW/DLL，与容器无关）
└── document/              # 官方文档 + 自研分析文档
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

## 真机（EtherCAT）

实机包（`soem_ros2` / `elfin_ethercat_driver` / `elfin_ros_control` / `elfin_robot_bringup`）已在工作空间内。
启动流程（4 个终端按序，需 PREEMPT_RT 内核）见 **`document/架构与数据流.md`** 真机章节，
一键脚本与诊断指令在 `scripts/legacy/`（`start_real.py`、`诊断指令.md`）。

## 文档

- **`document/架构与数据流.md`** —— 全部包/节点/topic/service 清单、仿真与真机启动命令、数据流图（Obsidian/mermaid）
- **`document/已知问题.md`** —— 坑与断链清单（launch 死代码、硬编码网卡、标定注意事项等）
- `document/README _cn.md`、`API_description.md`、`moveit_plugin_tutorial.md` —— 华沿官方文档

## 说明

- `windows_sdk/` 在 Windows 主机上用 MinGW 编译运行，通过 TCP（`192.168.10.10:10003`）
  直连控制柜，不经过 ROS，也不要放进 colcon 工作空间。
- 容器内没有 gnome-terminal，`start_sim.sh` 会自动切换为后台进程模式；
  在带桌面环境的 Linux 上运行则自动打开多个终端窗口。
