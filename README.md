# cos_ws — Elfin5 机械臂开发仓库

在华沿（Huayan）Elfin5（E05-1195_Pro，6 轴 5kg）官方 ROS2 支持包的基础上，
搭建 Gazebo 仿真 + MoveIt2 运动规划环境，并承载后续自研应用。

## 运行环境

- 主机：Ubuntu20.04 + ros2 foxy

## 目录结构

```
cos_ws/
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
│   └── legacy/            # 实机（EtherCAT）脚本与诊断文档（start_real.py / calib_zero.py 等）
├── windows_sdk/           # 华沿 C++ SDK（Windows 主机侧，MinGW/DLL，与容器无关）
└── document/              # 官方文档 + 自研分析文档
```

## 快速开始

在仓库根目录下
```bash
python3 ./scripts/start_sim.py

python3 ./scripts/start_real.py
```

