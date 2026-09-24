#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
start_real.py
=============
根据 README_cn.md 的「使用真实的Elfin机器人」章节，自动打开多个终端窗口，
启动真实的 Elfin 机械臂。

每个终端中依次执行:
    1. source ~/cos_ws/app_ws/install/setup.bash  (overlay, 自动串接 elfin_ws)
    2. sudo + ros2 launch <实机命令>

启动的 4 个终端:
    1. 硬件驱动(实时优先级):  sudo chrt 10 bash -c "source ... && ros2 launch <model>_ros2_moveit2 <model>_moveit.launch.py"
    2. MoveIt! + RViz:        ros2 launch <model>_ros2_moveit2 <model>_moveit_rviz.launch.py
    3. 后台程序 basic_api:    ros2 launch <model>_ros2_moveit2 <model>_basic_api.launch.py
    4. Elfin Control Panel:   ros2 launch elfin_basic_api elfin_gui.launch.py

用法:
    python3 start_real.py                    # 默认: Elfin5 实机, 用户态运行(推荐)
    python3 start_real.py --elfin elfin5     # 启动 Elfin5 实机
    python3 start_real.py --sudo             # 旧行为: 全部终端以 root 运行
    python3 start_real.py --no-wait          # 不做依赖等待, 直接启动各终端
    python3 start_real.py --wait-timeout 180 # 设置依赖等待超时(秒), 默认 120
    python3 start_real.py --check            # 只检查环境是否就绪, 不启动终端
    python3 start_real.py --list             # 只打印命令, 不启动终端
    python3 start_real.py --workspace ~/cos_ws/app_ws   # 指定工作空间路径

启动顺序(严格 1→2→3→4, 后一步会等待前一步就绪):
    1. 硬件驱动(EtherCAT 主站 + controller_manager)
    2. MoveIt! + RViz (等待 controller_manager 就绪后 spawn 控制器)
    3. 后台程序 basic_api (等待 move_group 就绪)
    4. Elfin Control Panel (等待 basic_api 服务就绪)

注意:
    - 实机控制需要实时内核(PREEMPT_RT), 请先打好实时补丁。
    - 使用前先确认 elfin_robot_bringup/config/elfin_arm_control.yaml 中的
      elfin_ethernet_name 与实际连接 Elfin 的网卡名称一致。
    - 把购买时得到的 elfin_drivers.yaml 放到 elfin_robot_bringup/config/ 下,
      并把参数复制到 elfin_arm_control.yaml 的 ros__parameters 下。
    - 默认用户态运行: 硬件终端经 `sudo capsh` 以普通用户身份持有
      cap_net_raw/cap_sys_nice (ambient capabilities)。全图同用户,
      可避免 root 节点的 DDS 共享内存段(0644)挡住普通用户终端。
      若环境异常可用 --sudo 回退旧行为(全部终端 root)。
    - 使用 capsh/--sudo 时硬件终端会提示输入密码, 请在该终端中手动输入。
    - 关闭机械臂电源前, 请先在 Elfin Control Panel 界面按 "Servo Off" 去使能。
"""

import argparse
import getpass
import os
import shlex
import shutil
import subprocess
import sys

# ---------------------------------------------------------------------------
# 可配置项
# ---------------------------------------------------------------------------
CATKIN_WS = os.path.expanduser("~/cos_ws/app_ws")  # 自研 overlay 工作空间(可被 --workspace 覆盖)
DRIVER_WS = os.path.expanduser("~/cos_ws/elfin_ws")  # 厂家驱动层 underlay
SETUP_SCRIPT = os.path.join(CATKIN_WS, "install", "setup.bash")  # 会自动串接 DRIVER_WS
BRINGUP_CONFIG = os.path.join(
    DRIVER_WS, "src", "elfin_robot", "elfin_robot_bringup", "config"
)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))  # 本脚本所在目录 (cos_ws/scripts)
# 依赖等待辅助脚本在上一级 scripts/ 目录; 若被移动则回退到同目录
WAIT_HELPER = os.path.join(SCRIPT_DIR, os.pardir, "wait_for_ros.sh")
if not os.path.isfile(WAIT_HELPER):
    WAIT_HELPER = os.path.join(SCRIPT_DIR, "wait_for_ros.sh")

# 每个终端启动前的就绪门控, 与 REAL_COMMANDS 的顺序一一对应。
# 格式: (名称, 类型 service|node, 匹配串); None 表示该步骤无需等待(第一个)。
WAIT_SPECS = [
    None,                                                               # 1. 硬件驱动: 第一个, 无需等待
    ("controller_manager", "service", "/controller_manager/list_controllers"),  # 2. 等控制管理器就绪
    ("move_group", "node", "move_group"),                               # 3. 等 MoveIt move_group 就绪
    ("basic_api", "service", "/get_reference_link"),                    # 4. 等 basic_api 服务就绪
]

# 实机启动命令。键为机型前缀, 值为 (终端标题, "ros2 launch 后面的参数", 是否硬件驱动) 列表。
# 内容来自 README_cn.md 的 "使用真实的Elfin机器人" 章节
# (README 中 elfin3_ros2_moveit / elfin_ros2_moveit2 为笔误, 实际包名统一为 <model>_ros2_moveit2)。
REAL_COMMANDS = {
    "elfin3": [
        ("硬件驱动 (EtherCAT, capsh 用户态)", "elfin3_ros2_moveit2 elfin3_moveit.launch.py", True),
        ("MoveIt! + RViz", "elfin3_ros2_moveit2 elfin3_moveit_rviz.launch.py", False),
        ("后台程序 basic_api", "elfin3_ros2_moveit2 elfin3_basic_api.launch.py", False),
        ("Elfin Control Panel", "elfin_basic_api elfin_gui.launch.py", False),
    ],
    "elfin5": [
        ("硬件驱动 (EtherCAT, capsh 用户态)", "elfin5_ros2_moveit2 elfin5_moveit.launch.py", True),
        ("MoveIt! + RViz", "elfin5_ros2_moveit2 elfin5_moveit_rviz.launch.py", False),
        ("后台程序 basic_api", "elfin5_ros2_moveit2 elfin5_basic_api.launch.py", False),
        ("Elfin Control Panel", "elfin_basic_api elfin_gui.launch.py", False),
    ],
    "elfin5_l": [
        ("硬件驱动 (EtherCAT, capsh 用户态)", "elfin5_l_ros2_moveit2 elfin5_l_moveit.launch.py", True),
        ("MoveIt! + RViz", "elfin5_l_ros2_moveit2 elfin5_l_moveit_rviz.launch.py", False),
        ("后台程序 basic_api", "elfin5_l_ros2_moveit2 elfin5_l_basic_api.launch.py", False),
        ("Elfin Control Panel", "elfin_basic_api elfin_gui.launch.py", False),
    ],
    "elfin10": [
        ("硬件驱动 (EtherCAT, capsh 用户态)", "elfin10_ros2_moveit2 elfin10_moveit.launch.py", True),
        ("MoveIt! + RViz", "elfin10_ros2_moveit2 elfin10_moveit_rviz.launch.py", False),
        ("后台程序 basic_api", "elfin10_ros2_moveit2 elfin10_basic_api.launch.py", False),
        ("Elfin Control Panel", "elfin_basic_api elfin_gui.launch.py", False),
    ],
    "elfin10_l": [
        ("硬件驱动 (EtherCAT, capsh 用户态)", "elfin10_l_ros2_moveit2 elfin10_l_moveit.launch.py", True),
        ("MoveIt! + RViz", "elfin10_l_ros2_moveit2 elfin10_l_moveit_rviz.launch.py", False),
        ("后台程序 basic_api", "elfin10_l_ros2_moveit2 elfin10_l_basic_api.launch.py", False),
        ("Elfin Control Panel", "elfin_basic_api elfin_gui.launch.py", False),
    ],
    "elfin15": [
        ("硬件驱动 (EtherCAT, capsh 用户态)", "elfin15_ros2_moveit2 elfin15_moveit.launch.py", True),
        ("MoveIt! + RViz", "elfin15_ros2_moveit2 elfin15_moveit_rviz.launch.py", False),
        ("后台程序 basic_api", "elfin15_ros2_moveit2 elfin15_basic_api.launch.py", False),
        ("Elfin Control Panel", "elfin_basic_api elfin_gui.launch.py", False),
    ],
}

# 常用的终端模拟器, 按优先级选择 (Ubuntu 20.04 默认 gnome-terminal)
TERMINALS = ["gnome-terminal", "konsole", "xterm", "xfce4-terminal", "tilix"]


# ---------------------------------------------------------------------------
# 构建命令
# ---------------------------------------------------------------------------
def build_payload(launch_args, wait_spec, wait_timeout, no_wait):
    """构建实际执行的命令体: source -> cd -> (等待依赖) -> ros2 launch。"""
    parts = [
        # DDS 只走回环: 实机网卡被 EtherCAT 占用, 组播发现会超时导致节点互不可见。
        # (终端以 bash -c 非交互方式运行, 不会读 ~/.bashrc, 必须显式导出)
        "export ROS_LOCALHOST_ONLY=1",
        "source {}".format(shlex.quote(SETUP_SCRIPT)),
        "cd {}".format(shlex.quote(CATKIN_WS)),
    ]
    if not no_wait and wait_spec:
        name, kind, pattern = wait_spec
        parts.append("bash {} {} {} {} {}".format(
            shlex.quote(WAIT_HELPER),
            shlex.quote(name),
            kind,
            shlex.quote(pattern),
            wait_timeout,
        ))
    parts.append("ros2 launch {}".format(launch_args))
    return " && ".join(parts)


def build_launch_command(launch_args, is_hardware, use_sudo,
                         wait_spec=None, wait_timeout=120, no_wait=False):
    """构建执行 ros2 launch 的命令。

    默认（use_sudo=False）:
    - 硬件驱动终端: `sudo capsh --keep=1 --user=<当前用户> --inh/addamb
      cap_net_raw,cap_sys_nice` —— 进程仍以普通用户身份运行，但带
      EtherCAT 裸套接字与实时调度所需 capability。全图同用户，
      避免 root 节点的 Fast DDS 共享内存段(/dev/shm/fastrtps_* 0644)
      挡住普通用户终端（跨用户发现/通信静默失败）。
      注意不能像 setcap 二进制那样授权: 带 file caps 的进程会被 glibc
      置于 secure-execution 模式并清除 LD_LIBRARY_PATH, Foxy 的
      rcpputils::find_library_path 依赖该变量, rmw 会加载失败。
    - 其他终端: 直接以当前用户运行。
    use_sudo=True (--sudo): 旧行为，全部以 root 运行
    （硬件终端 `sudo chrt 10`，其余 `sudo -E`）。
    - 若配置了 wait_spec, 会在 launch 前等待依赖服务/节点就绪
    """
    payload = build_payload(launch_args, wait_spec, wait_timeout, no_wait)
    if use_sudo:
        if is_hardware:
            return "sudo chrt 10 bash -c {}".format(shlex.quote(payload))
        return "sudo -E bash -c {}".format(shlex.quote(payload))
    if is_hardware:
        user = getpass.getuser()
        home = os.path.expanduser("~")
        inner = "export HOME={}; {}".format(shlex.quote(home), payload)
        return ("sudo capsh --keep=1 --user={} "
                "--inh=cap_net_raw,cap_sys_nice "
                "--addamb=cap_net_raw,cap_sys_nice -- -c {}").format(
                    shlex.quote(user), shlex.quote(inner))
    return payload


def build_terminal_command(launch_args, is_hardware, use_sudo,
                           wait_spec=None, wait_timeout=120, no_wait=False):
    """构建终端内执行的完整命令: source -> cd -> (等待) -> launch -> 保持打开。"""
    parts = [
        build_launch_command(launch_args, is_hardware, use_sudo,
                             wait_spec, wait_timeout, no_wait),
        "echo",
        "echo '=== 进程已退出, 按任意键关闭本终端 ==='",
        "read -n1",
    ]
    return " && ".join(parts)


# ---------------------------------------------------------------------------
# 环境检查
# ---------------------------------------------------------------------------
def get_ethercat_interface():
    """从 elfin_arm_control.yaml 中读取 elfin_ethernet_name。"""
    yaml_path = os.path.join(BRINGUP_CONFIG, "elfin_arm_control.yaml")
    try:
        import yaml
        with open(yaml_path) as f:
            data = yaml.safe_load(f)
        return (data or {}).get("/**", {}).get("ros__parameters", {}).get(
            "elfin_ethernet_name"
        )
    except Exception:
        return None


def interface_is_up(name):
    """检查指定网卡是否存在且处于 UP 状态。"""
    try:
        out = subprocess.check_output(
            ["ip", "-o", "link", "show", name],
            stderr=subprocess.DEVNULL, text=True,
        )
        return "LOWER_UP" in out or "state UP" in out
    except Exception:
        return False


def kernel_is_rt():
    """粗略判断是否为实时内核 (uname -r 含 rt / 或 uname -v 含 PREEMPT RT)。"""
    try:
        release = subprocess.check_output(["uname", "-r"], text=True).strip()
        version = subprocess.check_output(["uname", "-v"], text=True)
        if "rt" in release.lower():
            return True, release
        if "PREEMPT RT" in version or "PREEMPT_RT" in version:
            return True, release
        return False, release
    except Exception:
        return False, "unknown"


def check_environment(verbose=True):
    """检查实机启动前环境是否就绪, 返回 True 表示全部通过。"""
    ok = True

    # 1. 工作空间是否编译
    if os.path.isfile(SETUP_SCRIPT):
        if verbose:
            print("[OK] 工作空间 setup: {}".format(SETUP_SCRIPT))
    else:
        ok = False
        print("[!!] 未找到 {}, 请先 colcon build".format(SETUP_SCRIPT))

    # 2. elfin_drivers.yaml 是否存在
    drivers = os.path.join(BRINGUP_CONFIG, "elfin_drivers.yaml")
    if os.path.isfile(drivers):
        if verbose:
            print("[OK] 驱动参数文件: {}".format(drivers))
    else:
        ok = False
        print("[!!] 未找到 {} (请把购买时得到的 elfin_drivers.yaml 放到该目录)".format(drivers))

    # 3. EtherCAT 网卡是否存在且启用
    iface = get_ethercat_interface()
    if iface:
        if interface_is_up(iface):
            if verbose:
                print("[OK] EtherCAT 网卡 {}: 已启用".format(iface))
        else:
            ok = False
            print("[!!] EtherCAT 网卡 {} 不存在或未启用, "
                  "请修改 elfin_arm_control.yaml 的 elfin_ethernet_name".format(iface))
    else:
        ok = False
        print("[!!] 无法从 elfin_arm_control.yaml 读取 elfin_ethernet_name")

    # 4. 实时内核 (仅提示, 不阻断)
    rt, release = kernel_is_rt()
    if rt:
        if verbose:
            print("[OK] 实时内核: {}".format(release))
    else:
        print("[!!] 当前内核 {} 可能不是实时内核(PREEMPT_RT), "
              "实机控制需要实时性支持".format(release))

    # 5. capsh 可用 (硬件终端用户态授权 cap_net_raw/cap_sys_nice 的工具)
    if shutil.which("capsh"):
        if verbose:
            print("[OK] capsh 可用 (硬件终端将以普通用户 + ambient capabilities 运行)")
    else:
        ok = False
        print("[!!] 未找到 capsh (sudo apt install libcap2-bin), "
              "或改用 --sudo 回退全 root 模式")

    return ok


# ---------------------------------------------------------------------------
# 打开终端
# ---------------------------------------------------------------------------
def open_terminal(title, full_command):
    """在系统默认终端模拟器中新开一个终端并执行命令。"""
    for t in TERMINALS:
        terminal = shutil.which(t)
        if not terminal:
            continue
        if t == "gnome-terminal":
            subprocess.Popen(
                [terminal, "--title", title, "--", "bash", "-c", full_command]
            )
        elif t == "konsole":
            subprocess.Popen(
                [terminal, "--new-tab", "-p", "tabtitle={}".format(title),
                 "-e", "bash", "-c", full_command]
            )
        elif t == "xterm":
            subprocess.Popen(
                [terminal, "-T", title,
                 "-e", "bash -c {}".format(shlex.quote(full_command))]
            )
        else:  # xfce4-terminal / tilix
            subprocess.Popen(
                [terminal, "--title", title, "-e", "bash -c {}".format(shlex.quote(full_command))]
            )
        return
    sys.exit("错误: 未找到可用的终端模拟器 (gnome-terminal/konsole/xterm 等), 请先安装其中一个。")


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    global CATKIN_WS, SETUP_SCRIPT

    parser = argparse.ArgumentParser(
        description="根据 README_cn.md 自动启动 Elfin 机器人实机终端",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="示例:\n"
               "  python3 start_real.py\n"
               "  python3 start_real.py --check\n"
               "  python3 start_real.py --no-sudo\n"
               "  python3 start_real.py --elfin elfin5\n"
               "  python3 start_real.py --list",
    )
    parser.add_argument("--elfin", default="elfin5",
                        choices=list(REAL_COMMANDS.keys()),
                        help="机器人机型前缀, 默认 elfin5")
    parser.add_argument("--sudo", action="store_true",
                        help="以 root 执行实机命令（旧行为）。默认不需要："
                             "ros2_control_node 已授予 cap_net_raw/cap_sys_nice，"
                             "全用户态运行可避免 root↔普通用户 DDS 共享内存段权限问题")
    parser.add_argument("--check", action="store_true",
                        help="只检查环境是否就绪, 不启动终端")
    parser.add_argument("--list", action="store_true",
                        help="只打印将要执行的命令, 不启动终端")
    parser.add_argument("--workspace", default=CATKIN_WS,
                        help="工作空间路径, 默认 ~/cos_ws/app_ws")
    parser.add_argument("--wait-timeout", type=int, default=120,
                        help="等待上一步依赖就绪的超时秒数, 默认 120")
    parser.add_argument("--no-wait", action="store_true",
                        help="不做依赖等待, 直接按顺序启动各终端")
    args = parser.parse_args()

    CATKIN_WS = os.path.expanduser(args.workspace)
    SETUP_SCRIPT = os.path.join(CATKIN_WS, "install", "setup.bash")

    print("=" * 70)
    print("Elfin 实机启动器")
    print("工作空间: {}".format(CATKIN_WS))
    print("机型    : {}".format(args.elfin))
    print("使用sudo: {}".format("是" if args.sudo else "否"))
    print("=" * 70)

    # 环境检查
    print("\n--- 环境检查 ---")
    env_ok = check_environment()
    print("环境检查结果: {}".format("通过" if env_ok else "存在问题(见上方 [!!] 提示)"))

    if args.check:
        print("\n(--check 模式, 未启动终端)")
        sys.exit(0 if env_ok else 1)

    # 打印 / 启动命令 (严格按 1→2→3→4 顺序, 每步等待上一步就绪)
    use_sudo = args.sudo
    commands = REAL_COMMANDS[args.elfin]
    wait_mode = "关(不等待)" if args.no_wait else "开(超时 {}s)".format(args.wait_timeout)
    print("\n依赖等待: {}".format(wait_mode))
    print("\n将按顺序启动 {} 个终端:\n".format(len(commands)))

    for idx, ((title, launch_args, is_hardware), wait_spec) in enumerate(
            zip(commands, WAIT_SPECS), start=1):
        full = build_terminal_command(
            launch_args, is_hardware, use_sudo,
            wait_spec, args.wait_timeout, args.no_wait,
        )
        print("=" * 70)
        print("[{}] {}{}".format(idx, title,
                                 "" if wait_spec is None or args.no_wait
                                 else "  (先等待 {})".format(wait_spec[0])))
        print(full)
        if not args.list:
            open_terminal(title, full)

    print("\n" + "=" * 70)
    if args.list:
        print("以上为将要执行的命令 (--list 模式, 未启动终端)。")
    else:
        print("所有终端已启动。默认用户态运行, 无需输入密码; --sudo 模式才需要。")
        print("各终端会按 1→2→3→4 的依赖顺序自行等待就绪后再启动。")
        print("启动完成后: 在 Elfin Control Panel 界面先按 'Clear Fault' 清错, "
              "再按 'Servo On' 使能。")
        print("关闭电源前, 请先按 'Servo Off' 去使能。")


if __name__ == "__main__":
    main()
