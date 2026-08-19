#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
calib_zero.py
=============
Elfin 机械臂原点校准脚本 —— 解决 "仿真模型与实机回传数据对不上" 的问题。

原理 (来自驱动源码):
    position = -(count - count_zero) / count_rad_factor
    count_rad_factor = reduction_ratio * 131072 / (2*pi)

因此, 在已知实际关节角度 θ 的姿态下, 正确原点为:
    count_zero = raw_count + θ_rad * count_rad_factor

用法 (机械臂必须已连接、驱动已启动):
    1. 把机械臂摆到一个你确切知道各关节实际角度的姿态。
    2. 运行其一:
       python3 calib_zero.py --angles 0,0,0,0,0,0      # 实际角度(J1..J6, 度)
       python3 calib_zero.py --at-home                 # 当前姿态即原点(全部 0 度)
    3. 脚本读取驱动回传的原始编码器计数, 反算 count_zeros,
       自动写入 elfin_arm_control.yaml 与 elfin_drivers.yaml
       (自动按配置顺序 [J2, J1, J3, J4, J5, J6] 排列)。
    4. 脚本会询问是否立即重新编译 elfin_robot_bringup。

注意:
    - 读取编码器计数需要 root 权限(节点以 root 运行), 会弹一次 sudo 密码。
    - 校准完成后需重启硬件驱动终端, RViz 模型即与实际对齐。
"""

import argparse
import os
import re
import subprocess
import sys

WS = os.path.expanduser("~/cos_ws/ros2_ws")
BRINGUP_SRC = os.path.join(WS, "src", "elfin_robot", "elfin_robot_bringup", "config")
ARM_YAML = os.path.join(BRINGUP_SRC, "elfin_arm_control.yaml")
DRIVERS_YAML = os.path.join(BRINGUP_SRC, "elfin_drivers.yaml")


def load_yaml_params(path):
    import yaml
    with open(path) as f:
        data = yaml.safe_load(f)
    return (data or {}).get("/**", {}).get("ros__parameters", {})


def read_raw_counts():
    """调用 /get_current_position 服务, 返回按配置顺序的原始计数列表。"""
    cmd = (
        "source /opt/ros/foxy/setup.bash && "
        "source {}/install/setup.bash && "
        "ros2 service call /get_current_position std_srvs/srv/SetBool "
        "\"{{data: true}}\"".format(WS)
    )
    # 节点以 root 运行, 需要 sudo; 使用 -E 保留环境
    out = subprocess.check_output(["sudo", "-E", "bash", "-c", cmd], text=True)
    if "success=True" not in out:
        sys.exit("读取失败:\n{}".format(out))

    # 每个从站返回格式: slaveN_current_position:\naxis1: X, axis2: Y.
    pairs = re.findall(r"axis1:\s*(-?\d+)\s*,\s*axis2:\s*(-?\d+)", out)
    counts = []
    for a1, a2 in pairs:
        counts.extend([int(a1), int(a2)])
    return counts


def main():
    parser = argparse.ArgumentParser(description="Elfin 原点校准 (count_zeros)")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--angles", default=None,
                       help="当前姿态各关节实际角度, J1..J6 自然顺序, 逗号分隔, 单位度。如: 0,0,-90,0,0,0")
    group.add_argument("--at-home", action="store_true",
                       help="当前姿态就是原点姿态(各关节实际角度均为 0)")
    parser.add_argument("--no-build", action="store_true",
                        help="只写配置文件, 不自动重新编译")
    args = parser.parse_args()

    params = load_yaml_params(ARM_YAML)
    joint_names = params.get("joint_names")
    reduction_ratios = params.get("reduction_ratios")
    axis_factors = params.get("axis_position_factors")
    slave_no = params.get("slave_no")
    old_zeros = params.get("count_zeros")
    if not all([joint_names, reduction_ratios, axis_factors, slave_no]):
        sys.exit("错误: 无法从 {} 读取完整参数".format(ARM_YAML))

    import math
    factors = [r * f / (2 * math.pi) for r, f in zip(reduction_ratios, axis_factors)]

    # 实际角度 (J1..J6 自然顺序)
    if args.at_home:
        angles_deg = [0.0] * 6
    else:
        try:
            angles_deg = [float(x.strip()) for x in args.angles.split(",")]
        except ValueError:
            sys.exit("错误: --angles 必须是逗号分隔的 6 个数字")
        if len(angles_deg) != 6:
            sys.exit("错误: 需要 6 个关节角度 (J1..J6), 实际给了 {} 个".format(len(angles_deg)))

    angles_rad = {i + 1: math.radians(a) for i, a in enumerate(angles_deg)}  # {J1: rad, ...}

    print("读取原始编码器计数 (可能需要 sudo 密码) ...")
    counts = read_raw_counts()
    if len(counts) != 6:
        sys.exit("错误: 期望 6 个计数, 实际解析到 {} 个: {}".format(len(counts), counts))

    print("\n" + "=" * 70)
    print("从站数: {} (slave_no={})".format(len(slave_no), slave_no))
    print("配置顺序: {}".format(joint_names))
    print("旧 count_zeros: {}".format(old_zeros))
    print("=" * 70)

    # counts 顺序即配置顺序 [J2, J1, J3, J4, J5, J6]
    new_zeros = []
    print("\n关节     原始计数        实际角度      count_rad_factor  新 count_zero")
    for name, c, f in zip(joint_names, counts, factors):
        jnum = int(name.replace("elfin_joint", ""))
        theta = angles_rad[jnum]
        cz = c + theta * f
        new_zeros.append(int(round(cz)))
        print("{:<8} {:<15} {:>8.2f}°   {:>16.0f}   {}".format(
            name.replace("elfin_", ""), c, math.degrees(theta), f, int(round(cz))))

    new_zeros_str = "[{}]".format(", ".join(str(v) for v in new_zeros))
    print("\n新 count_zeros (按 [J2, J1, J3, J4, J5, J6] 顺序):")
    print("    count_zeros: " + new_zeros_str)

    # 写入两个 yaml
    for path in (ARM_YAML, DRIVERS_YAML):
        with open(path) as f:
            content = f.read()
        new_line = "        count_zeros: " + new_zeros_str + "  # 顺序 [J2, J1, J3, J4, J5, J6]"
        content, n = re.subn(r"^(\s*)count_zeros:.*$", new_line, content,
                             flags=re.MULTILINE)
        if n != 1:
            sys.exit("错误: 在 {} 中未找到/找到多个 count_zeros 行".format(path))
        with open(path, "w") as f:
            f.write(content)
        print("已写入: {}".format(path))

    if not args.no_build:
        print("\n重新编译 elfin_robot_bringup ...")
        subprocess.run(
            "source /opt/ros/foxy/setup.bash && "
            "cd {} && colcon build --packages-select elfin_robot_bringup".format(WS),
            shell=True, executable="/bin/bash", check=True)
        print("编译完成。")
        print("下一步: 重启硬件驱动终端 (start_real.py 终端 1), 再起其余终端。")
    else:
        print("(--no-build: 跳过编译, 请自行 colcon build --packages-select elfin_robot_bringup)")


if __name__ == "__main__":
    main()
