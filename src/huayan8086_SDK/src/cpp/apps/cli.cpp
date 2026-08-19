/**
 * Huayan Robot Interactive CLI Control Tool
 * Based on HuayanRobotLibrary V1.0.15.0
 * Compiler: MinGW g++
 *
 * Commands:
 *   j J1 J2 J3 J4 J5 J6  - Joint move
 *   l X Y Z Rx Ry Rz     - Linear move
 *   rj / rp / rs          - Read joints/pose/state
 *   stop / reset          - Stop / Reset
 *   speed 0.1~1.0        - Set speed override
 *   home                  - Go to home (0,0,90,0,90,0)
 *   quit                  - Safe shutdown and exit
 *   help                  - Show this help
 */

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <windows.h>
#include "HR_Pro.h"

using namespace std;

// ============ 全局配置 ============
const unsigned int   BOX_ID = 0;
const unsigned int   RBT_ID = 0;
const double         DEFAULT_VEL  = 30.0;
const double         DEFAULT_ACC  = 50.0;
const double         BLEND_R      = 5.0;
const string         TCP_NAME = "TCP";
const string         UCS_NAME = "Base";

double g_override = 0.3;  // speed override

// ============ Helpers ============

void printHelp() {
    cout << "\n========================================" << endl;
    cout << "  Huayan Robot Interactive Console" << endl;
    cout << "  Model: E05-1195_Pro" << endl;
    cout << "========================================" << endl;
    cout << "\nCommands:" << endl;
    cout << "  j  J1 J2 J3 J4 J5 J6  - Joint move (deg)" << endl;
    cout << "  l  X Y Z Rx Ry Rz     - Linear move (mm, deg)" << endl;
    cout << "  rj                    - Read current joint angles" << endl;
    cout << "  rp                    - Read current TCP pose" << endl;
    cout << "  rs                    - Read robot state" << endl;
    cout << "  stop                  - Stop motion" << endl;
    cout << "  reset                 - Reset errors" << endl;
    cout << "  recover               - Auto-recover to Standby (FSM 33)" << endl;
    cout << "  speed <0.1~1.0>      - Set speed override" << endl;
    cout << "  home                  - Go home (0,0,90,0,90,0)" << endl;
    cout << "  quit                  - Safe shutdown & exit" << endl;
    cout << "  help                  - Show this help" << endl;
    cout << "\nExamples:" << endl;
    cout << "  j 0 0 90 0 90 0      - Move to home joints" << endl;
    cout << "  l 400 0 400 180 0 180 - Linear move to pose" << endl;
    cout << "  speed 0.5             - Set 50% speed" << endl;
    cout << "========================================" << endl;
}

// Parse command line
vector<string> parseLine(const string& line) {
    vector<string> args;
    istringstream iss(line);
    string token;
    while (iss >> token) args.push_back(token);
    return args;
}

// Check return value
bool checkOK(const char* name, int ret) {
    if (ret != 0) {
        string err;
        HRIF_GetErrorCodeStr(BOX_ID, ret, err);
        cerr << "  [FAIL] " << name << ": " << ret << " - " << err << endl;
        return false;
    }
    return true;
}

// Wait for motion to complete
bool waitBlendingDone(int timeoutSec = 30) {
    bool done = false;
    int w = 0;
    while (!done && w < timeoutSec * 10) {
        Sleep(100);
        HRIF_IsBlendingDone(BOX_ID, RBT_ID, done);
        w++;
    }
    return done;
}

// ============ Command handlers ============

void cmdReadJoints() {
    double j[6], x[6], tcp[6], ucs[6];
    int ret = HRIF_ReadActPos(BOX_ID, RBT_ID,
        x[0], x[1], x[2], x[3], x[4], x[5],
        j[0], j[1], j[2], j[3], j[4], j[5],
        tcp[0], tcp[1], tcp[2], tcp[3], tcp[4], tcp[5],
        ucs[0], ucs[1], ucs[2], ucs[3], ucs[4], ucs[5]);
    if (checkOK("ReadActPos", ret)) {
        cout << fixed << setprecision(3);
        cout << "  Joints(deg): J1=" << j[0] << " J2=" << j[1] << " J3=" << j[2]
             << " J4=" << j[3] << " J5=" << j[4] << " J6=" << j[5] << endl;
    }
}

void cmdReadPose() {
    double j[6], x[6], tcp[6], ucs[6];
    int ret = HRIF_ReadActPos(BOX_ID, RBT_ID,
        x[0], x[1], x[2], x[3], x[4], x[5],
        j[0], j[1], j[2], j[3], j[4], j[5],
        tcp[0], tcp[1], tcp[2], tcp[3], tcp[4], tcp[5],
        ucs[0], ucs[1], ucs[2], ucs[3], ucs[4], ucs[5]);
    if (checkOK("ReadActPos", ret)) {
        cout << fixed << setprecision(3);
        cout << "  TCP(mm,deg): X=" << x[0] << " Y=" << x[1] << " Z=" << x[2]
             << " Rx=" << x[3] << " Ry=" << x[4] << " Rz=" << x[5] << endl;
    }
}

void cmdReadState() {
    int curFSM = 0; string strFSM;
    HRIF_ReadCurFSM(BOX_ID, RBT_ID, curFSM, strFSM);

    int m, en, er, ec, ea, br, pa, es, sg, el, cb, bd, ip;
    HRIF_ReadRobotState(BOX_ID, RBT_ID, m, en, er, ec, ea, br, pa, es, sg, el, cb, bd, ip);

    cout << "  FSM=" << curFSM << " (" << strFSM << ")" << endl;
    cout << "  enable=" << en << " electrify=" << el
         << " moving=" << m << " error=" << er << endl;

    double ov;
    HRIF_ReadOverride(BOX_ID, RBT_ID, ov);
    cout << "  override=" << (ov * 100) << "%" << endl;
}

void cmdMoveJ(const vector<string>& args) {
    if (args.size() < 7) { cerr << "  Usage: j J1 J2 J3 J4 J5 J6" << endl; return; }
    double j[6];
    for (int i = 0; i < 6; i++) j[i] = stod(args[i + 1]);

    cout << "  MoveJ → J1=" << j[0] << " J2=" << j[1] << " J3=" << j[2]
         << " J4=" << j[3] << " J5=" << j[4] << " J6=" << j[5] << endl;

    int ret = HRIF_MoveJ(BOX_ID, RBT_ID,
        0, 0, 0, 0, 0, 0,     // pose (unused when IsUseJoint=1)
        j[0], j[1], j[2], j[3], j[4], j[5],
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        true, false, 0, 0, "CMD");
    if (checkOK("MoveJ", ret)) {
        if (waitBlendingDone()) cout << "  [OK] Done." << endl;
        else cout << "  [WARN] Timeout." << endl;
    }
}

void cmdMoveL(const vector<string>& args) {
    if (args.size() < 7) { cerr << "  Usage: l X Y Z Rx Ry Rz" << endl; return; }
    double x[6];
    for (int i = 0; i < 6; i++) x[i] = stod(args[i + 1]);

    cout << "  MoveL → X=" << x[0] << " Y=" << x[1] << " Z=" << x[2]
         << " Rx=" << x[3] << " Ry=" << x[4] << " Rz=" << x[5] << endl;

    // 使用非奇异参考关节角 (参考SDK示例)
    int ret = HRIF_MoveL(BOX_ID, RBT_ID,
        x[0], x[1], x[2], x[3], x[4], x[5],
        0, 0, 90, 0, 90, 0,           // ref joints (non-singular)
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        false, 0, 0, "CMD");
    if (checkOK("MoveL", ret)) {
        if (waitBlendingDone()) cout << "  [OK] Done." << endl;
        else cout << "  [WARN] Timeout." << endl;
    }
}

void cmdHome() {
    cout << "  Moving to home position (0,0,90,0,90,0)..." << endl;
    int ret = HRIF_MoveJ(BOX_ID, RBT_ID,
        0, 0, 0, 0, 0, 0,
        0, 0, 90, 0, 90, 0,
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        true, false, 0, 0, "HOME");
    if (checkOK("MoveJ(home)", ret)) {
        if (waitBlendingDone()) cout << "  [OK] Home reached." << endl;
        else cout << "  [WARN] Timeout." << endl;
    }
}

void cmdStop() {
    cout << "  Stopping..." << endl;
    HRIF_GrpStop(BOX_ID, RBT_ID);
    cout << "  [OK] Stop sent." << endl;
}

void cmdReset() {
    cout << "  Resetting errors..." << endl;
    int ret = HRIF_GrpReset(BOX_ID, RBT_ID);
    if (checkOK("GrpReset", ret)) cout << "  [OK] Reset done." << endl;
}

void cmdSpeed(const vector<string>& args) {
    if (args.size() < 2) { cerr << "  Usage: speed <0.1~1.0>" << endl; return; }
    g_override = stod(args[1]);
    if (g_override < 0.01) g_override = 0.01;
    if (g_override > 1.0)  g_override = 1.0;
    HRIF_SetOverride(BOX_ID, RBT_ID, g_override);
    cout << "  [OK] Override set to " << (g_override * 100) << "%" << endl;
}

// Run state machine to bring robot to Standby (FSM=33)
bool recoverToStandby(int timeoutSec = 30) {
    bool ready = false;
    int loops = 0;
    int maxLoops = timeoutSec * 2;  // 500ms per loop
    int prevFSM = -1;

    while (!ready && loops < maxLoops) {
        int fsm = 0; string sfsm;
        HRIF_ReadCurFSM(BOX_ID, RBT_ID, fsm, sfsm);

        if (fsm != prevFSM) {
            cout << "  FSM=" << fsm << " (" << sfsm << ")" << endl;
            prevFSM = fsm;
        }

        switch (fsm) {
        case 5: case 10: case 12: case 17: case 21: case 22:
            HRIF_GrpReset(BOX_ID, RBT_ID);   break;
        case 7:
            HRIF_Electrify(BOX_ID);           break;
        case 14:
            HRIF_Connect2Controller(BOX_ID);   break;
        case 24:
            HRIF_GrpEnable(BOX_ID, RBT_ID);    break;
        // 23 = RobotEnabling (transient, just wait)
        case 33:
            ready = true;                      break;
        }
        loops++;
        Sleep(500);
    }

    if (ready) {
        HRIF_SetOverride(BOX_ID, RBT_ID, g_override);
        cout << "  [OK] Robot ready (Standby)." << endl;
        return true;
    }
    return false;
}

void cmdRecover() {
    cout << "  Recovering to Standby..." << endl;
    if (!recoverToStandby()) {
        cerr << "  [FAIL] Recovery timeout. Check pendant." << endl;
    }
}


// ============ Main ============

int main(int argc, char* argv[]) {
    const char* ip = (argc >= 2) ? argv[1] : "192.168.10.10";

    cout << "========================================" << endl;
    cout << "  Huayan Robot Interactive Console" << endl;
    cout << "  SDK V1.0.15.0  |  E05-1195_Pro" << endl;
    cout << "  Connecting: " << ip << ":10003" << endl;
    cout << "========================================" << endl;

    // ----- Connect & init (state-machine driven) -----
    cout << "\n[INIT] Connecting..." << endl;
    int ret = HRIF_Connect(BOX_ID, ip, 10003);
    if (!checkOK("HRIF_Connect", ret)) {
        cerr << "  Cannot connect to robot. Check network and IP." << endl;
        system("pause"); return 1;
    }

    string model, ver;
    HRIF_ReadRobotModel(BOX_ID, model);
    cout << "  Model: " << model << endl;

    // State-machine init
    cout << "[INIT] State-machine initialization..." << endl;
    if (!recoverToStandby()) {
        cerr << "[FAIL] Init timeout. Check robot state on pendant." << endl;
        HRIF_DisConnect(BOX_ID);
        system("pause"); return 1;
    }

    // Set default params (already done by recoverToStandby, keep for safety)
    HRIF_SetOverride(BOX_ID, RBT_ID, g_override);
    HRIF_SetJointMaxVel(BOX_ID, RBT_ID, DEFAULT_VEL, DEFAULT_VEL, DEFAULT_VEL,
                        DEFAULT_VEL, DEFAULT_VEL, DEFAULT_VEL);
    HRIF_SetJointMaxAcc(BOX_ID, RBT_ID, DEFAULT_ACC, DEFAULT_ACC, DEFAULT_ACC,
                        DEFAULT_ACC, DEFAULT_ACC, DEFAULT_ACC);
    HRIF_SetLinearMaxVel(BOX_ID, RBT_ID, DEFAULT_VEL);
    HRIF_SetLinearMaxAcc(BOX_ID, RBT_ID, DEFAULT_ACC);

    printHelp();

    // ----- 主命令循环 -----
    string line;
    cout << "\n> " << flush;
    while (getline(cin, line)) {
        if (line.empty()) { cout << "> " << flush; continue; }

        auto args = parseLine(line);
        if (args.empty()) { cout << "> " << flush; continue; }

        string cmd = args[0];

        if (cmd == "j" || cmd == "movej") {
            cmdMoveJ(args);
        } else if (cmd == "l" || cmd == "movel") {
            cmdMoveL(args);
        } else if (cmd == "rj" || cmd == "joints") {
            cmdReadJoints();
        } else if (cmd == "rp" || cmd == "pose") {
            cmdReadPose();
        } else if (cmd == "rs" || cmd == "state") {
            cmdReadState();
        } else if (cmd == "stop") {
            cmdStop();
        } else if (cmd == "reset") {
            cmdReset();
        } else if (cmd == "recover") {
            cmdRecover();
        } else if (cmd == "speed") {
            cmdSpeed(args);
        } else if (cmd == "home") {
            cmdHome();
        } else if (cmd == "help" || cmd == "?") {
            printHelp();
        } else if (cmd == "quit" || cmd == "exit" || cmd == "q") {
            break;
        } else {
            cerr << "  Unknown command: " << cmd << " (type help for list)" << endl;
        }

        cout << "> " << flush;
    }

    // ----- 安全关闭 -----
    cout << "\n[EXIT] Shutting down..." << endl;
    HRIF_GrpDisable(BOX_ID, RBT_ID);
    Sleep(500);
    HRIF_Blackout(BOX_ID);
    Sleep(500);
    HRIF_DisConnect(BOX_ID);
    cout << "[OK] Robot disconnected. Bye." << endl;

    return 0;
}
