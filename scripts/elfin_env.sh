#!/usr/bin/env bash
# elfin_env.sh —— 一次性配好 Elfin 工作空间的 ROS 环境 (用 source 加载, 不要直接执行)
#
# 用法:
#   source ~/cos_ws/scripts/elfin_env.sh          # 普通用户终端
#   sudo bash -c 'source ~/cos_ws/scripts/elfin_env.sh && exec bash'   # root shell
#
# 内容:
#   1. source ROS2 Foxy 系统环境
#   2. source elfin_ws 的 install/setup.bash
#   3. export ROS_LOCALHOST_ONLY=1 (与 start_sim.py / start_real.py 保持一致,
#      否则看不到它们启动的节点和 service)

_ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_SETUP="$_ENV_DIR/../elfin_ws/install/setup.bash"

if [ ! -f "$_SETUP" ]; then
    echo "[elfin_env] 错误: 未找到 $_SETUP, 请先运行 elfin_ws/build_all.sh 编译" >&2
    return 1 2>/dev/null || exit 1
fi

source /opt/ros/foxy/setup.bash
source "$_SETUP"
export ROS_LOCALHOST_ONLY=1

unset _ENV_DIR _SETUP
