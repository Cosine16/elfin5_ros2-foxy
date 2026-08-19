/**
 * 华沿机器人 C++ SDK 控制程序
 * 基于 HuayanRobotLibrary V1.0.15.0
 * 编译: MinGW g++
 * 
 * 功能:
 *   1. 连接机器人控制器
 *   2. 初始化和上电
 *   3. 伺服使能
 *   4. 读取当前位置
 *   5. 执行关节运动和直线运动
 *   6. 安全关闭
 */

#include <iostream>
#include <string>
#include <windows.h>
#include "HR_Pro.h"

// ============ 配置参数 ============
// 默认机器人IP (可通过命令行参数覆盖)
const char*  DEFAULT_ROBOT_IP = "192.168.10.10";
const unsigned short PORT     = 10003;
const unsigned int   BOX_ID   = 0;
const unsigned int   RBT_ID   = 0;

// 默认 TCP 和 UCS 名称
const string DEFAULT_TCP = "TCP";
const string DEFAULT_UCS = "Base";

// 运动参数
const double JOINT_VEL  = 30.0;    // 关节速度 °/s
const double JOINT_ACC  = 15.0;    // 关节加速度 °/s²
const double LINEAR_VEL = 50.0;    // 直线速度 mm/s
const double LINEAR_ACC = 25.0;    // 直线加速度 mm/s²
const double BLEND_R    = 5.0;     // 过渡半径 mm

// ============ 辅助函数 ============

/** 检查返回值，非0则打印错误并返回false */
bool checkRet(const char* funcName, int ret) {
    if (ret != 0) {
        // 获取错误信息
        string errMsg;
        HRIF_GetErrorCodeStr(BOX_ID, ret, errMsg);
        std::cerr << "[ERROR] " << funcName << " failed: ret=" << ret 
                  << " msg=" << errMsg << std::endl;
        return false;
    }
    std::cout << "[OK] " << funcName << std::endl;
    return true;
}

/** 打印当前位姿 */
void printPose(double x, double y, double z, double rx, double ry, double rz) {
    std::cout << "  Pose(XYZ mm, RPY deg): "
              << "X=" << x << " Y=" << y << " Z=" << z << " "
              << "Rx=" << rx << " Ry=" << ry << " Rz=" << rz << std::endl;
}

/** 打印关节角度 */
void printJoints(double j1, double j2, double j3, double j4, double j5, double j6) {
    std::cout << "  Joints(deg): "
              << "J1=" << j1 << " J2=" << j2 << " J3=" << j3 << " "
              << "J4=" << j4 << " J5=" << j5 << " J6=" << j6 << std::endl;
}

/** 等待机器人停止运动 */
bool waitForIdle(int timeoutSec = 30) {
    int movingState = 1;
    int waited = 0;
    while (movingState != 0 && waited < timeoutSec * 10) {
        Sleep(100);
        int enableState, errorState, errorCode, errorAxis;
        int breaking, pause, emergencyStop, safeGuard;
        int electrify, isConnectToBox, blendingDone, inpos;
        
        HRIF_ReadRobotState(BOX_ID, RBT_ID, movingState, enableState, 
                           errorState, errorCode, errorAxis, breaking, pause,
                           emergencyStop, safeGuard, electrify, isConnectToBox,
                           blendingDone, inpos);
        
        if (errorState != 0) {
            string errMsg;
            HRIF_GetErrorCodeStr(BOX_ID, errorCode, errMsg);
            std::cerr << "[ERROR] Robot error: code=" << errorCode 
                      << " msg=" << errMsg << std::endl;
            return false;
        }
        waited++;
    }
    if (movingState != 0) {
        std::cerr << "[WARN] Timeout waiting for robot to stop" << std::endl;
        return false;
    }
    return true;
}


// ============ 主程序 ============

int main(int argc, char* argv[]) {
    // 解析命令行参数
    const char* robotIP = DEFAULT_ROBOT_IP;
    if (argc >= 2) {
        robotIP = argv[1];
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  华沿机械臂 SDK 控制程序 V1.0" << std::endl;
    std::cout << "  SDK Version: 1.0.15.0" << std::endl;
    std::cout << "  Robot IP: " << robotIP << std::endl;
    std::cout << "========================================" << std::endl;
    
    int ret;
    
    // ===== 第1步: 连接 CPS =====
    std::cout << "\n[Step 1] Connecting to CPS at " << robotIP 
              << ":" << PORT << " ..." << std::endl;
    ret = HRIF_Connect(BOX_ID, robotIP, PORT);
    if (!checkRet("HRIF_Connect", ret)) {
        std::cerr << "请检查:" << std::endl;
        std::cerr << "  1. 机器人是否已开机" << std::endl;
        std::cerr << "  2. 网线是否连接正确" << std::endl;
        std::cerr << "  3. 本机IP是否与机器人同一网段" << std::endl;
        std::cerr << "  4. 机器人IP是否为 " << robotIP << std::endl;
        std::cerr << "  用法: " << argv[0] << " [机器人IP]" << std::endl;
        system("pause");
        return 1;
    }
    
    // 读取版本信息
    string strVer;
    int cpsVer, codesysVer, boxVerMajor, boxVerMid, boxVerMin;
    int algoVer, elfinFwVer;
    HRIF_ReadVersion(BOX_ID, RBT_ID, strVer, cpsVer, codesysVer,
                     boxVerMajor, boxVerMid, boxVerMin, algoVer, elfinFwVer);
    std::cout << "  Robot Version: " << strVer << std::endl;
    
    // 读取机器人型号
    string model;
    HRIF_ReadRobotModel(BOX_ID, model);
    std::cout << "  Robot Model: " << model << std::endl;
    
    // ===== 第2步: XToStandby 式状态机初始化 =====
    // 参考 SDK Sample_XToStandby，根据当前 FSM 状态执行对应操作
    std::cout << "\n[Step 2] Initializing robot (state-machine driven)..." << std::endl;
    {
        bool bDone = false;
        int loops = 0;
        while (!bDone && loops < 60) {  // 最多60次循环 ≈ 30秒
            int curFSM = 0; string strFSM;
            HRIF_ReadCurFSM(BOX_ID, RBT_ID, curFSM, strFSM);
            
            switch (curFSM) {
            case 2:   // ElectricBoxDisconnect - 软件无法处理
            case 16:  // ControllerVersionError
            case 25:  // Moving
            case 26:  // LongJogMoving
            case 31:  // FreeDriver
            case 32:  // RobotHolding
            case 34:  // ScriptRunning
            case 36:  // ScriptHolding
                std::cerr << "  [ABORT] Robot in uninterruptible state FSM=" << curFSM << std::endl;
                bDone = true;
                break;
            
            case 5:   // EmergencyStop - 尝试一次 Reset
            case 10:  // SafeGuardError
            case 12:  // SafeGuard
                std::cout << "  [FSM=" << curFSM << "] Trying GrpReset..." << std::endl;
                HRIF_GrpReset(BOX_ID, RBT_ID);
                break;
            
            case 17:  // EtherCATError
            case 21:  // RobotCollisionStop
            case 22:  // Error
                std::cout << "  [FSM=" << curFSM << "] GrpReset..." << std::endl;
                HRIF_GrpReset(BOX_ID, RBT_ID);
                break;
            
            case 7:   // Blackout_48V
                std::cout << "  [FSM=7] Electrify..." << std::endl;
                HRIF_Electrify(BOX_ID);
                break;
            
            case 14:  // ControllerDisconnect
                std::cout << "  [FSM=14] Connect2Controller..." << std::endl;
                HRIF_Connect2Controller(BOX_ID);
                break;
            
            case 24:  // Disable
                std::cout << "  [FSM=24] GrpEnable..." << std::endl;
                HRIF_GrpEnable(BOX_ID, RBT_ID);
                break;
            
            case 33:  // StandBy - 成功!
                std::cout << "  [FSM=33] Robot Standby - READY!" << std::endl;
                bDone = true;
                break;
            
            default:
                break;
            }
            
            loops++;
            if (loops % 10 == 0) {
                std::cout << "  ... loop " << loops << " (FSM=" << curFSM << ")" << std::endl;
            }
            Sleep(500);
        }
        
        if (!bDone) {
            // 超时，检查是否至少到了可操作状态
            int curFSM = 0; string strFSM;
            HRIF_ReadCurFSM(BOX_ID, RBT_ID, curFSM, strFSM);
            if (curFSM == 33 || curFSM == 24) {
                std::cout << "  [OK] Robot in acceptable state FSM=" << curFSM << std::endl;
            } else {
                std::cerr << "[ERROR] Init timeout, FSM=" << curFSM << std::endl;
                HRIF_DisConnect(BOX_ID);
                system("pause");
                return 1;
            }
        }
    }
    
    // ===== 第3步: 伺服使能（如果 FSM=24） =====
    {
        int curFSM = 0; string strFSM;
        HRIF_ReadCurFSM(BOX_ID, RBT_ID, curFSM, strFSM);
        if (curFSM == 24) {
            std::cout << "\n[Step 3] Enabling servo..." << std::endl;
            ret = HRIF_GrpEnable(BOX_ID, RBT_ID);
            if (!checkRet("HRIF_GrpEnable", ret)) {
                HRIF_Blackout(BOX_ID);
                HRIF_DisConnect(BOX_ID);
                system("pause");
                return 1;
            }
            Sleep(1000);
        } else {
            std::cout << "\n[Step 3] Robot already enabled (FSM=" << curFSM << ")" << std::endl;
        }
    }
    
    // ===== 第4步: 设置运动参数 =====
    std::cout << "\n[Step 4] Setting motion parameters..." << std::endl;
    
    HRIF_SetOverride(BOX_ID, RBT_ID, 0.3);  // 30% 速度，安全第一
    std::cout << "  Override: 30%" << std::endl;
    
    HRIF_SetJointMaxVel(BOX_ID, RBT_ID, JOINT_VEL, JOINT_VEL, JOINT_VEL,
                        JOINT_VEL, JOINT_VEL, JOINT_VEL);
    HRIF_SetJointMaxAcc(BOX_ID, RBT_ID, JOINT_ACC, JOINT_ACC, JOINT_ACC,
                        JOINT_ACC, JOINT_ACC, JOINT_ACC);
    HRIF_SetLinearMaxVel(BOX_ID, RBT_ID, LINEAR_VEL);
    HRIF_SetLinearMaxAcc(BOX_ID, RBT_ID, LINEAR_ACC);
    std::cout << "  Joint Vel=" << JOINT_VEL << " deg/s, Acc=" << JOINT_ACC << " deg/s²" << std::endl;
    std::cout << "  Linear Vel=" << LINEAR_VEL << " mm/s, Acc=" << LINEAR_ACC << " mm/s²" << std::endl;
    
    // ===== 第5步: 读取当前位置 =====
    std::cout << "\n[Step 5] Reading current position..." << std::endl;
    double curX, curY, curZ, curRx, curRy, curRz;
    double curJ1, curJ2, curJ3, curJ4, curJ5, curJ6;
    double tcpX, tcpY, tcpZ, tcpRx, tcpRy, tcpRz;
    double ucsX, ucsY, ucsZ, ucsRx, ucsRy, ucsRz;
    
    ret = HRIF_ReadActPos(BOX_ID, RBT_ID,
                          curX, curY, curZ, curRx, curRy, curRz,
                          curJ1, curJ2, curJ3, curJ4, curJ5, curJ6,
                          tcpX, tcpY, tcpZ, tcpRx, tcpRy, tcpRz,
                          ucsX, ucsY, ucsZ, ucsRx, ucsRy, ucsRz);
    checkRet("HRIF_ReadActPos", ret);
    std::cout << "  Current pose:" << std::endl;
    printPose(curX, curY, curZ, curRx, curRy, curRz);
    std::cout << "  Current joints:" << std::endl;
    printJoints(curJ1, curJ2, curJ3, curJ4, curJ5, curJ6);
    
    // ===== 第6步: 关节运动 MoveJ (使用关节角度，参考SDK示例) =====
    std::cout << "\n[Step 6] Joint Move (MoveJ) - J1 move 5 degrees..." << std::endl;
    {
        // 关节运动目标：J1 偏移 +5°，其余关节保持
        double targetJ1 = curJ1 + 5.0;
        double targetJ2 = curJ2;
        double targetJ3 = curJ3;
        double targetJ4 = curJ4;
        double targetJ5 = curJ5;
        double targetJ6 = curJ6;
        
        // 参考SDK Sample_Move: nIsUseJoint=1 直接使用关节角度
        ret = HRIF_MoveJ(BOX_ID, RBT_ID,
                         0, 0, 0, 0, 0, 0,             // pose (关节模式不使用)
                         targetJ1, targetJ2, targetJ3,
                         targetJ4, targetJ5, targetJ6,
                         DEFAULT_TCP, DEFAULT_UCS,
                         JOINT_VEL, JOINT_ACC, BLEND_R,
                         true,                           // nIsUseJoint=1
                         false, 0, 0,                   // no seek
                         "CMD1");
        if (checkRet("HRIF_MoveJ", ret)) {
            std::cout << "  Target J1=" << targetJ1 << std::endl;
            // 使用 IsBlendingDone 等待运动完成
            bool bDone = false;
            int w = 0;
            while (!bDone && w < 300) {
                Sleep(100);
                HRIF_IsBlendingDone(BOX_ID, RBT_ID, bDone);
                w++;
            }
            std::cout << "  MoveJ completed." << std::endl;
        }
    }
    
    // ===== 第7步: 直线运动 MoveL (使用 SDK 示例的参考关节角) =====
    std::cout << "\n[Step 7] Linear Move (MoveL) - Z axis +30mm..." << std::endl;
    {
        // 读取运动后位姿
        double actX, actY, actZ, actRx, actRy, actRz;
        double actJ1, actJ2, actJ3, actJ4, actJ5, actJ6;
        double tX, tY, tZ, tRX, tRY, tRZ;
        double uX, uY, uZ, uRX, uRY, uRZ;
        HRIF_ReadActPos(BOX_ID, RBT_ID,
                        actX, actY, actZ, actRx, actRy, actRz,
                        actJ1, actJ2, actJ3, actJ4, actJ5, actJ6,
                        tX, tY, tZ, tRX, tRY, tRZ,
                        uX, uY, uZ, uRX, uRY, uRZ);
        
        double targetZ = actZ + 30.0;
        
        // 关键：参考关节角使用非奇异构型 (0,0,90,0,90,0)
        // 参考 SDK Sample_Move 中的 MoveL 函数
        ret = HRIF_MoveL(BOX_ID, RBT_ID,
                         actX, actY, targetZ, actRx, actRy, actRz,
                         0, 0, 90, 0, 90, 0,          // 参考关节角（非奇异构型）
                         DEFAULT_TCP, DEFAULT_UCS,
                         LINEAR_VEL, LINEAR_ACC, BLEND_R,
                         false, 0, 0,
                         "CMD2");
        if (checkRet("HRIF_MoveL", ret)) {
            std::cout << "  Target Z=" << targetZ << std::endl;
            bool bDone = false;
            int w = 0;
            while (!bDone && w < 300) {
                Sleep(100);
                HRIF_IsBlendingDone(BOX_ID, RBT_ID, bDone);
                w++;
            }
            std::cout << "  MoveL completed." << std::endl;
        }
    }
    
    // ===== 第8步: 读取最终位置 =====
    std::cout << "\n[Step 8] Reading final position..." << std::endl;
    double finalX, finalY, finalZ, finalRx, finalRy, finalRz;
    double finalJ1, finalJ2, finalJ3, finalJ4, finalJ5, finalJ6;
    ret = HRIF_ReadActPos(BOX_ID, RBT_ID,
                          finalX, finalY, finalZ, finalRx, finalRy, finalRz,
                          finalJ1, finalJ2, finalJ3, finalJ4, finalJ5, finalJ6,
                          tcpX, tcpY, tcpZ, tcpRx, tcpRy, tcpRz,
                          ucsX, ucsY, ucsZ, ucsRx, ucsRy, ucsRz);
    if (checkRet("HRIF_ReadActPos", ret)) {
        std::cout << "  Final pose:" << std::endl;
        printPose(finalX, finalY, finalZ, finalRx, finalRy, finalRz);
        std::cout << "  Final joints:" << std::endl;
        printJoints(finalJ1, finalJ2, finalJ3, finalJ4, finalJ5, finalJ6);
    }
    
    // ===== 第9步: 安全关闭 =====
    std::cout << "\n[Step 9] Shutting down safely..." << std::endl;
    
    std::cout << "  Disabling servo..." << std::endl;
    HRIF_GrpDisable(BOX_ID, RBT_ID);
    Sleep(500);
    
    std::cout << "  Powering off..." << std::endl;
    HRIF_Blackout(BOX_ID);
    Sleep(500);
    
    std::cout << "  Disconnecting..." << std::endl;
    HRIF_DisConnect(BOX_ID);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Program completed successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    system("pause");
    return 0;
}
