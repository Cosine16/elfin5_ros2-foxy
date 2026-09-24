# cos_ws — Elfin5 机械臂开发仓库

在华沿（Huayan）Elfin5（E05-1195_Pro，6 轴 5kg）官方 ROS2 支持包的基础上，
搭建 Gazebo 仿真 + MoveIt2 运动规划环境，并承载后续自研应用。

## 运行环境

- 主机：Windows 11 专业版 + Docker Desktop（WSL2）
- 容器：Ubuntu 20.04 + ROS2 **Foxy** + Gazebo 11 + MoveIt2 2.2.3
- GUI：通过 X11 转发（`DISPLAY`）显示 Gazebo / RViz

## 目录结构

仓库按控制技术栈分为两层 ROS2 工作空间 + 两个独立区域：

```
cos_ws/                  # 项目根目录（本身不是 colcon 工作空间）
├── elfin_ws/            # 【厂家驱动层 / underlay】EtherCAT 控制栈
│   ├── src/elfin_robot/   # 华沿官方 ROS2 包（核心驱动，勿改结构以便同步上游）
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
│   ├── src/elfin_demo/    # 华沿官方示例
│   └── build_all.sh       # 一键清理 + 构建（路径自适应）
├── app_ws/              # 【自研应用层 / overlay】依赖 elfin_ws，须后构建
│   ├── src/cos_shape/     # 末端轨迹形状演示（圆周运动 + 调参面板）
│   ├── src/cos_rsvisual/  # Elfin5 + RealSense D435i 眼在手上视觉跟随
│   └── build_all.sh       # 自动 source elfin_ws 后构建
├── realsense_ws/        # RealSense 感知/仿真辅助工作空间（generated*/ 为运行时产物）
├── scripts/
│   ├── start_sim.py       # 仿真主入口（多机型，开多终端）
│   ├── start_real.py      # 实机主入口（EtherCAT，4 终端按依赖顺序启动）
│   ├── start_sim.sh       # 仿真快捷脚本（容器内自动转后台进程）
│   ├── circle_sim.sh / circle_real.sh   # 圆周运动一键演示（仿真/真机）
│   ├── elfin_env.sh       # 一键配好 ROS 环境（source 用）
│   ├── root_ros.sh        # 以 root 执行 ros2 命令
│   ├── wait_for_ros.sh    # 等待某个 ROS2 service/node 出现
│   └── legacy/            # 旧实机脚本与诊断文档归档
├── third_party/
│   └── windows_sdk/       # 华沿 V6 SDK（Windows 侧 TCP 直连控制柜，不经 ROS，勿放进 colcon 空间）
└── document/              # 官方文档 + 自研分析文档
```

命名约定：`cos_` 前缀 = 自研包；`elfin_` / `realsense_` 前缀 = 外部来源（厂家/官方）。

## 构建（顺序不能反）

```sh
~/cos_ws/elfin_ws/build_all.sh   # 1. 先构建厂家驱动层
~/cos_ws/app_ws/build_all.sh     # 2. 再构建自研应用层（自动 source elfin_ws）
```

日常使用时只需 source 最上层，elfin_ws 会被自动串接：

```sh
source ~/cos_ws/app_ws/install/setup.bash
# 或一步到位（含 ROS_LOCALHOST_ONLY=1）：
source ~/cos_ws/scripts/elfin_env.sh
```

## 快速开始（仿真）

```sh
# 启动 Gazebo + MoveIt2 + RViz（容器内自动以后台进程运行）
~/cos_ws/scripts/start_sim.sh            # 日志在 ~/cos_ws/app_ws/log/sim/
~/cos_ws/scripts/start_sim.sh stop       # 停止后台仿真

# 或使用多终端版本的主入口
python3 ~/cos_ws/scripts/start_sim.py                # 默认 elfin5
python3 ~/cos_ws/scripts/start_sim.py --elfin elfin10

# 如需 Elfin Control Panel GUI
~/cos_ws/scripts/start_sim.sh gazebo
```

官方仿真命令（等价手动方式，见 `document/README_cn.md`）：

```sh
source ~/cos_ws/app_ws/install/setup.bash
ros2 launch elfin5_ros2_moveit2 elfin5.launch.py          # Gazebo + MoveIt2 + RViz
ros2 launch elfin_basic_api fake_elfin_gui.launch.py      # Control Panel（仿真用 fake 版）
```

## 真机（EtherCAT）

实机包（`soem_ros2` / `elfin_ethercat_driver` / `elfin_ros_control` / `elfin_robot_bringup`）在 elfin_ws 内。
主入口为 `scripts/start_real.py`（4 个终端按依赖顺序启动，自动环境检查）：

```sh
python3 ~/cos_ws/scripts/start_real.py --check   # 只做环境检查
python3 ~/cos_ws/scripts/start_real.py           # 启动实机全栈
```

启动流程细节（需 PREEMPT_RT 内核）见 **`document/架构与数据流.md`** 真机章节，
旧脚本与诊断指令归档在 `scripts/legacy/`。

## 文档

- **`document/架构与数据流.md`** —— 全部包/节点/topic/service 清单、仿真与真机启动命令、数据流图（Obsidian/mermaid）
- **`document/已知问题.md`** —— 坑与断链清单（launch 死代码、硬编码网卡、标定注意事项等）
- `document/README_cn.md`、`API_description.md`、`moveit_plugin_tutorial.md` —— 华沿官方文档

## 说明

- `third_party/windows_sdk/` 是另一条控制栈：在 Windows 主机上通过 TCP
  （`192.168.10.10:10003`，V6 协议）直连控制柜，与 EtherCAT 栈相互独立，
  不经过 ROS，也不要放进任何 colcon 工作空间。
- 容器内没有 gnome-terminal，`start_sim.sh` 会自动切换为后台进程模式；
  在带桌面环境的 Linux 上运行则自动打开多个终端窗口。
