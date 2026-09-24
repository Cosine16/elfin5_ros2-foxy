# 华沿机械臂 SDK 控制程序

基于 **HuayanRobotLibrary C++ SDK V1.0.15.0** 的机械臂控制程序。

## 环境配置

| 项目 | 配置 |
|------|------|
| 操作系统 | Windows |
| 编译器 | MinGW g++ 16.1.0 (x86_64) |
| SDK 版本 | HuayanRobotLibrary-C++-V1.0.15.0 |
| 通信协议版本 | HuayanRobotV6 V1.0.19.1 |
| 本机 IP | `192.168.10.197` / `255.255.255.0`（以太网直连） |
| 机器人 IP | `192.168.10.10` (enp4s0) |
| 机器人型号 | **E05-1195_Pro**（Elfin Pro 系列，6轴，5kg） |
| 机器人端口 | `10003`（IF命令服务端口） |

## 机器人网络接口

| 接口 | IP | 说明 |
|------|-----|------|
| enp1s0 (huayanPAD) | 192.168.156.2 | ❌ 灰色不可用 |
| enp3s0 | 192.168.0.10 | 可用 |
| enp4s0 | **192.168.10.10** | ✅ 蓝色可用（推荐） |

## 目录结构

```
huayan-boot-8664sdk/
├── README.md                          # 本文件
├── refs/
│   └── HuayanRobotLibrary-C++-V1.0.15.0/
│       ├── include/HR_Pro.h           # SDK 头文件 (API 定义)
│       ├── MinGW/                     # MinGW 库文件
│       │   ├── libHR_Pro.dll          # Release DLL
│       │   └── libHR_Pro.dll.a        # 导入库
│       ├── MSVC/                      # MSVC 库文件
│       └── Linux/                     # Linux 库文件
└── src/
    ├── main.cpp                       # 主控制程序
    ├── build.bat                      # 编译脚本
    ├── scan_robot.bat                 # 网络扫描工具
    ├── huayan_robot_ctrl.exe          # 编译产物
    └── libHR_Pro.dll                  # 运行时 DLL (从SDK复制)
```

## 快速开始

### 1. 网络配置

将以太网适配器 IP 设为与机器人同一网段：

```powershell
# 连接到 192.168.10.10 (enp4s0, 推荐)
netsh interface ip set address "以太网" static 192.168.10.197 255.255.255.0

# 或者连接到 192.168.0.10 (enp3s0)
netsh interface ip set address "以太网" static 192.168.0.197 255.255.255.0
```

### 2. 查找机器人 IP

```batch
cd src
scan_robot.bat
```

### 3. 编译

```batch
cd src
build.bat
```

### 4. 运行

```batch
huayan_robot_ctrl.exe [机器人IP]
```

示例：
```batch
huayan_robot_ctrl.exe 192.168.10.10
huayan_robot_ctrl.exe 192.168.0.10
```

## 机器人 FSM 状态机

通过实测和协议文档分析，华沿机器人 V6 控制器的 FSM 状态如下：

| FSM | 状态名称 | 说明 |
|-----|---------|------|
| 5 | EmergencyStopped | 急停状态，需人工解除急停按钮 |
| 14 | ControllerDisconnected | 控制器未连接 |
| 15 | ConnectingToController | 正在连接控制器（异步过程） |
| 24 | RobotDisable | 机器人已通电但伺服关闭 |

## 初始化流程（协议层）

根据 `HuayanRobotV6控制通信协议接口_V1.0.19.1.pdf`：

```
1. ConnectToBox    → 连接控制箱
2. StartMaster     → 启动主站，连接控制器（状态：14→15→24）
3. Electrify       → 机器人上电（需在状态24执行）
4. GrpEnable       → 伺服使能
5. WayPoint/MoveJ/MoveL → 运动控制
6. GrpDisable      → 关闭伺服
7. BlackOut        → 断电
8. CloseMaster     → 断开控制器
```

## 程序工作流程 (C++ SDK)

```
1. HRIF_Connect()             → 连接 CPS (端口 10003) ✅ 已验证
2. HRIF_Connect2Controller()  → 初始化控制器（如未初始化）
3. 等待状态稳定               → 轮询 ReadRobotState/ReadCurFSM
4. HRIF_Electrify()           → 上电（如未上电）
5. HRIF_GrpEnable()           → 伺服使能
6. HRIF_SetOverride(0.5)      → 设置速度倍率 50%
7. HRIF_ReadActPos()          → 读取当前位姿和关节角度
8. HRIF_WayPoint(MoveJ)       → 关节运动
9. HRIF_WayPoint(MoveL)       → 直线运动
10. HRIF_GrpDisable()         → 断开伺服
11. HRIF_Blackout()           → 断电
12. HRIF_DisConnect()         → 断开CPS连接
```

## 实测连接结果

✅ **HRIF_Connect** → 成功连接 CPS，获取：
- 机器人版本: `20260206.34335.6.8229.4115.103.341118212`
- 机器人型号: `E05-1195_Pro`

✅ **HRIF_Connect2Controller** → 控制器初始化成功（首次需等待异步完成）

⚠️ **HRIF_Electrify** → 需在正确的 FSM 状态下调用：
- FSM=24 (RobotDisable): ✅ 可上电
- FSM=5 (EmergencyStopped): ❌ 需先解除急停
- FSM=15 (ConnectingToController): ❌ 需等待初始化完成

## 服务端口表

| 端口 | 说明 |
|------|------|
| 10001 | IF 快速命令服务端口 |
| **10003** | **IF 命令服务端口 (SDK使用)** |
| 10004 | DataSheet JSON 推送 (50ms周期) |
| 10005 | DataSheet JSON 推送 (100ms周期) |
| 10006 | DataSheet JSON 推送 (200ms周期) |
| 10014 | DataSheet Struct 推送 (50ms周期) |
| 10015 | DataSheet Struct 推送 (100ms周期) |
| 10016 | DataSheet Struct 推送 (200ms周期) |
| 10502 | ModbusTCP 服务端 |
| 1883 | MQTT 协议服务器端口 |

## SDK 关键 API 参考

### 连接与初始化
| API | 说明 |
|-----|------|
| `HRIF_Connect(boxID, ip, port)` | 连接 CPS，默认端口 10003 |
| `HRIF_Connect2Box(boxID)` | 连接控制箱 |
| `HRIF_Connect2Controller(boxID)` | 完整初始化流程 |
| `HRIF_DisConnect(boxID)` | 断开连接 |
| `HRIF_IsConnected(boxID)` | 检查连接状态 |

### 电源与伺服
| API | 说明 |
|-----|------|
| `HRIF_Electrify(boxID)` | 上电 |
| `HRIF_Blackout(boxID)` | 断电 |
| `HRIF_GrpEnable(boxID, rbtID)` | 伺服使能 |
| `HRIF_GrpDisable(boxID, rbtID)` | 断开伺服 |
| `HRIF_GrpReset(boxID, rbtID)` | 复位 |
| `HRIF_GrpStop(boxID, rbtID)` | 停止运动 |

### 运动控制
| API | 说明 |
|-----|------|
| `HRIF_WayPoint(boxID, rbtID, type, ...)` | 通用路径点运动 (推荐) |
| `HRIF_WayPoint2(boxID, rbtID, ...)` | 通用路径点运动 (含辅助点) |
| `HRIF_MoveJ(boxID, rbtID, ...)` | 关节运动 |
| `HRIF_MoveL(boxID, rbtID, ...)` | 直线运动 |
| `HRIF_MoveC(boxID, rbtID, ...)` | 圆弧运动 |

### 状态读取
| API | 说明 |
|-----|------|
| `HRIF_ReadActPos(boxID, rbtID, ...)` | 读取实际位姿和关节角度 |
| `HRIF_ReadRobotState(boxID, rbtID, ...)` | 读取机器人状态 |
| `HRIF_ReadVersion(boxID, rbtID, ...)` | 读取版本信息 |
| `HRIF_ReadRobotModel(boxID, model)` | 读取机器人型号 |

### 参数设置
| API | 说明 |
|-----|------|
| `HRIF_SetOverride(boxID, rbtID, val)` | 设置速度倍率 (0.01~1.00) |
| `HRIF_SetJointMaxVel(boxID, rbtID, ...)` | 设置关节最大速度 |
| `HRIF_SetJointMaxAcc(boxID, rbtID, ...)` | 设置关节最大加速度 |
| `HRIF_SetLinearMaxVel(boxID, rbtID, val)` | 设置直线最大速度 |
| `HRIF_SetLinearMaxAcc(boxID, rbtID, val)` | 设置直线最大加速度 |

## 安全注意事项

1. **紧急停止**: 机器人运行时，可使用示教器上的急停按钮
2. **速度控制**: 默认使用 50% 速度倍率，调试时建议先用低速
3. **工作空间**: 确保机器人在安全工作空间内运行
4. **运动范围**: 程序仅做小幅度运动测试 (J1 +10°, Z +50mm)

## 编译命令 (手动)

```bash
g++ -Wall -std=c++17 -O2 \
    -I"../refs/HuayanRobotLibrary-C++-V1.0.15.0/include" \
    -L"../refs/HuayanRobotLibrary-C++-V1.0.15.0/MinGW" \
    main.cpp -o huayan_robot_ctrl.exe \
    -lHR_Pro -static-libgcc -static-libstdc++
```

需要将 `libHR_Pro.dll` 放在 exe 同目录或系统 PATH 中。
