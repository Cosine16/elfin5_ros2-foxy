#!/usr/bin/env bash
# root_ros.sh —— 以 root 身份使用 ROS, 无需手动 source setup.bash
#
# 用法:
#   bash scripts/root_ros.sh                       # 打开一个已配好 ROS 环境的 root shell
#   bash scripts/root_ros.sh <命令...>             # 以 root 执行单条命令
#
# 示例:
#   bash scripts/root_ros.sh ros2 service list
#   bash scripts/root_ros.sh ros2 service call /enable_robot std_srvs/srv/SetBool "{data: true}"
#   bash scripts/root_ros.sh ros2 topic echo /fault_state
#
# 原理: sudo 会清空环境变量, 所以把 source elfin_env.sh 放进 root 的 bash -c 内部执行,
#       等效于 start_real.py 里 "sudo bash -c 'source ... && ros2 launch ...'" 的写法。
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/elfin_env.sh"

if [ ! -f "$ENV_FILE" ]; then
    echo "错误: 未找到 $ENV_FILE" >&2
    exit 1
fi

if [ $# -eq 0 ]; then
    # 交互式 root shell, 环境已配好, 直接敲 ros2 命令即可
    exec sudo bash -c "source '$ENV_FILE' && exec bash"
else
    # 单条命令模式: "$@" 由外层 bash -c 的参数位传入, 保持原始引号语义
    exec sudo bash -c "source '$ENV_FILE' && \"\$@\"" root-ros "$@"
fi
