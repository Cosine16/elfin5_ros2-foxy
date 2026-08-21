#!/usr/bin/env bash
# =====================================================================
# check_real.sh —— 真机接入后的只读诊断脚本（不动机械臂、不需要互联网）
#
# 用法:
#   bash ~/cos_ws/scripts/legacy/check_real.sh
#
# 前提: 网线已插到控制柜、控制柜已上电 1~2 分钟、
#       硬件驱动已启动（start_real.py 终端 1 或 elfin_ros2_ethercat.launch.py）
#
# 逐项检查并在最后给出结论与下一步指令。
# =====================================================================
set -u

WS="$HOME/cos_ws/elfin_ws"
YAML="$WS/src/elfin_robot/elfin_robot_bringup/config/elfin_arm_control.yaml"

export ROS_LOCALHOST_ONLY=1
source /opt/ros/foxy/setup.bash 2>/dev/null
source "$WS/install/setup.bash" 2>/dev/null

PASS=0; FAIL=0
ok()   { echo "  [PASS] $1"; PASS=$((PASS+1)); }
bad()  { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }
info() { echo "  [info] $1"; }

echo "== 1. 环境与网卡 =="
if [ "${ROS_LOCALHOST_ONLY:-0}" = "1" ]; then ok "ROS_LOCALHOST_ONLY=1"; else bad "ROS_LOCALHOST_ONLY 未设置"; fi
if ip -br addr show enp2s0 2>/dev/null | grep -q UP; then
  ok "enp2s0 UP"
  ip -br addr show enp2s0 | grep -q "192\.168\.137\." \
    && info "enp2s0 还挂着 192.168.137.x 的 IP —— 若已插机械臂，建议: sudo nmcli device set enp2s0 managed no && sudo ip addr flush dev enp2s0 && sudo ip link set enp2s0 up"
else
  bad "enp2s0 不存在或未 UP"
fi

echo "== 2. 驱动进程 =="
if pgrep -f "ros2_control_node|elfin_ethercat_driver" >/dev/null; then
  ok "驱动进程在运行: $(pgrep -fc 'ros2_control_node|elfin_ethercat_driver') 个"
else
  bad "没有驱动进程 —— 先启动: python3 ~/cos_ws/scripts/legacy/start_real.py"
  echo; echo "驱动不在，后续检查跳过。"; exit 1
fi

echo "== 3. 驱动服务 =="
SVC=$(timeout 10 ros2 service list 2>/dev/null)
for s in /clear_fault /recognize_position /get_current_position /enable_robot; do
  echo "$SVC" | grep -q "^$s$" && ok "$s 可用" || bad "$s 不可用（驱动可能还在启动中，姿态识别阶段约需 40s，稍等重跑本脚本）"
done

echo "== 4. 原始编码器计数 vs count_zeros（真假连接判据）=="
OUT=$(timeout 20 ros2 service call /get_current_position std_srvs/srv/SetBool "{data: true}" 2>/dev/null)
echo "$OUT" | grep -oE "axis[12]: -?[0-9]+" | sed 's/^/  /'
# count_zeros 从 yaml 读出，顺序 [J2,J1,J3,J4,J5,J6]；从站顺序 slave2.axis1=J2 ...
CZ=$(grep -oP 'count_zeros:\s*\[\K[^\]]+' "$YAML" | tr -d ' ' | cut -d'#' -f1)
COUNTS=$(echo "$OUT" | grep -oP 'axis[12]:\s*\K-?[0-9]+' | paste -sd, -)
python3 - "$CZ" "$COUNTS" <<'EOF'
import sys
try:
    zeros  = [int(x) for x in sys.argv[1].split(',') if x]
    counts = [int(x) for x in sys.argv[2].split(',') if x]
except Exception:
    print("  [FAIL] 解析失败"); sys.exit(0)
if len(zeros) != 6 or len(counts) != 6:
    print("  [FAIL] 计数数量不对 zeros=%d counts=%d" % (len(zeros), len(counts))); sys.exit(0)
factor = 101.0 * 131072.0 / (2 * 3.141592653589793)  # counts/rad
print("  关节(配置序)  当前计数      count_zero   偏差角度")
worst = 0.0
for i, (c, z) in enumerate(zip(counts, zeros)):
    dev = abs(c - z) / factor * 180.0 / 3.141592653589793
    worst = max(worst, dev)
    print("  J%-6d      %-12d %-12d %7.2f°" % ([2,1,3,4,5,6][i], c, z, dev))
if worst < 3.0:
    print("  [PASS] 偏差均在 3° 内（机械臂应停在零位划线处）—— 真连接且标定正确")
elif worst < 15.0:
    print("  [warn] 有偏差(<15°)：机械臂可能没停在零位，或需 calib_zero.py 重标")
else:
    print("  [FAIL] 偏差巨大 —— 大概率没连机械臂（假连接）或 count_zeros 不对")
EOF

echo "== 5. /joint_states 合理性 =="
JS=$(timeout 10 ros2 topic echo /joint_states 2>/dev/null | grep -m1 -A6 "position:" | tail -6 | grep -oP -- '-?[0-9.]+' | paste -sd, -)
python3 - "$JS" <<'EOF'
import sys, math
try:
    vals = [float(x) for x in sys.argv[1].split(',') if x]
except Exception:
    vals = []
if len(vals) != 6:
    print("  [FAIL] /joint_states 无数据或关节数不对（10s 内没收到）")
else:
    print("  关节角度(rad):", ["%.3f" % v for v in vals])
    if all(abs(v) < math.pi for v in vals):
        print("  [PASS] 数值在 ±π 内（姿态识别成功）")
    else:
        print("  [FAIL] 存在超 ±π 的值 —— 姿态识别失败，勿使能！")
EOF

echo
echo "==================== 结论 ===================="
echo "PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -eq 0 ]; then
  echo "全部通过。可以继续在 Control Panel 做: Clear Fault → Servo On"
else
  echo "有 FAIL 项，按上方提示处理后重跑本脚本。"
  echo "姿态识别失败时手动重试: ros2 service call /recognize_position std_srvs/srv/SetBool \"{data: true}\""
fi
echo "=============================================="
