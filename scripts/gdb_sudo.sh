#!/usr/bin/env bash
# VS Code cppdbg 调试器包装脚本。
# ros2_control_node 以 root 运行(sudo chrt 10 bash 启动),普通用户的 gdb
# 没有权限 attach,这里经 sudo -n 提权。
#
# 前提(一次性,见 tasks.json 的 "setup: allow sudo gdb"):
#   /etc/sudoers.d/99-gdb-nopasswd 中对 /usr/bin/gdb 配置了 NOPASSWD,
#   否则 sudo -n 会直接失败(VS Code 调试通道里无法交互输密码)。
exec sudo -n /usr/bin/gdb "$@"
