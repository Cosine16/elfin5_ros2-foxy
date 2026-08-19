# =====================================================================
# Elfin5 仿真开发环境（ROS2 Foxy + Gazebo 11 + MoveIt2，Ubuntu 20.04）
#
# 与生产裸机安装脚本 scripts/setup_ubuntu2004.sh 共用同一份依赖清单：
# 新增/变更依赖时请两边同步修改。
#
# 构建（Windows 侧，仓库根目录）：
#   docker compose build
# 单独构建：
#   docker build -t cos-elfin-foxy:dev .
# =====================================================================
FROM ros:foxy-ros-base-focal

ENV DEBIAN_FRONTEND=noninteractive

# ---- ROS / Gazebo / MoveIt / 构建工具（与 setup_ubuntu2004.sh 保持一致）----
RUN apt-get update && apt-get install -y --no-install-recommends \
    # 仿真与规划
    gazebo11 \
    ros-foxy-gazebo-ros \
    ros-foxy-gazebo-ros2-control \
    ros-foxy-moveit \
    ros-foxy-ros2-control \
    ros-foxy-ros2-controllers \
    ros-foxy-xacro \
    ros-foxy-tf2-tools \
    ros-foxy-trajectory-msgs \
    ros-foxy-demo-nodes-cpp \
    ros-foxy-demo-nodes-py \
    # 构建与开发工具
    build-essential git curl \
    python3-colcon-common-extensions python3-colcon-mixin \
    python3-rosdep python3-vcstool python3-pip \
    clangd tree procps poppler-utils \
    # GUI：中文字体 + glxinfo（验证 GPU 渲染是否生效）
    fonts-wqy-zenhei mesa-utils \
    # wxPython（fake_elfin_gui / cos_shape 调参面板）GTK3 运行时库
    libgtk-3-0 libsm6 libxxf86vm1 libjpeg-turbo8 libtiff5 libsdl2-2.0-0 \
 && rm -rf /var/lib/apt/lists/*

# wxPython：PyPI 不提供 Linux 轮子，使用官方 extras 的 focal 预编译包。
# 注：extras 源 focal/cp38 最高只到 4.2.1（当前容器里的 4.2.2 是源码编译的，
# patch 版本差异，fake_elfin_gui / cos_shape 面板只用基础 API，兼容）。
RUN pip3 install --no-cache-dir \
      -f https://extras.wxpython.org/wxPython4/extras/linux/gtk3/ubuntu-20.04/ \
      wxPython==4.2.1

# WSL2 GPU 直通（docker compose 里 gpus: all）后，mesa d3d12 驱动
# 需要从这里加载 Windows 侧注入的 DX12 库（libd3d12.so / libdxcore.so 等）
ENV LD_LIBRARY_PATH=/usr/lib/wsl/lib

WORKDIR /root/cos_ws
CMD ["bash"]
