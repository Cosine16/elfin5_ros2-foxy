#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
merge_compile_commands.py
=========================
把 colcon/CMake 为每个包各自生成的编译数据库合并成一份工作空间级的文件，
供 clangd (及 VS Code clangd 扩展) 使用。

背景:
    CMake 只会把 compile_commands.json 写到自己的构建目录里:
        build/<包名>/compile_commands.json
    colcon 不会自动合并，所以工具默认看不到整个工作空间的编译参数。
    本脚本把 build/*/compile_commands.json 合并成:
        <工作空间>/compile_commands.json      (即 elfin_ws/compile_commands.json)
    .clangd 里通过 `CompilationDatabase: elfin_ws` 指向这个目录即可。

前置条件:
    构建时必须带 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    (elfin_ws/build_all.sh 已经带该参数)。

用法:
    python3 merge_compile_commands.py                  # 合并本脚本所在的工作空间
    python3 merge_compile_commands.py --ws ~/ws_ros2/cos_ws/elfin_ws
    python3 merge_compile_commands.py --out /tmp/cc.json
    python3 merge_compile_commands.py --quiet           # 只报错, 不打印明细

注意:
    合并结果包含绝对路径，属于本机产物，不要提交到版本库
    (仓库 .gitignore 已忽略 compile_commands.json)。
"""

import argparse
import glob
import json
import os
import sys

# 本脚本所在目录 = 工作空间根目录 (elfin_ws)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_WS = SCRIPT_DIR


def load_entries(path):
    """读取一个 compile_commands.json，返回条目列表。"""
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if isinstance(data, dict):          # 极少数工具会写成 {"commands": [...]}
        data = data.get("commands", [])
    if not isinstance(data, list):
        raise ValueError("不是合法的编译数据库格式")
    return data


def merge(ws, out_path, quiet=False):
    """合并 ws/build/*/compile_commands.json -> out_path，返回 (条目数, 成功文件数)。"""
    pattern = os.path.join(ws, "build", "*", "compile_commands.json")
    sources = sorted(glob.glob(pattern))
    # 排除 build/compile_commands.json 自身(它不匹配上面的两级通配, 但以防万一)
    sources = [p for p in sources if os.path.abspath(p) != os.path.abspath(out_path)]

    if not sources:
        print("[merge] 错误: 未找到 {}。\n"
              "[merge] 请先带 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 构建"
              " (直接运行 build_all.sh 即可)。".format(pattern), file=sys.stderr)
        return None

    merged = {}
    ok_files = 0
    details = []
    for src in sources:
        pkg = os.path.basename(os.path.dirname(src))
        try:
            entries = load_entries(src)
        except Exception as exc:                     # json 解析失败 / 文件被截断
            details.append((pkg, None, str(exc)))
            continue
        ok_files += 1
        details.append((pkg, len(entries), None))
        for e in entries:
            # 以源文件绝对路径为键去重: 同一文件重复出现时, 后出现的覆盖先出现的
            key = os.path.abspath(e.get("file", ""))
            if key:
                merged[key] = e
            else:
                # 没有 file 字段的条目直接丢弃(非法条目)
                pass

    if not merged:
        print("[merge] 错误: 所有编译数据库都是空的, 未生成 {}".format(out_path),
              file=sys.stderr)
        return None

    # 稳定排序, 便于 diff
    result = [merged[k] for k in sorted(merged.keys())]

    os.makedirs(os.path.dirname(os.path.abspath(out_path)) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, sort_keys=True)
        f.write("\n")

    if not quiet:
        print("[merge] 合并 {} 个编译数据库 -> {}".format(ok_files, out_path))
        for pkg, count, err in details:
            if err:
                print("        {:<28} !! 跳过 ({})".format(pkg, err))
            else:
                print("        {:<28} {} 条".format(pkg, count))
        print("[merge] 合计 {} 个源文件条目".format(len(result)))
    return len(result), ok_files


def main():
    parser = argparse.ArgumentParser(
        description="合并 colcon 各包的 compile_commands.json 为工作空间级编译数据库")
    parser.add_argument("--ws", default=DEFAULT_WS,
                        help="工作空间根目录, 默认本脚本所在目录")
    parser.add_argument("--out", default=None,
                        help="输出文件路径, 默认 <工作空间>/compile_commands.json")
    parser.add_argument("--quiet", action="store_true", help="只报错, 不打印明细")
    args = parser.parse_args()

    ws = os.path.abspath(os.path.expanduser(args.ws))
    out = os.path.abspath(os.path.expanduser(args.out)) if args.out \
        else os.path.join(ws, "compile_commands.json")

    if not os.path.isdir(os.path.join(ws, "build")):
        print("[merge] 错误: {} 下没有 build/ 目录, 请先构建。".format(ws),
              file=sys.stderr)
        return 1

    return 0 if merge(ws, out, args.quiet) else 1


if __name__ == "__main__":
    sys.exit(main())
