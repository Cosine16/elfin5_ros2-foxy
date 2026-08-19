#!/usr/bin/env bash
# =====================================================================
# 生产环境（裸机 Ubuntu 20.04 LTS）一键安装脚本
#
# 与开发容器 Dockerfile 共用同一份依赖清单（改动请两边同步）。
#
# 用法：
#   chmod +x setup_ubuntu2004.sh
#   ./setup_ubuntu2004.sh [cos_ws目标目录]     # 默认 ~/cos_ws
#
# 完成后：
#   source ~/cos_ws/elfin_ws/install/setup.bash
#   ~/cos_ws/scripts/start_sim.sh
# =====================================================================
set -euo pipefail

WS_TARGET="${1:-$HOME/cos_ws}"
REPO_URL="git@github.com:Cosine16/cos_ws.git"

# ---- 0. 环境检查 ----
if [ -r /etc/os-release ]; then
  . /etc/os-release
  if [ "${VERSION_ID:-}" != "20.04" ]; then
    echo "警告: 当前系统不是 Ubuntu 20.04（检测到 ${PRETTY_NAME:-unknown}），继续可能失败。" >&2
  fi
fi
SUDO=""
[ "$(id -u)" != "0" ] && SUDO="sudo"

# ---- 1. ROS2 Foxy apt 源 ----
$SUDO apt-get update
$SUDO apt-get install -y curl gnupg2 lsb-release
$SUDO curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
http://packages.ros.org/ros2/ubuntu $(lsb_release -cs) main" \
  | $SUDO tee /etc/apt/sources.list.d/ros2.list > /dev/null

# ---- 2. 依赖（与 Dockerfile 中的清单保持一致）----
$SUDO apt-get update
$SUDO apt-get install -y \
  gazebo11 \
  ros-foxy-gazebo-ros \
  ros-foxy-gazebo-ros2-control \
  ros-foxy-moveit \
  ros-foxy-ros2-control \
  ros-foxy-ros2-controllers \
  ros-foxy-xacro \
  ros-foxy-tf2-tools \
  ros-foxy-trajectory-msgs \
  ros-foxy-demo-nodes-cpp \
  ros-foxy-demo-nodes-py \
  build-essential git curl \
  python3-colcon-common-extensions python3-colcon-mixin \
  python3-rosdep python3-vcstool python3-pip \
  fonts-wqy-zenhei mesa-utils \
  libgtk-3-0 libsm6 libxxf86vm1 libjpeg-turbo8 libtiff5 libsdl2-2.0-0

# wxPython（官方 extras 的 focal 预编译包，cp38 最高 4.2.1）
pip3 install --user \
  -f https://extras.wxpython.org/wxPython4/extras/linux/gtk3/ubuntu-20.04/ \
  wxPython==4.2.1

# ---- 3. NVIDIA 显卡（可选，GTX1050 渲染加速）----
# 裸机有 N 卡且需要 GPU 渲染时，取消下一行注释：
# $SUDO ubuntu-drivers autoinstall   # 或: apt-get install -y nvidia-driver-535

# ---- 4. 拉代码并构建 ----
if [ ! -d "$WS_TARGET/.git" ]; then
  git clone "$REPO_URL" "$WS_TARGET"
fi
"$WS_TARGET/elfin_ws/build_all.sh"

echo
echo "=== 安装完成 ==="
echo "source $WS_TARGET/elfin_ws/install/setup.bash"
echo "$WS_TARGET/scripts/start_sim.sh"
