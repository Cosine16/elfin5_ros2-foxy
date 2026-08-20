# Windows 侧重建 Elfin5 开发容器操作指引

> 读者：Windows 侧的 AI Agent（或人）。按步骤顺序执行，每步有验证标准，失败就停下报告，不要跳步、不要自行变通。

## 背景

旧开发容器存在三个问题：

1. 启动时没带 `--init`，PID 1 是 `tail -f /dev/null`，僵尸进程（`ros2 <defunct>`）无人回收；
2. 没有 GPU 直通（无 `/dev/dxg`），Gazebo/RViz 全部软件渲染（llvmpipe），CPU 吃满；
3. 代码 `/root/cos_ws` 放在容器的 overlay 可写层里，容器一删代码即丢。

仓库根目录已新增 `Dockerfile`、`docker-compose.yml`、`.dockerignore`、`scripts/setup_ubuntu2004.sh`（在 `feature/docker-dev-env` 分支，已包含在下面的 bundle 里）。本指引的目标：把代码落到 Windows 文件系统，用 compose 重建带 GPU 直通和 init 的新容器。

## 前置条件（先检查，不满足就先解决再继续）

- Windows 11 + Docker Desktop，使用 WSL2 后端（设置 → General → "Use the WSL 2 based engine"）。
- Windows 侧已安装较新的 NVIDIA 显卡驱动（GTX1050）。
- PowerShell 里 `docker version` 能正常输出。
- 旧容器**先不要删、不要停**（里面仿真还在跑没关系，本指引不依赖它，删它是最后一步）。

## 步骤 1：从 bundle 恢复代码到 Windows

完整仓库（含全部分支历史）已打包为 `C:\cos_ws-backup-20260819.bundle`。

```powershell
cd C:\
git clone C:\cos_ws-backup-20260819.bundle cos_ws
cd C:\cos_ws
git checkout feature/docker-dev-env
git remote set-url origin git@github.com:Cosine16/cos_ws.git
```

验证：`git log --oneline -2` 第一条应为 `122964d build: dockerized dev env (GPU + init) and bare-metal Ubuntu 20.04 setup`；`Test-Path Dockerfile, docker-compose.yml` 均为 True。

## 步骤 2：推送分支到 GitHub（代码双保险）

```powershell
git push -u origin feature/docker-dev-env
```

验证：`git ls-remote origin feature/docker-dev-env` 有输出。推送失败（网络/认证）不阻塞后续步骤，但必须明确报告。

## 步骤 3：验证 GPU 直通可用

```powershell
docker run --rm --gpus all ubuntu:22.04 ls /dev/dxg /usr/lib/wsl/lib
```

通过标准：输出包含 `/dev/dxg`。若报错（`could not select device driver` 之类），先更新 NVIDIA 驱动和 Docker Desktop 再重试，不要带病继续。

## 步骤 4：启动 Windows 侧 X server

二选一（如果已有可用 X server 且勾选了硬件 OpenGL，跳过）：

- VcXsrv：`vcxsrv -ac -wgl`（`-wgl` 让 OpenGL 渲染走 Windows 侧 GPU；`-ac` 关闭访问控制）
- X410：开启 Native OpenGL

## 步骤 5：构建并启动新容器

```powershell
cd C:\cos_ws
docker compose build
docker compose up -d
```

- 首次 build 约 10-20 分钟（要装 Gazebo11/MoveIt2）。**build 报错就把完整错误日志带回来**，不要自己改 Dockerfile。
- 若 compose 报 `gpus` 字段不认识（Compose 版本过老），先升级 Docker Desktop；仍不行就报告，不要自行改 yaml。

验证：`docker ps` 能看到 `elfin_sim`，镜像 `cos-elfin-foxy:dev`。

## 步骤 6：容器内验证

用 VS Code "Dev Containers: Attach to Running Container" 附加到 `elfin_sim`，或 `docker exec -it elfin_sim bash`，依次执行：

```bash
# 1) GPU 设备已注入
ls /dev/dxg /usr/lib/wsl/lib

# 2) 渲染器不再是 llvmpipe（应为 D3D12 (NVIDIA ...) 或经 wgl 的 Windows GPU）
glxinfo -B | grep -i renderer

# 3) PID 1 是 init（tini），不再是 tail
ps -p 1 -o comm=

# 4) 构建并启动仿真
cd ~/cos_ws/elfin_ws && ./build_all.sh
~/cos_ws/scripts/start_sim.sh
```

通过标准：Gazebo/RViz 窗口正常显示；`top` 里 gzclient/RViz 的 CPU 占用明显低于旧容器（旧值约 46%）。

## 步骤 7：清理旧容器（仅在步骤 6 全部通过后）

```powershell
docker rm -f <旧容器名或ID>
```

旧容器删除后，里面的僵尸进程和 overlay 层残留一并消失。

## 已知注意事项

- **仓库放 `C:\` 上 colcon 构建会变慢**（Docker Desktop 跨文件系统共享的开销）。能接受就不管；嫌慢可把 `C:\cos_ws` 挪到某个 WSL2 发行版的 ext4（如 `\\wsl$\Ubuntu\home\<user>\cos_ws`），在该目录重新 `docker compose up -d` 即可，compose 文件不用改。
- 生产环境（裸机 Ubuntu 20.04）不用 Docker：`git clone` 仓库后跑 `scripts/setup_ubuntu2004.sh`，它与 Dockerfile 共用同一份依赖清单。
- 想进一步省 CPU 可用无头模式（不开 gzclient，RViz 照常）：
  `ros2 launch elfin5_ros2_moveit2 elfin5.launch.py gui:=false`
- 画圆演示：仿真启动后 `ros2 launch cos_shape circle_motion.launch.py`，调参面板 `ros2 run cos_shape circle_panel.py`（详见 `elfin_ws/src/cos_shape/README.md`）。
