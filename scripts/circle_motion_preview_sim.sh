#!/usr/bin/env bash
# =====================================================================
# circle_motion_preview_sim.sh —— 先规划再执行的仿真启动脚本
#
# 用法:
#   ./circle_motion_preview_sim.sh
#   ./circle_motion_preview_sim.sh stop
#
# 流程:
#   1. 若 elfin5 仿真未启动，则调用 start_sim.sh moveit2 复用已有仿真入口
#   2. 等待 move_group 就绪
#   3. 在当前终端中前台启动 circle_motion_preview 节点
#
# 启动后需要手动发送两步命令：
#   ros2 topic pub --once /circle_motion_preview/enable std_msgs/msg/Bool "{data: true}"
#   ros2 topic pub --once /circle_motion_preview/execute std_msgs/msg/Bool "{data: true}"
#
# 这样在 RViz 中先能看到黄色规划轨迹，确认后再执行圆周运动。
# =====================================================================
set -euo pipefail

export ROS_LOCALHOST_ONLY=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WS="${SIM_WS:-$HOME/cos_ws/elfin_ws}"
SETUP="$WS/install/setup.bash"

if [ ! -f "$SETUP" ]; then
  echo "错误: 找不到 $SETUP" >&2
  echo "请先构建工作空间: cd $WS && ./build_all.sh" >&2
  exit 1
fi
if [ ! -d "$WS/install/cos_shape" ]; then
  echo "错误: cos_shape 未构建，请先: cd $WS && ./build_all.sh" >&2
  exit 1
fi

if [ "${1:-}" = "stop" ]; then
  pkill -f "circle_motion_preview" 2>/dev/null || true
  echo "已停止 preview 节点。"
  exit 0
fi

if [ -n "${WSL_DISTRO_NAME:-}" ] && [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
  echo "[preview-sim] 检测到 WSL 环境且未配置 GUI 显示，脚本将改为后台模式运行。"
fi

if ! pgrep -f "ros2 launch elfin5_ros2_moveit2.*elfin5.launch.py" >/dev/null 2>&1; then
  echo "[preview-sim] 启动 elfin5 仿真（MoveIt2 + RViz）..."
  bash "$SCRIPT_DIR/start_sim.sh" moveit2
fi

set +u
source "$SETUP"
set -u
bash "$SCRIPT_DIR/wait_for_ros.sh" move_group node move_group 180

echo "[preview-sim] 启动 circle_motion_preview 节点（先规划展示，等待 /execute）..."
exec ros2 launch cos_shape circle_motion_preview.launch.py
