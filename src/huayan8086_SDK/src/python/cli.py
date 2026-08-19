"""
Huayan Robot Interactive CLI Control Tool (Python)
Based on HuayanRobot Python SDK V1.1.1.1

Usage:
  python cli.py [--ip 192.168.10.10] [--dry-run]

Commands:
  j J1 J2 J3 J4 J5 J6  - Joint move
  l X Y Z Rx Ry Rz     - Linear move
  rj / rp / rs          - Read joints/pose/state
  stop / reset          - Stop / Reset errors
  recover               - Auto-recover to Standby (FSM 33)
  speed 0.1~1.0        - Set speed override
  home                  - Go to home (0,0,90,0,90,0)
  quit                  - Safe shutdown and exit
  help                  - Show this help
"""

import sys
import os
import time
import shlex

# ----- SDK path -----
_sdk_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         '..', '..', 'refs', 'HuayanRobotPythonSDK_V1.1.1.1')
sys.path.insert(0, _sdk_path)
from CPS import CPSClient  # type: ignore[import-untyped]

# ============ Global config ============
BOX_ID      = 0
RBT_ID      = 0
DEFAULT_VEL = 30.0
DEFAULT_ACC = 50.0
BLEND_R     = 5.0
TCP_NAME    = "TCP"
UCS_NAME    = "Base"

cps = CPSClient()
g_override = 0.3
g_dry_run  = False


# ============ Helpers ============

def check_ok(name: str, ret: int) -> bool:
    if ret != 0:
        result = []
        cps.HRIF_GetErrorCodeStr(BOX_ID, ret, result)
        err = result[0] if result else "unknown"
        print(f"  [FAIL] {name}: {ret} - {err}", file=sys.stderr)
        return False
    return True


def wait_blending_done(timeout_sec: int = 30) -> bool:
    if g_dry_run:
        print("  [DRY-RUN] Motion complete.")
        return True
    result = []
    for _ in range(timeout_sec * 10):
        time.sleep(0.1)
        cps.HRIF_IsBlendingDone(BOX_ID, RBT_ID, result)
        if result and result[0] == True:
            return True
        result.clear()
    return False


def print_help():
    print("""
========================================
  Huayan Robot Interactive Console (Python)
  Model: E05-1195_Pro
========================================

Commands:
  j  J1 J2 J3 J4 J5 J6  - Joint move (deg)
  l  X Y Z Rx Ry Rz     - Linear move (mm, deg)
  rj                    - Read current joint angles
  rp                    - Read current TCP pose
  rs                    - Read robot state
  stop                  - Stop motion
  reset                 - Reset errors
  recover               - Auto-recover to Standby (FSM 33)
  speed <0.1~1.0>      - Set speed override
  home                  - Go home (0,0,90,0,90,0)
  quit                  - Safe shutdown & exit
  help                  - Show this help

Examples:
  j 0 0 90 0 90 0      - Move to home joints
  l 400 0 400 180 0 180 - Linear move to pose
  speed 0.5             - Set 50% speed
========================================""")


# ============ FSM Recovery ============

def recover_to_standby(timeout_sec: int = 30) -> bool:
    if g_dry_run:
        print("  [DRY-RUN] FSM recovery skipped.")
        return True
    prev_fsm = -1
    for _ in range(timeout_sec * 2):
        result = []
        cps.HRIF_ReadCurFSM(BOX_ID, RBT_ID, result)
        fsm = int(result[0]) if result else -1
        sfsm = result[1] if len(result) > 1 else ""

        if fsm != prev_fsm:
            print(f"  FSM={fsm} ({sfsm})")
            prev_fsm = fsm

        if fsm in (5, 10, 12, 17, 21, 22):
            cps.HRIF_GrpReset(BOX_ID, RBT_ID)
        elif fsm == 7:
            cps.HRIF_Electrify(BOX_ID)
        elif fsm == 14:
            cps.HRIF_Connect2Controller(BOX_ID)
        elif fsm == 24:
            cps.HRIF_GrpEnable(BOX_ID, RBT_ID)
        elif fsm == 33:
            cps.HRIF_SetOverride(BOX_ID, RBT_ID, g_override)
            print("  [OK] Robot ready (Standby).")
            return True

        time.sleep(0.5)
    return False


# ============ Connect & Shutdown ============

def connect_and_init(host_ip: str) -> bool:
    print("\n[INIT] Connecting...")
    ret = cps.HRIF_Connect(BOX_ID, host_ip, 10003)
    if not check_ok("HRIF_Connect", ret):
        print("  Cannot connect to robot. Check network and IP.", file=sys.stderr)
        return False

    result = []
    cps.HRIF_ReadRobotModel(BOX_ID, RBT_ID, result)
    model = result[0] if result else "unknown"
    print(f"  Model: {model}")

    print("[INIT] State-machine initialisation...")
    if not recover_to_standby():
        print("[FAIL] Init timeout. Check robot state on pendant.", file=sys.stderr)
        cps.HRIF_DisConnect(BOX_ID)
        return False

    # Set default motion params
    cps.HRIF_SetOverride(BOX_ID, RBT_ID, g_override)
    cps.HRIF_SetJointMaxVel(BOX_ID, RBT_ID,
        [DEFAULT_VEL]*6)
    cps.HRIF_SetJointMaxAcc(BOX_ID, RBT_ID,
        [DEFAULT_ACC]*6)
    cps.HRIF_SetLinearMaxVel(BOX_ID, RBT_ID, DEFAULT_VEL)
    cps.HRIF_SetLinearMaxAcc(BOX_ID, RBT_ID, DEFAULT_ACC)
    return True


def safe_shutdown():
    print("\n[EXIT] Shutting down...")
    cps.HRIF_GrpDisable(BOX_ID, RBT_ID)
    time.sleep(0.5)
    cps.HRIF_BlackOut(BOX_ID)
    time.sleep(0.5)
    cps.HRIF_DisConnect(BOX_ID)
    print("[OK] Robot disconnected. Bye.")


# ============ Command handlers ============

def cmd_read_joints():
    if g_dry_run:
        print("  [DRY-RUN] Joints: 0 0 90 0 90 0")
        return
    result = []
    ret = cps.HRIF_ReadActJointPos(BOX_ID, RBT_ID, result)
    if check_ok("ReadActJointPos", ret):
        vals = [float(v) for v in result[:6]]
        print(f"  Joints(deg): J1={vals[0]:.3f} J2={vals[1]:.3f} J3={vals[2]:.3f} "
              f"J4={vals[3]:.3f} J5={vals[4]:.3f} J6={vals[5]:.3f}")


def cmd_read_pose():
    if g_dry_run:
        print("  [DRY-RUN] TCP: 400 0 400 180 0 180")
        return
    result = []
    ret = cps.HRIF_ReadActTcpPos(BOX_ID, RBT_ID, result)
    if check_ok("ReadActTcpPos", ret):
        vals = [float(v) for v in result[:6]]
        print(f"  TCP(mm,deg): X={vals[0]:.3f} Y={vals[1]:.3f} Z={vals[2]:.3f} "
              f"Rx={vals[3]:.3f} Ry={vals[4]:.3f} Rz={vals[5]:.3f}")


def cmd_read_state():
    if g_dry_run:
        print("  [DRY-RUN] FSM=33 (Standby) enable=1 moving=0 error=0")
        return
    result = []
    cps.HRIF_ReadCurFSM(BOX_ID, RBT_ID, result)
    fsm = int(result[0]) if result else -1
    sfsm = result[1] if len(result) > 1 else ""

    result2 = []
    cps.HRIF_ReadRobotState(BOX_ID, RBT_ID, result2)
    # result2 layout: moving, enable, error, errorCode, errorAxis,
    #                 breaking, pause, emergencyStop, safeGuard,
    #                 electrify, isConnectToBox, blendingDone, inPos
    en = result2[1] if len(result2) > 1 else '?'
    el2 = result2[9] if len(result2) > 9 else '?'
    m  = result2[0] if result2 else '?'
    er = result2[2] if len(result2) > 2 else '?'

    print(f"  FSM={fsm} ({sfsm})")
    print(f"  enable={en} electrify={el2} moving={m} error={er}")

    result3 = []
    cps.HRIF_ReadOverride(BOX_ID, RBT_ID, result3)
    ov = float(result3[0]) if result3 else 0
    print(f"  override={ov*100:.0f}%")


def cmd_move_j(args_list):
    if len(args_list) < 7:
        print("  Usage: j J1 J2 J3 J4 J5 J6", file=sys.stderr)
        return
    j = [float(args_list[i]) for i in range(1, 7)]

    print(f"  MoveJ → J1={j[0]} J2={j[1]} J3={j[2]} J4={j[3]} J5={j[4]} J6={j[5]}")
    if g_dry_run:
        print("  [DRY-RUN] Motion skipped.")
        return

    ret = cps.HRIF_MoveJ(BOX_ID, RBT_ID,
        [0]*6,          # pose (unused when isJoint=1)
        j,              # target joints
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        1,              # isJoint
        0, 0, 0,        # isSeek, bit, state
        "CMD")
    if check_ok("MoveJ", ret):
        if wait_blending_done():
            print("  [OK] Done.")
        else:
            print("  [WARN] Timeout.")


def cmd_move_l(args_list):
    if len(args_list) < 7:
        print("  Usage: l X Y Z Rx Ry Rz", file=sys.stderr)
        return
    x = [float(args_list[i]) for i in range(1, 7)]

    print(f"  MoveL → X={x[0]} Y={x[1]} Z={x[2]} Rx={x[3]} Ry={x[4]} Rz={x[5]}")
    if g_dry_run:
        print("  [DRY-RUN] Motion skipped.")
        return

    ret = cps.HRIF_MoveL(BOX_ID, RBT_ID,
        x,                              # target pose
        [0, 0, 90, 0, 90, 0],          # ref joints (non-singular)
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        0, 0, 0,                        # isSeek, bit, state
        "CMD")
    if check_ok("MoveL", ret):
        if wait_blending_done():
            print("  [OK] Done.")
        else:
            print("  [WARN] Timeout.")


def cmd_home():
    print("  Moving to home position (0,0,90,0,90,0)...")
    if g_dry_run:
        print("  [DRY-RUN] Motion skipped.")
        return

    ret = cps.HRIF_MoveJ(BOX_ID, RBT_ID,
        [0]*6, [0, 0, 90, 0, 90, 0],
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        1, 0, 0, 0, "HOME")
    if check_ok("MoveJ(home)", ret):
        if wait_blending_done():
            print("  [OK] Home reached.")
        else:
            print("  [WARN] Timeout.")


def cmd_stop():
    print("  Stopping...")
    if g_dry_run:
        print("  [DRY-RUN] Stop skipped.")
        return
    cps.HRIF_GrpStop(BOX_ID, RBT_ID)
    print("  [OK] Stop sent.")


def cmd_reset():
    print("  Resetting errors...")
    if g_dry_run:
        print("  [DRY-RUN] Reset skipped.")
        return
    ret = cps.HRIF_GrpReset(BOX_ID, RBT_ID)
    if check_ok("GrpReset", ret):
        print("  [OK] Reset done.")


def cmd_speed(args_list):
    global g_override
    if len(args_list) < 2:
        print("  Usage: speed <0.1~1.0>", file=sys.stderr)
        return
    g_override = max(0.01, min(1.0, float(args_list[1])))
    if not g_dry_run:
        cps.HRIF_SetOverride(BOX_ID, RBT_ID, g_override)
    print(f"  [OK] Override set to {g_override*100:.0f}%")


def cmd_recover():
    print("  Recovering to Standby...")
    if not recover_to_standby():
        print("  [FAIL] Recovery timeout. Check pendant.", file=sys.stderr)


# ============ Main ============

def main():
    global g_dry_run

    ip = "192.168.10.10"
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--dry-run":
            g_dry_run = True
        elif args[i] == "--ip" and i + 1 < len(args):
            i += 1
            ip = args[i]
        i += 1

    print("========================================")
    print("  Huayan Robot Interactive Console (Python)")
    print("  SDK V1.1.1.1  |  Python")
    if g_dry_run:
        print("  MODE: DRY-RUN (no robot connection)")
    else:
        print(f"  Connecting: {ip}:10003")
    print("========================================")

    if not g_dry_run:
        if not connect_and_init(ip):
            print("[FATAL] Cannot initialise robot. Exiting.", file=sys.stderr)
            sys.exit(1)

    print_help()

    # Command loop
    try:
        while True:
            try:
                line = input("> ").strip()
            except EOFError:
                break
            if not line:
                continue

            try:
                parts = shlex.split(line)
            except ValueError:
                parts = line.split()

            if not parts:
                continue

            cmd = parts[0].lower()

            if cmd in ("j", "movej"):
                cmd_move_j(parts)
            elif cmd in ("l", "movel"):
                cmd_move_l(parts)
            elif cmd in ("rj", "joints"):
                cmd_read_joints()
            elif cmd in ("rp", "pose"):
                cmd_read_pose()
            elif cmd in ("rs", "state"):
                cmd_read_state()
            elif cmd == "stop":
                cmd_stop()
            elif cmd == "reset":
                cmd_reset()
            elif cmd == "recover":
                cmd_recover()
            elif cmd == "speed":
                cmd_speed(parts)
            elif cmd == "home":
                cmd_home()
            elif cmd in ("help", "?"):
                print_help()
            elif cmd in ("quit", "exit", "q"):
                break
            else:
                print(f"  Unknown command: {cmd} (type help for list)", file=sys.stderr)

    except KeyboardInterrupt:
        print("\n[INTERRUPT]")

    if not g_dry_run:
        safe_shutdown()


if __name__ == "__main__":
    main()
