#!/usr/bin/env bash
# =====================================================================
# 启动 elfin5 仿真（MoveIt2 / Gazebo）
# 使用 Ubuntu 系统自带终端分别打开窗口，不依赖 VS Code。
#
# 用法:
#   ./start_sim.sh             同时打开 moveit2 与 gazebo 两个终端窗口
#   ./start_sim.sh moveit2     只打开 moveit2 终端窗口
#   ./start_sim.sh gazebo      只打开 gazebo 终端窗口
#
# 环境变量:
#   SIM_NO_SUDO=1   禁用 sudo（默认使用 `sudo -E`）
#   SIM_WS=<path>   覆盖工作空间路径（默认 ~/cos_ws/references）
# =====================================================================
set -euo pipefail

WS="${SIM_WS:-$HOME/cos_ws/references}"
SETUP="$WS/install/setup.bash"

# sudo 处理：默认 `sudo -E`；SIM_NO_SUDO=1 关闭；已是 root 自动关闭
SUDO="sudo -E"
[ "${SIM_NO_SUDO:-0}" = "1" ] && SUDO=""
[ "$(id -u)" = "0" ] && SUDO=""

need_setup() {
  if [ ! -f "$SETUP" ]; then
    echo "错误: 找不到 $SETUP" >&2
    echo "请先构建工作空间: cd $WS && ./build_all.sh" >&2
    exit 1
  fi
}

# 选择一个可用的系统终端模拟器
detect_terminal() {
  for t in gnome-terminal x-terminal-emulator xfce4-terminal konsole; do
    command -v "$t" >/dev/null 2>&1 && { echo "$t"; return 0; }
  done
  return 1
}

TERM_CMD="$(detect_terminal)" || {
  echo "错误: 未找到可用的图形终端模拟器（gnome-terminal / x-terminal-emulator 等）" >&2
  exit 1
}

# 在新终端窗口运行一个 ros2 launch（失败时窗口停留 5 秒便于查看错误）
#   open_term <窗口标题> <包名> <launch文件>
open_term() {
  local name="$1" pkg="$2" launch="$3"
  local inner
  if [ -n "$SUDO" ]; then
    # sudo 会重置 PATH（secure_path），必须在 sudo 内部重新 source 才能找到 ros2
    inner="sudo -E bash -c 'source $SETUP && ros2 launch $pkg $launch' || { echo; echo '>>> 启动失败，5 秒后关闭'; sleep 5; }"
  else
    inner="source '$SETUP' && ros2 launch $pkg $launch || { echo; echo '>>> 启动失败，5 秒后关闭'; sleep 5; }"
  fi
  case "$TERM_CMD" in
    gnome-terminal)
      gnome-terminal --title="$name" -- bash -c "$inner" &
      ;;
    x-terminal-emulator)
      x-terminal-emulator --title="$name" -- bash -c "$inner" &
      ;;
    xfce4-terminal)
      xfce4-terminal --title="$name" -x bash -c "$inner" &
      ;;
    konsole)
      konsole --title "$name" -e bash -c "$inner" &
      ;;
  esac
  echo "已打开终端窗口: $name"
}

case "${1:-}" in
  moveit2)
    need_setup
    open_term "🧪 moveit2" elfin5_ros2_moveit2 elfin5.launch.py
    ;;
  gazebo)
    need_setup
    open_term "🧪 gazebo" elfin_basic_api fake_elfin_gui.launch.py
    ;;
  ""|all)
    need_setup
    open_term "🧪 moveit2" elfin5_ros2_moveit2 elfin5.launch.py
    open_term "🧪 gazebo" elfin_basic_api fake_elfin_gui.launch.py
    ;;
  *)
    cat <<'HELP'
用法:
  ./start_sim.sh             同时打开 moveit2 与 gazebo 两个终端窗口
  ./start_sim.sh moveit2     只打开 moveit2 窗口
  ./start_sim.sh gazebo      只打开 gazebo 窗口

使用 Ubuntu 系统自带终端（gnome-terminal 等），无需 VS Code。
默认以 sudo -E 运行；如需禁用: SIM_NO_SUDO=1 ./start_sim.sh
环境变量: SIM_WS=<path> 可覆盖工作空间路径（默认 ~/cos_ws/references）
HELP
    ;;
esac