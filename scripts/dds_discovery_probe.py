#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dds_discovery_probe.py
======================
DDS 节点发现探针 —— 用于快速判断「某两个 ROS 进程能不能互相看见」。

背景:
    本项目实测确认 **root 与 fit 的 DDS 参与者互相发现不了**(详见
    document/排查证据-面板与驱动通信调查.md 证据 13)。症状是普通用户跑
    rqt / ros2 node list 一片空白, 而节点其实都在跑。
    用本探针可以在 1 分钟内按 uid 组合做对照, 不必依赖现场实机。

原理:
    起一个 rclpy 节点, spin N 秒收集 get_node_names(), 打印结果。
    比 `ros2 node list` 可靠 —— 不走 ros2 daemon 缓存。

用法:
    # 基本: 起一个名为 ProbeA 的节点, 观察 15 秒
    source /opt/ros/foxy/setup.bash
    python3 scripts/dds_discovery_probe.py ProbeA 15

    # 对照实验(两条命令要同时跑, 用 & 或两个终端):
    #   同 uid 应当互发现; 跨 uid 应当互不可见
    ROS_DOMAIN_ID=42 python3 scripts/dds_discovery_probe.py FitA 15 &
    sleep 3
    ROS_DOMAIN_ID=42 python3 scripts/dds_discovery_probe.py FitB 12

    sudo -n bash -c 'source /opt/ros/foxy/setup.bash; export ROS_DOMAIN_ID=43; \
                     python3 scripts/dds_discovery_probe.py RootA 15' &
    sleep 3
    ROS_DOMAIN_ID=43 python3 scripts/dds_discovery_probe.py FitC 12

建议:
    - 换一个**干净的 ROS_DOMAIN_ID** 做实验, 避开现场残留的 /dev/shm/fastrtps_* 段。
    - 两侧的 ROS_DOMAIN_ID 与 ROS_LOCALHOST_ONLY 必须一致。
    - 启动间隔 ≥3s, spin 时间 ≥12s(Fast DDS 首轮发现需要时间)。

退出码: 恒为 0(本工具只报事实, 不判定成败), 结果看打印的节点数。
"""

import argparse
import sys
import time

try:
    import rclpy
except ImportError:
    sys.exit("错误: 导入 rclpy 失败, 请先 source /opt/ros/foxy/setup.bash")


def main():
    parser = argparse.ArgumentParser(
        description="DDS 节点发现探针: spin N 秒, 打印发现了哪些节点")
    parser.add_argument("name", nargs="?", default="discovery_probe",
                        help="本节点名, 默认 discovery_probe")
    parser.add_argument("seconds", nargs="?", type=float, default=15.0,
                        help="观察时长(秒), 默认 15")
    args = parser.parse_args()

    rclpy.init()
    node = rclpy.create_node(args.name)
    try:
        end = time.time() + args.seconds
        while time.time() < end:
            rclpy.spin_once(node, timeout_sec=0.2)
        found = sorted(set(node.get_node_names()))
        others = [n for n in found if n != args.name]
        print("[{}] 发现 {} 个节点(含自己): {}".format(
            args.name, len(found), found), flush=True)
        if others:
            print("[{}] ✅ 除自己外还看到: {}".format(args.name, others), flush=True)
        else:
            print("[{}] ❌ 除自己外一个都没看到"
                  " —— 与对端不通(跨 uid? 变量不一致? 窗口太短?)".format(args.name),
                  flush=True)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
