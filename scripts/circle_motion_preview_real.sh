#!/usr/bin/env bash
# =====================================================================
# circle_motion_preview_real.sh —— 真实机启动脚本（先规划预览，再执行）
#
# 用法:
#   ./circle_motion_preview_real.sh
#   ./circle_motion_preview_real.sh --check
#   ./circle_motion_preview_real.sh --no-sudo
#
# 流程:
#   1. 调用 start_real.py 启动真实机各终端（驱动、MoveIt2、basic_api、GUI）
#   2. 等待 move_group 就绪
#   3. 在当前终端前台启动 circle_motion_preview 节点
#
# 启动后需要手动发送：
#   ros2 topic pub --once /circle_motion_preview/enable std_msgs/msg/Bool "{data: true}"
#   ros2 topic pub --once /circle_motion_preview/execute std_msgs/msg/Bool "{data: true}"
#
# 实机建议先做：Clear Fault → Servo On，然后再执行 preview / execute。
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

if [ "${1:-}" = "--check" ]; then
  exec python3 "$SCRIPT_DIR/start_real.py" --check
fi

echo "[preview-real] 启动实机终端（硬件驱动 → MoveIt2 + RViz → basic_api → 面板）..."
python3 "$SCRIPT_DIR/start_real.py" "$@"

set +u
source "$SETUP"
set -u
bash "$SCRIPT_DIR/wait_for_ros.sh" move_group node move_group 300

echo "[preview-real] 启动 circle_motion_preview 节点（先规划预览，等待 /execute）..."
echo "[preview-real] 请先在 Elfin Control Panel 中 Clear Fault / Servo On，然后再 enable + execute。"
exec ros2 launch cos_shape circle_motion_preview.launch.py
