# cos_realsense — D435i 点云 Demo 包

ROS2 Foxy + Intel RealSense D435i 示例：同一份点云 API，两种数据源。

## 来源说明

- 本包所有代码（节点、launch、URDF、world、RViz 配置）均为自研，
  **不包含 Intel 官方源码**。
- Intel 官方组件以系统 deb 安装（`ros-foxy-realsense2-camera` 4.51.1 驱动、
  `ros-foxy-realsense2-description`、`ros-foxy-librealsense2` 2.51.1 SDK），
  真机链路通过 launch include 调用官方驱动，未复制其代码。
- `urdf/d435i.urdf.xacro` 是自写的简化模型（供 Gazebo 仿真用），
  与官方 `realsense2_description` 无关。

| 链路 | 数据源 | 启动命令 |
|---|---|---|
| 真机 | D435i 硬件 (`realsense2_camera` 驱动) | `ros2 launch cos_realsense real_pointcloud.launch.py` |
| 仿真 | Gazebo 中的虚拟 D435i (`gazebo_ros` 深度相机插件) | `ros2 launch cos_realsense sim_pointcloud.launch.py` |

两条链路发布**同名话题**，demo 节点和 RViz 配置完全复用。

## 构建

本包在自研应用层工作空间 `app_ws` 内（overlay，依赖 elfin_ws）：

```bash
~/cos_ws/app_ws/build_all.sh     # 自动 source elfin_ws 后构建
source ~/cos_ws/app_ws/install/setup.bash
```

## 运行

```bash
# 仿真(无需相机):Gazebo 里能看到场景,RViz 里能看到点云
ros2 launch cos_realsense sim_pointcloud.launch.py

# 真机(插上 D435i):
ros2 launch cos_realsense real_pointcloud.launch.py
```

RViz 中应看到：RGB 着色的点云、TF 坐标系、以及一个绿色球体（点云质心 Marker，由 demo 节点发布）。

## 关键话题

| 话题 | 类型 | 说明 |
|---|---|---|
| `/camera/depth/color/points` | `sensor_msgs/PointCloud2` | 深度对齐到彩色后的点云 |
| `/camera/color/image_raw` | `sensor_msgs/Image` | 彩色图像 |
| `/camera/depth/image_rect_raw` | `sensor_msgs/Image` | 深度图 (16UC1, 毫米) |
| `/pointcloud_stats/centroid_marker` | `visualization_msgs/Marker` | demo 节点发布的质心 |

注意：两个数据源都以 **sensor data QoS (Best Effort)** 发布，订阅端必须用匹配的 QoS（demo 节点用 `qos_profile_sensor_data`，RViz 配置里已设为 Best Effort），否则会收不到数据。

## 目录结构

```
app_ws/
├── generated/                     # 运行产物:仿真采集的高度图 + world
├── generated_real/                # 运行产物:真机采集的高度图 + world
└── src/cos_realsense/
    ├── package.xml / setup.py / setup.cfg     # ament_python 包定义
    ├── cos_realsense/
    │   ├── pointcloud_stats_node.py           # demo 节点:订阅点云→统计→发 Marker
    │   ├── depth_projector_node.py            # 深度图→点云投影(绕过 gazebo 插件 bug)
    │   └── heightmap_builder_node.py          # 点云→高度图→Gazebo terrain
    ├── launch/
    │   ├── real_pointcloud.launch.py          # 真机链路
    │   ├── sim_pointcloud.launch.py           # 仿真链路
    │   ├── capture_heightmap.launch.py        # 采集高度图(真机/仿真)
    │   └── view_heightmap.launch.py           # 在 Gazebo 中查看采集结果
    ├── urdf/d435i.urdf.xacro                  # D435i 模型 + gazebo 深度相机插件
    ├── worlds/demo.world                      # 仿真场景(地面 + 3 个彩色障碍物)
    └── rviz/pointcloud.rviz                   # 两条链路共用的 RViz 配置
```

## real-to-sim:点云 → Gazebo 高度图

把相机看到的场景烘焙成 Gazebo 地形,供端到端模型在仿真里作业:

```bash
# 1. 采集(真机,相机放平稳视前方;num_frames 越多越完整)
ros2 launch cos_realsense capture_heightmap.launch.py

# 也可以用仿真相机自测整条管线
ros2 launch cos_realsense capture_heightmap.launch.py camera_source:=sim z_max:=0.6

# 2. 查看生成物(PNG + world)
ls ~/cos_ws/app_ws/generated/

# 3. 在 Gazebo 里加载采集到的地形
ros2 launch cos_realsense view_heightmap.launch.py \
    world:=/home/fit/cos_ws/app_ws/generated/heightmap.world
```

管线(`heightmap_builder_node.py`):点云 →tf2 变换到 `camera_link`(z 朝上)
→ ROI 过滤(`x/y/z_min/max` 参数)→ 按 `resolution` 栅格化,每格取最大 z
(地表顶面)→ 空格邻域填充 → 16 位灰度 PNG + 引用它的 `heightmap.world`。

关键参数(都可从 launch 命令行覆盖):`num_frames` `resolution`
`x_min/x_max y_min/y_max z_min/z_max`(米,相对 camera_link)`output_dir`。

注意:相机要**水平放置**(camera_link 的 z 轴即"上"),否则地形是斜的。

## 已绕过的两个 gazebo_ros_pkgs 3.5.3 (foxy) bug

1. **传感器朝向**:Gazebo 渲染相机沿所在 link 的 **+X 轴**看、+Z 朝上。
   深度传感器必须 `<gazebo reference="camera_link">`,不能挂 optical frame
   (x 朝右),否则渲染出的深度图是"一行重复 480 次"的废图。
2. **插件点云塌缩**:`libgazebo_ros_camera.so` 发布的 `rgbd/points` 所有点
   x 被冻结在第一列光线角(x ≈ -0.9497×深度),云是坏的。深度**图像**本身
   是对的,所以 `depth_projector_node` 用 camera_info 内参自行重投影,
   发布正确的 `/camera/depth/color/points`。真机驱动无此问题。

## demo 节点 API 要点

1. **订阅** `PointCloud2`：真实驱动与 gazebo 插件发布的都是 organized cloud
   （`width*height` 个按图像排列的点，无效点为 NaN）。
2. **解析**：按 `msg.fields` 的 `name/offset/datatype` 构造 numpy dtype，
   `itemsize=msg.point_step`，一次 `np.frombuffer` 解出全部点——这就是
   `sensor_msgs_py.point_cloud2.read_points` 内部做的事（若装了
   `ros-foxy-sensor-msgs-py` 可直接用它）。
3. **处理**：过滤 NaN → 计算有效点数 / 质心 / 最近点距离，每 30 帧打印一次。
4. **发布**：把质心以 `Marker.SPHERE` 发回 RViz，注意 header 沿用点云的
   `frame_id`，TF 才能正确变换。

## 常见问题

- **gzclient 打开 heightmap.world 时段错误 (exit 139)**:Gazebo 要求高度图
  PNG 为 **2^n+1 的方形**(如 513×513),旧版生成物(320×320)会崩。
  `heightmap_builder_node` 已自动重采样到合规尺寸,重新采集一次即可。

- **RViz 点云不显示**：检查 Displays → PointCloud2 → Topic 的 Reliability
  是否为 Best Effort。
- **想加 IMU**：编辑 `launch/real_pointcloud.launch.py`，取消
  `enable_gyro/enable_accel/unite_imu_method` 三行注释。
- **固件报错 `HW not ready`**：固件 05.15.01.55 与 SDK 2.51.1 的已知
  兼容警告，不影响点云；介意可用 realsense-viewer 降固件到 05.13.00.50。
