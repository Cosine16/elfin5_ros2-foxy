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
# 说明:
#   - 请以【普通用户】运行本脚本（勿加 sudo）：gnome-terminal 无法以 root 打开窗口
#   - 仿真命令默认以普通用户执行，gzclient/rviz/GUI 才能正常显示
#   - 如确需以 root 执行仿真命令: SIM_SUDO=1 ./scripts/start_sim.sh
#
# 环境变量:
#   SIM_SUDO=1      以 `sudo -E` 执行仿真命令（默认普通用户运行）
#   SIM_WS=<path>   覆盖工作空间路径（默认 ~/cos_ws/references）
# =====================================================================
set -euo pipefail

# 拒绝以 root 运行：gnome-terminal 无法以 root 打开终端窗口
if [ "$(id -u)" = "0" ]; then
  echo "错误: 请勿用 sudo 运行本脚本 —— gnome-terminal 无法以 root 打开窗口。" >&2
  echo "请以普通用户运行: cd ~/cos_ws && ./scripts/start_sim.sh" >&2
  echo "（仿真命令默认普通用户执行，GUI 才能正常显示；如需 root 执行: SIM_SUDO=1）" >&2
  exit 1
fi

WS="${SIM_WS:-$HOME/cos_ws/references}"
SETUP="$WS/install/setup.bash"

# sudo 处理：默认【不使用】sudo（仿真 GUI 需普通用户才能正常显示）；
# 设置 SIM_SUDO=1 时才用 `sudo -E` 执行仿真命令。
SUDO=""
[ "${SIM_SUDO:-0}" = "1" ] && SUDO="sudo -E"

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
请以普通用户运行（勿加 sudo）；仿真命令默认普通用户执行，GUI 才能正常显示。
如需以 root 执行: SIM_SUDO=1 ./start_sim.sh
环境变量: SIM_WS=<path> 可覆盖工作空间路径（默认 ~/cos_ws/references）
HELP
    ;;
esac