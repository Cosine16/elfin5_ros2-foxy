# ROS2 环境分析与工作空间迁移笔记

> 记录时间：2026-08-18
> 涉及：Elfin5 机械臂仿真从 `~/catkin_ws` 迁移到 `~/cos_ws/ros2_ws`，以及 ROS2 Foxy 常见问题排查经验。

---

## 一、背景

原 ROS2 Foxy 工作空间为 `~/catkin_ws`（目录名沿用了 ROS1 习惯，实际是 ROS2 colcon 工作空间），
其中 `src/elfin_robot` 下包含多个型号（elfin3/elfin5/elfin5_l/elfin10/elfin10_l/elfin15）的 Gazebo 与 MoveIt2 包，
以及真实硬件（EtherCAT）驱动包。

**目标**：将 Elfin5 仿真相关的包迁移到新工作空间 `~/cos_ws/ros2_ws`，使其独立运行，
并让 `.bashrc` 与启动脚本不再依赖旧的 `~/catkin_ws`。

---

## 二、Elfin5 仿真依赖分析

通过阅读各包 `package.xml` 与 launch 文件，确定 Elfin5 仿真的**最小必要包集合**：

| 包 | 作用 | 必要性 |
|----|------|--------|
| `elfin5_ros2_gazebo` | Gazebo 仿真：world、urdf、控制器配置、launch | ✅ 必须 |
| `elfin5_ros2_moveit2` | MoveIt2 规划：srdf、kinematics、ompl、controllers、launch | ✅ 必须 |
| `elfin_description` | 机器人描述：meshes（STL）、urdf、rviz 配置 | ✅ 必须（urdf 用 `$(find elfin_description)/meshes/elfin5/*.STL`） |
| `elfin_basic_api` | 控制面板 GUI + basic_api 节点 | ✅ 必须（`fake_elfin_gui.launch.py`、`elfin5_basic_api.launch.py` 依赖） |
| `elfin_robot_msgs` | 消息/服务定义（srv） | ✅ 必须（`elfin_basic_api` 通过 `find_package` 依赖） |

**可舍弃的包**（属于其他型号或真实硬件，Elfin5 仿真的 launch 均未引用）：
- `elfin3/`、`elfin10/`、`elfin10_l/`、`elfin15/`、`elfin5_l/` —— 其他型号
- `elfin_ethercat_driver`、`elfin_ros_control`、`elfin_robot_bringup`、`soem_ros2` —— 真实硬件（EtherCAT）驱动

**依赖关系链**：

```
elfin5_ros2_moveit2 ──┐
                      ├──> elfin5_ros2_gazebo ──> elfin_description (meshes/urdf)
                      └──> elfin_basic_api ──────> elfin_robot_msgs (srv 定义)
```

---

## 三、迁移步骤

### 3.1 复制源码包

```bash
mkdir -p ~/cos_ws/ros2_ws/src/elfin_robot/elfin5
cp -r ~/catkin_ws/src/elfin_robot/elfin5/elfin5_ros2_gazebo   ~/cos_ws/ros2_ws/src/elfin_robot/elfin5/
cp -r ~/catkin_ws/src/elfin_robot/elfin5/elfin5_ros2_moveit2  ~/cos_ws/ros2_ws/src/elfin_robot/elfin5/
cp -r ~/catkin_ws/src/elfin_robot/elfin_description            ~/cos_ws/ros2_ws/src/elfin_robot/
cp -r ~/catkin_ws/src/elfin_robot/elfin_basic_api              ~/cos_ws/ros2_ws/src/elfin_robot/
cp -r ~/catkin_ws/src/elfin_robot/elfin_robot_msgs             ~/cos_ws/ros2_ws/src/elfin_robot/
```

### 3.2 干净环境构建

> ⚠️ **关键教训**：colcon 生成的 `install/setup.bash` 会把**构建时环境中的 underlay 链**硬编码进去。
> 如果构建时 shell 里 source 了旧工作空间，新工作空间的 setup 链就会被旧路径污染。

**正确做法** —— 用独立脚本 + 清空环境变量：

```bash
# ~/cos_ws/ros2_ws/build_all.sh
#!/usr/bin/env bash
set -e
unset AMENT_PREFIX_PATH COLCON_PREFIX_PATH CMAKE_PREFIX_PATH ROS_PACKAGE_PATH \
      ROS_LOCALHOST_ONLY ROS_DOMAIN_ID RMW_IMPLEMENTATION PYTHONPATH LD_LIBRARY_PATH \
      PKG_CONFIG_PATH COLCON_DEFAULTS_FILE 2>/dev/null || true
export HOME=/home/fit
export PATH=/usr/bin:/bin:/usr/local/bin
rm -rf build install log
source /opt/ros/foxy/setup.bash
cd /home/fit/cos_ws/ros2_ws
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release
```

```bash
chmod +x ~/cos_ws/ros2_ws/build_all.sh
nohup setsid bash ~/cos_ws/ros2_ws/build_all.sh > /tmp/build_all.log 2>&1 < /dev/null &
```

**构建结果**：5 个包全部成功（`elfin_basic_api` 有少量无害 warning）。

### 3.3 更新 .bashrc

```bash
# 修改前
# source ~/catkin_ws/install/setup.bash
# 修改后
source ~/cos_ws/ros2_ws/install/setup.bash
```

> 修改前建议备份：`cp ~/.bashrc ~/.bashrc.bak_$(date +%Y%m%d_%H%M%S)`

### 3.4 更新 cos_ws 启动脚本

`~/cos_ws/scripts/ros2_setup/` 下这些文件硬编码了 `~/catkin_ws`，已批量替换为 `~/cos_ws/ros2_ws`：

- `start_sim.py` / `start_real.py` / `check_net.py` / `calib_zero.py`
- `opcode.bash` / `enable_sim_in_catkinws.bash` / `诊断指令.md`

---

## 四、验证

source 新工作空间后：

```bash
ros2 pkg list | grep elfin
# 应只输出：
#   elfin5_ros2_gazebo
#   elfin5_ros2_moveit2
#   elfin_basic_api
#   elfin_description
#   elfin_robot_msgs

echo $AMENT_PREFIX_PATH   # 应只含 ~/cos_ws/ros2_ws/install/* 和 /opt/ros/foxy
```

---

## 五、附：排查过程中发现的 ROS2 常见坑

### 5.1 turtlesim 键盘无效 / 100% CPU 空转

**现象**：`turtle_teleop_key` 100% CPU，按键盘无反应，海龟不动。

**原因**：Foxy 版 `turtle_teleop_key` 源码在 `read()` 失败时会 `continue` 死循环忙转，
当它运行在 **stdin 不是真正 tty** 的环境（管道、后台、代理终端）时就会这样。

**判断标准**：
- 正常状态：CPU 低（约 1~2%）、进程状态为 `Ss`（阻塞等待输入）
- 异常状态：CPU 100%、状态 `R`

**结论**：程序本身没问题，必须在真正的交互式终端运行 teleop。

### 5.2 ros2 node list 为空

可能原因与处理：
1. 节点确实没启动 → 先启动节点
2. **daemon 缓存损坏** → `ros2 daemon stop && rm -rf ~/.ros && ros2 daemon start`
3. 环境不一致 → 检查 `ROS_DOMAIN_ID`、`RMW_IMPLEMENTATION` 是否统一
4. 多网卡/虚拟化环境多播失败 → 统一设置 `export ROS_LOCALHOST_ONLY=1`

### 5.3 节点可见但话题不可见（发现图撕裂）

**现象**：`ros2 node list` 能看到节点，但 `ros2 topic list` 看不到该节点的话题。

**原因**：ROS daemon 缓存损坏。
**处理**：清理 `~/.ros` 并重启 daemon（同 5.2）。

### 5.4 后台/管道运行 ROS2 交互程序

`ros2 run turtlesim turtle_teleop_key` 等需要交互式输入的程序，
**不能**用 `&` 后台、管道、`nohup` 等方式启动，否则会收到 `SIGTTIN` 停止或 100% CPU 忙转。

---

## 六、colcon 构建要点

1. **setup 链污染**：构建环境里的 underlay 会被写进 `install/setup.bash`。务必在干净环境构建。
2. **override 检查**：如果新工作空间与旧 underlay 有同名包，colcon 会告警
   `Some selected packages are already built in one or more underlay workspaces`，
   可用 `--allow-overriding <pkg>` 强制覆盖，但更推荐在干净环境构建避免该情况。
3. **后台构建防中断**：长编译建议用 `nohup setsid bash script.sh ... &` 脱离终端，
   避免终端交互（SIGINT/SIGHUP）中断构建。
4. **常见参数**：
   - `--packages-select <pkg>` 只构建指定包
   - `--cmake-args -DCMAKE_BUILD_TYPE=Release` 发行版编译
   - `--event-handlers console_direct+` 实时输出日志
