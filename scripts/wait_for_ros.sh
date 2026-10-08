#!/usr/bin/env bash
# wait_for_ros.sh —— 等待某个 ROS2 service 或 node 出现, 用于实机启动时按依赖顺序排队。
#
# 用法: wait_for_ros.sh <名称> <类型: service|node> <匹配串> [超时秒=120]
#
# 示例:
#   wait_for_ros.sh controller_manager service /controller_manager/list_controllers 120
#   wait_for_ros.sh move_group node move_group 120
#
# 说明: 该脚本需要在一个已经 source 过 ROS 环境的 shell 中运行 (ros2 命令可用)。
set -u

NAME="${1:?用法: wait_for_ros.sh <名称> <service|node> <匹配串> [超时秒]}"
TYPE="${2:?}"
PATTERN="${3:?}"
TIMEOUT="${4:-120}"

case "$TYPE" in
  service) LIST_CMD=(ros2 service list) ;;
  node)    LIST_CMD=(ros2 node list) ;;
  *) echo "[wait] 未知类型 '$TYPE' (只支持 service|node)"; exit 2 ;;
esac

echo "[wait] 等待 $NAME ($TYPE: $PATTERN) 就绪, 最多 ${TIMEOUT}s ..."
i=0
while [ "$i" -lt "$TIMEOUT" ]; do
  if "${LIST_CMD[@]}" 2>/dev/null | grep -q -- "$PATTERN"; then
    echo "[wait] OK: $NAME 已就绪 (约 ${i}s)"
    exit 0
  fi
  i=$((i + 2))
  sleep 2
done

echo "[wait] !! 超时: $NAME 在 ${TIMEOUT}s 内未就绪, 仍将继续启动 (可 Ctrl+C 中止)"
exit 0
