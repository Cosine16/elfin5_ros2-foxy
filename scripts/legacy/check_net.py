#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_net.py
============
排查直连 Elfin 机械臂(EtherCAT)时的网络问题。

流程:
    1. 读取 elfin_arm_control.yaml 中配置的 EtherCAT 网卡 (或 --iface 指定),
       检查网卡是否启用、链路是否连通、IP/子网是否正确。
    2. 用 nmap 扫描该网卡所在子网, 找出在线主机。
    3. 对在线主机 (或 --target 指定的 IP) 扫描端口, 判断服务是否可达。
    4. 输出 ARP 邻居表, 帮助定位链路层问题。

用法:
    python3 check_net.py                           # 自动检测网卡并扫描其子网
    python3 check_net.py --iface eth0              # 手动指定网卡
    python3 check_net.py --target 192.168.1.100    # 只扫描指定 IP 的端口
    python3 check_net.py --ports 1-1000            # 指定端口范围 (默认 1-1024)
    python3 check_net.py --syn                     # 使用 SYN 扫描 (需要 sudo)
    python3 check_net.py --arp                     # 只看 ARP 邻居表, 不做 nmap
    python3 check_net.py --no-nmap                 # 不依赖 nmap, 只做基础检查

前提:
    完整扫描需要安装 nmap:
        sudo apt-get install nmap

提示:
    - EtherCAT 从站通常是链路层设备, 没有 IP, 扫描不到属正常;
      关键是网卡链路状态(LOWER_UP)和主站能否在链路上收发数据帧。
    - 直连时建议给网卡配置静态 IP (例如 192.168.1.x/24),
      并与机械臂/控制柜的服务 IP 处于同一网段。
"""

import argparse
import os
import re
import shutil
import subprocess
import sys

# 配置文件的默认位置 (可通过 --workspace 覆盖)
DEFAULT_WS = os.path.expanduser("~/cos_ws/elfin_ws")
CONFIG_REL = os.path.join("src", "elfin_robot", "elfin_robot_bringup",
                          "config", "elfin_arm_control.yaml")


# ---------------------------------------------------------------------------
# 基础工具
# ---------------------------------------------------------------------------
def run(cmd, check=False):
    """执行命令并返回 (返回码, 标准输出)。"""
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
        return p.returncode, p.stdout.strip()
    except FileNotFoundError:
        return 127, ""
    except Exception as e:
        return 1, str(e)


def section(title):
    print("\n" + "=" * 70)
    print(title)
    print("=" * 70)


# ---------------------------------------------------------------------------
# 网卡信息
# ---------------------------------------------------------------------------
def get_configured_iface(workspace):
    """从 elfin_arm_control.yaml 读取 elfin_ethernet_name。"""
    path = os.path.join(workspace, CONFIG_REL)
    if not os.path.isfile(path):
        return None, path
    try:
        import yaml
        with open(path) as f:
            data = yaml.safe_load(f)
        iface = (data or {}).get("/**", {}).get("ros__parameters", {}).get(
            "elfin_ethernet_name")
        return iface, path
    except Exception:
        return None, path


def iface_link_info(iface):
    """返回网卡的链路信息, 不存在则返回 None。"""
    rc, out = run(["ip", "-o", "link", "show", iface])
    if rc != 0 or not out:
        return None
    # 2: enp2s0: <BROADCAST,MULTICAST,PROMISC,UP,LOWER_UP> mtu 1500 ... state UP ...
    m = re.search(r"<\S+>\s+mtu\s+(\d+).*state\s+(\w+)", out)
    link_up = "LOWER_UP" in out
    carrier = "NO-CARRIER" not in out
    mac = ""
    mm = re.search(r"link/ether\s+([0-9a-f:]+)", out)
    if mm:
        mac = mm.group(1)
    return {
        "mtu": m.group(1) if m else "?",
        "state": m.group(2) if m else "?",
        "link_up": link_up,
        "carrier": carrier,
        "mac": mac,
    }


def iface_ip_info(iface):
    """返回网卡的 IPv4 地址 (ip/prefix), 无则返回 None。"""
    rc, out = run(["ip", "-o", "-f", "inet", "addr", "show", iface])
    if rc != 0 or not out:
        return None
    m = re.search(r"inet\s+(\d+\.\d+\.\d+\.\d+)/(\d+)", out)
    if not m:
        return None
    return m.group(1), int(m.group(2))


def subnet_of(ip, prefix):
    """根据 IP 和前缀长度计算子网 CIDR。"""
    import ipaddress
    try:
        return str(ipaddress.ip_network("{}/{}".format(ip, prefix), strict=False))
    except Exception:
        return None


# ---------------------------------------------------------------------------
# nmap 扫描
# ---------------------------------------------------------------------------
def nmap_available():
    return shutil.which("nmap") is not None


def nmap_ping_scan(subnet):
    """nmap -sn 发现在线主机, 返回 [(ip, hostname)]。"""
    rc, out = run(["nmap", "-sn", subnet, "-oG", "-"])
    hosts = []
    for line in out.splitlines():
        if "Status: Up" in line:
            m = re.match(r"Host:\s+(\S+)\s+\(([^)]*)\)", line)
            if m:
                hosts.append((m.group(1), m.group(2)))
            else:
                m2 = re.match(r"Host:\s+(\S+)", line)
                if m2:
                    hosts.append((m2.group(1), ""))
    return hosts


def nmap_port_scan(target, ports, syn=False):
    """扫描目标端口, 返回打开的端口列表 [(port, service)]。"""
    scan_type = "-sS" if syn else "-sT"
    cmd = ["nmap", scan_type, "-p", ports, "--open", "-oG", "-", target]
    if not syn:  # 非 root 时避免慢的额外探测, 更快
        cmd.insert(2, "-Pn")
    rc, out = run(cmd)
    opened = []
    for line in out.splitlines():
        if "Ports:" in line:
            m = re.search(r"Ports:\s*(.+?)(?:\s+Ignored|\s*$)", line)
            if m:
                for part in m.group(1).split(","):
                    part = part.strip()
                    # nmap -oG 端口格式: 23/open/tcp//telnet///
                    mm = re.match(r"(\d+)/(\w+)/(\w+)/([^/]*)", part)
                    if mm and mm.group(2) == "open":
                        service = mm.group(4) or mm.group(3)
                        opened.append((mm.group(1), service))
    return opened


# ---------------------------------------------------------------------------
# ARP 邻居表
# ---------------------------------------------------------------------------
def show_arp(iface):
    rc, out = run(["ip", "neigh", "show", "dev", iface])
    print("邻居表 (dev {}) :".format(iface))
    if not out:
        print("  (空 - 链路上没有发现其它设备)")
        return
    for line in out.splitlines():
        print("  " + line)


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="排查直连 Elfin 机械臂时的网络问题 (nmap 扫描)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="提示: 需要安装 nmap (sudo apt-get install nmap)。",
    )
    parser.add_argument("--iface", default=None,
                        help="指定网卡名称 (默认读取 elfin_arm_control.yaml 配置)")
    parser.add_argument("--workspace", default=DEFAULT_WS,
                        help="工作空间路径 (用于定位配置文件), 默认 ~/cos_ws/elfin_ws")
    parser.add_argument("--target", default=None,
                        help="只扫描指定 IP 的端口, 不扫描整个子网")
    parser.add_argument("--ports", default="1-1024",
                        help="端口范围, 默认 1-1024")
    parser.add_argument("--syn", action="store_true",
                        help="使用 SYN 扫描 (需要 sudo)")
    parser.add_argument("--arp", action="store_true",
                        help="只看 ARP 邻居表, 不运行 nmap")
    parser.add_argument("--no-nmap", action="store_true",
                        help="不依赖 nmap, 只做基础检查")
    args = parser.parse_args()

    # 1. 确定网卡
    iface = args.iface
    cfg_path = None
    if not iface:
        iface, cfg_path = get_configured_iface(args.workspace)
        if iface:
            print("配置文件 {} -> 网卡: {}".format(cfg_path, iface))
        else:
            iface = "eth0"
            print("警告: 未能从配置读取网卡, 回退使用 eth0 "
                  "(可用 --iface 指定)")

    # 2. 链路信息
    section("1. 网卡链路状态")
    link = iface_link_info(iface)
    if link is None:
        print("!! 网卡 {} 不存在。".format(iface))
        print("   请用 ip link show 查看可用网卡, 并检查接线 / 驱动。")
        sys.exit(1)
    print("网卡   : {}".format(iface))
    print("MAC    : {}".format(link["mac"] or "?"))
    print("状态   : {}  (链路: {})".format(link["state"],
                                          "已连接" if link["carrier"] else "未连接"))
    if not link["carrier"]:
        print("!! 链路未连接 (NO-CARRIER): 请检查网线是否插好、"
              "对端设备(机械臂/控制柜)是否上电。")

    ip_info = iface_ip_info(iface)
    if ip_info:
        ip, prefix = ip_info
        print("IPv4   : {}/{}".format(ip, prefix))
        if prefix != 24:
            print("提示: 直连时建议使用 /24 网段并配置静态 IP。")
    else:
        print("!! 网卡没有 IPv4 地址。")
        print("   直连建议: sudo ip addr add 192.168.1.2/24 dev {} ; "
              "sudo ip link set {} up".format(iface, iface))

    if args.no_nmap:
        section("2. ARP 邻居表 (基础模式)")
        show_arp(iface)
        print("\n(--no-nmap 模式, 未运行 nmap)")
        return

    # 3. nmap 扫描
    if not nmap_available():
        section("2. 缺少 nmap")
        print("未检测到 nmap, 请先安装:  sudo apt-get install nmap")
        print("或在扫描前使用基础模式:  python3 check_net.py --no-nmap")
        print("\n(仅完成基础检查)")
        show_arp(iface)
        return

    targets = []
    if args.target:
        targets = [args.target]
    else:
        if not ip_info:
            print("\n!! 网卡无 IP, 无法计算子网进行扫描。")
            print("   可先配置 IP (见上方提示), 或用 --target 指定目标。")
            show_arp(iface)
            return
        subnet = subnet_of(ip_info[0], ip_info[1])
        if not subnet:
            print("!! 无法计算子网。")
            return
        section("2. 子网主机发现 (nmap -sn {})".format(subnet))
        print("正在扫描, 请稍候...")
        hosts = nmap_ping_scan(subnet)
        if not hosts:
            print("未发现在线主机。")
            print("提示: EtherCAT 从站通常没有 IP, 扫描不到属正常; "
                  "关键是链路是否 LOWER_UP。")
        else:
            print("发现 {} 个在线主机:".format(len(hosts)))
            for ip, name in hosts:
                print("  {}  ({})".format(ip, name if name else "无主机名"))
            targets = [ip for ip, _ in hosts]

    # 4. 端口扫描
    section("3. 端口扫描 (端口 {})".format(args.ports))
    if not targets:
        print("没有可扫描的目标。")
    for t in targets:
        print("\n-- 扫描 {} --".format(t))
        opened = nmap_port_scan(t, args.ports, syn=args.syn)
        if not opened:
            print("  未发现开放端口 (或目标不可达)。")
        else:
            for port, service in opened:
                print("  {}/tcp  open  {}".format(port, service))
        if not args.syn and os.geteuid() == 0:
            print("  (检测到以 root 运行, 可加 --syn 使用更快的 SYN 扫描)")

    # 5. ARP 邻居表
    section("4. ARP 邻居表")
    show_arp(iface)

    print("\n" + "=" * 70)
    print("检查完成。如果链路正常但仍连不上, 请核对:")
    print("  - elfin_arm_control.yaml 的 elfin_ethernet_name 是否就是当前网卡")
    print("  - 网卡 IP 与机械臂/控制柜服务 IP 是否在同一网段")
    print("  - 是否已按 README 配置好 elfin_drivers.yaml")


if __name__ == "__main__":
    main()
