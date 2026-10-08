/**
 * Huayan Robot Interactive CLI Control Tool (C#)
 * Based on HuayanRobotLibrary C# SDK V1.0.17.0
 * Target: .NET 10
 *
 * Usage:
 *   dotnet run -- [--ip 192.168.10.10] [--dry-run]
 *
 * Commands:
 *   j J1 J2 J3 J4 J5 J6  - Joint move
 *   l X Y Z Rx Ry Rz     - Linear move
 *   rj / rp / rs          - Read joints/pose/state
 *   stop / reset          - Stop / Reset errors
 *   recover               - Auto-recover to Standby (FSM 33)
 *   speed 0.1~1.0        - Set speed override
 *   home                  - Go to home (0,0,90,0,90,0)
 *   quit                  - Safe shutdown and exit
 *   help                  - Show this help
 */

using RobotLibrarys;

// ============ Global config ============
const int    BOX_ID      = 0;
const int    RBT_ID      = 0;
const double DEFAULT_VEL = 30.0;
const double DEFAULT_ACC = 50.0;
const double BLEND_R     = 5.0;
const string TCP_NAME    = "TCP";
const string UCS_NAME    = "Base";

RobotAPI hr_api = new();
double   g_override = 0.3;
bool     g_dryRun   = false;

// ============ Main ============
string ip = "192.168.10.10";

// Parse CLI args
for (int i = 0; i < args.Length; i++)
{
    if (args[i] == "--dry-run")        g_dryRun = true;
    else if (args[i] == "--ip" && i + 1 < args.Length) ip = args[++i];
}

Console.WriteLine("========================================");
Console.WriteLine("  Huayan Robot Interactive Console (C#)");
Console.WriteLine("  SDK V1.0.17.0  |  .NET 10");
if (g_dryRun)
    Console.WriteLine("  MODE: DRY-RUN (no robot connection)");
else
    Console.WriteLine($"  Connecting: {ip}:10003");
Console.WriteLine("========================================");

if (!g_dryRun)
{
    if (!ConnectAndInit(ip))
    {
        Console.Error.WriteLine("[FATAL] Cannot initialise robot. Exiting.");
        return 1;
    }
}

PrintHelp();
RunCommandLoop();

if (!g_dryRun) SafeShutdown();
return 0;

// ============ Init & Shutdown ============

bool ConnectAndInit(string hostIp)
{
    Console.WriteLine("\n[INIT] Connecting...");
    int ret = hr_api.HRIF_Connect(BOX_ID, hostIp, 10003);
    if (!CheckOK("HRIF_Connect", ret))
    {
        Console.Error.WriteLine("  Cannot connect to robot. Check network and IP.");
        return false;
    }

    string model = "";
    hr_api.HRIF_ReadRobotModel(BOX_ID, ref model);
    Console.WriteLine($"  Model: {model}");

    Console.WriteLine("[INIT] State-machine initialisation...");
    if (!RecoverToStandby())
    {
        Console.Error.WriteLine("[FAIL] Init timeout. Check robot state on pendant.");
        hr_api.HRIF_DisConnect(BOX_ID);
        return false;
    }

    // Set default motion params
    hr_api.HRIF_SetOverride(BOX_ID, RBT_ID, g_override);
    hr_api.HRIF_SetJointMaxVel(BOX_ID, RBT_ID,
        DEFAULT_VEL, DEFAULT_VEL, DEFAULT_VEL,
        DEFAULT_VEL, DEFAULT_VEL, DEFAULT_VEL);
    hr_api.HRIF_SetJointMaxAcc(BOX_ID, RBT_ID,
        DEFAULT_ACC, DEFAULT_ACC, DEFAULT_ACC,
        DEFAULT_ACC, DEFAULT_ACC, DEFAULT_ACC);
    hr_api.HRIF_SetLinearMaxVel(BOX_ID, RBT_ID, DEFAULT_VEL);
    hr_api.HRIF_SetLinearMaxAcc(BOX_ID, RBT_ID, DEFAULT_ACC);

    return true;
}

void SafeShutdown()
{
    Console.WriteLine("\n[EXIT] Shutting down...");
    hr_api.HRIF_GrpDisable(BOX_ID, RBT_ID);
    Thread.Sleep(500);
    hr_api.HRIF_BlackOut(BOX_ID);
    Thread.Sleep(500);
    hr_api.HRIF_DisConnect(BOX_ID);
    Console.WriteLine("[OK] Robot disconnected. Bye.");
}

// ============ Command loop ============

void RunCommandLoop()
{
    Console.Write("\n> ");
    string? line;
    while ((line = Console.ReadLine()) != null)
    {
        if (string.IsNullOrWhiteSpace(line)) { Console.Write("> "); continue; }

        var args2 = ParseLine(line);
        if (args2.Count == 0) { Console.Write("> "); continue; }

        string cmd = args2[0].ToLowerInvariant();

        try
        {
            switch (cmd)
            {
                case "j": case "movej":  CmdMoveJ(args2);       break;
                case "l": case "movel":  CmdMoveL(args2);       break;
                case "rj": case "joints": CmdReadJoints();      break;
                case "rp": case "pose":   CmdReadPose();        break;
                case "rs": case "state":  CmdReadState();       break;
                case "stop":              CmdStop();            break;
                case "reset":             CmdReset();           break;
                case "recover":           CmdRecover();         break;
                case "speed":             CmdSpeed(args2);      break;
                case "home":              CmdHome();            break;
                case "help": case "?":    PrintHelp();          break;
                case "quit": case "exit": case "q": return;
                default:
                    Console.Error.WriteLine($"  Unknown command: {cmd} (type help for list)");
                    break;
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"  [ERROR] {ex.Message}");
        }

        Console.Write("> ");
    }
}

// ============ Helpers ============

List<string> ParseLine(string line)
{
    var parts = new List<string>();
    bool inQuote = false;
    string current = "";
    foreach (char c in line)
    {
        if (c == '"') { inQuote = !inQuote; continue; }
        if (char.IsWhiteSpace(c) && !inQuote)
        {
            if (current.Length > 0) { parts.Add(current); current = ""; }
        }
        else current += c;
    }
    if (current.Length > 0) parts.Add(current);
    return parts;
}

bool CheckOK(string name, int ret)
{
    if (ret != 0)
    {
        string err = "";
        hr_api.HRIF_GetErrorCodeStr(BOX_ID, ret, ref err);
        Console.Error.WriteLine($"  [FAIL] {name}: {ret} - {err}");
        return false;
    }
    return true;
}

bool WaitBlendingDone(int timeoutSec = 30)
{
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] Motion complete."); return true; }
    bool done = false;
    for (int w = 0; w < timeoutSec * 10 && !done; w++)
    {
        Thread.Sleep(100);
        hr_api.HRIF_IsBlendingDone(BOX_ID, RBT_ID, ref done);
    }
    return done;
}

void PrintHelp()
{
    Console.WriteLine(@"
========================================");
    Console.WriteLine(@"  Huayan Robot Interactive Console (C#)
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
========================================");
}

// ============ FSM Recovery ============

bool RecoverToStandby(int timeoutSec = 30)
{
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] FSM recovery skipped."); return true; }
    bool ready = false;
    int loops = 0, maxLoops = timeoutSec * 2, prevFSM = -1;

    while (!ready && loops < maxLoops)
    {
        int fsm = 0; string sfsm = "";
        hr_api.HRIF_ReadCurFSM(BOX_ID, RBT_ID, ref fsm, ref sfsm);

        if (fsm != prevFSM)
        {
            Console.WriteLine($"  FSM={fsm} ({sfsm})");
            prevFSM = fsm;
        }

        switch (fsm)
        {
            case 5: case 10: case 12: case 17: case 21: case 22:
                hr_api.HRIF_GrpReset(BOX_ID, RBT_ID);  break;
            case 7:
                hr_api.HRIF_Electrify(BOX_ID);          break;
            case 14:
                hr_api.HRIF_Connect2Controller(BOX_ID); break;
            case 24:
                hr_api.HRIF_GrpEnable(BOX_ID, RBT_ID);  break;
            case 33:
                ready = true;                            break;
        }
        loops++;
        Thread.Sleep(500);
    }

    if (ready)
    {
        hr_api.HRIF_SetOverride(BOX_ID, RBT_ID, g_override);
        Console.WriteLine("  [OK] Robot ready (Standby).");
        return true;
    }
    return false;
}

// ============ Command handlers ============

void CmdReadJoints()
{
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] Joints: 0 0 90 0 90 0"); return; }
    double j1=0,j2=0,j3=0,j4=0,j5=0,j6=0;
    int ret = hr_api.HRIF_ReadActJointPos(BOX_ID, RBT_ID, ref j1, ref j2, ref j3, ref j4, ref j5, ref j6);
    if (CheckOK("ReadActJointPos", ret))
        Console.WriteLine($"  Joints(deg): J1={j1:F3} J2={j2:F3} J3={j3:F3} J4={j4:F3} J5={j5:F3} J6={j6:F3}");
}

void CmdReadPose()
{
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] TCP: 400 0 400 180 0 180"); return; }
    double x=0,y=0,z=0,rx=0,ry=0,rz=0;
    int ret = hr_api.HRIF_ReadActTcpPos(BOX_ID, RBT_ID, ref x, ref y, ref z, ref rx, ref ry, ref rz);
    if (CheckOK("ReadActTcpPos", ret))
        Console.WriteLine($"  TCP(mm,deg): X={x:F3} Y={y:F3} Z={z:F3} Rx={rx:F3} Ry={ry:F3} Rz={rz:F3}");
}

void CmdReadState()
{
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] FSM=33 (Standby) enable=1 moving=0 error=0"); return; }
    int fsm=0; string sfsm="";
    hr_api.HRIF_ReadCurFSM(BOX_ID, RBT_ID, ref fsm, ref sfsm);

    int m=0,en=0,er=0,ec=0,ea=0,br=0,pa=0,es=0,sg=0,el2=0,cb=0,bd=0,ip=0;
    hr_api.HRIF_ReadRobotState(BOX_ID, RBT_ID,
        ref m, ref en, ref er, ref ec, ref ea, ref br, ref pa, ref es, ref sg, ref el2, ref cb, ref bd, ref ip);

    Console.WriteLine($"  FSM={fsm} ({sfsm})");
    Console.WriteLine($"  enable={en} electrify={el2} moving={m} error={er}");

    double ov=0;
    hr_api.HRIF_ReadOverride(BOX_ID, RBT_ID, ref ov);
    Console.WriteLine($"  override={ov*100:F0}%");
}

void CmdMoveJ(List<string> args2)
{
    if (args2.Count < 7) { Console.Error.WriteLine("  Usage: j J1 J2 J3 J4 J5 J6"); return; }
    double[] j = new double[6];
    for (int i = 0; i < 6; i++) j[i] = double.Parse(args2[i + 1]);

    Console.WriteLine($"  MoveJ → J1={j[0]} J2={j[1]} J3={j[2]} J4={j[3]} J5={j[4]} J6={j[5]}");
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] Motion skipped."); return; }

    int ret = hr_api.HRIF_MoveJ(BOX_ID, RBT_ID,
        0,0,0,0,0,0,
        j[0], j[1], j[2], j[3], j[4], j[5],
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        1, 0, 0, 0, "CMD");
    if (CheckOK("MoveJ", ret))
    {
        if (WaitBlendingDone()) Console.WriteLine("  [OK] Done.");
        else Console.WriteLine("  [WARN] Timeout.");
    }
}

void CmdMoveL(List<string> args2)
{
    if (args2.Count < 7) { Console.Error.WriteLine("  Usage: l X Y Z Rx Ry Rz"); return; }
    double[] x = new double[6];
    for (int i = 0; i < 6; i++) x[i] = double.Parse(args2[i + 1]);

    Console.WriteLine($"  MoveL → X={x[0]} Y={x[1]} Z={x[2]} Rx={x[3]} Ry={x[4]} Rz={x[5]}");
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] Motion skipped."); return; }

    // Use non-singular reference joint angles
    int ret = hr_api.HRIF_MoveL(BOX_ID, RBT_ID,
        x[0], x[1], x[2], x[3], x[4], x[5],
        0, 0, 90, 0, 90, 0,
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        0, 0, 0, "CMD");
    if (CheckOK("MoveL", ret))
    {
        if (WaitBlendingDone()) Console.WriteLine("  [OK] Done.");
        else Console.WriteLine("  [WARN] Timeout.");
    }
}

void CmdHome()
{
    Console.WriteLine("  Moving to home position (0,0,90,0,90,0)...");
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] Motion skipped."); return; }

    int ret = hr_api.HRIF_MoveJ(BOX_ID, RBT_ID,
        0,0,0,0,0,0,
        0,0,90,0,90,0,
        TCP_NAME, UCS_NAME,
        DEFAULT_VEL, DEFAULT_ACC, BLEND_R,
        1, 0, 0, 0, "HOME");
    if (CheckOK("MoveJ(home)", ret))
    {
        if (WaitBlendingDone()) Console.WriteLine("  [OK] Home reached.");
        else Console.WriteLine("  [WARN] Timeout.");
    }
}

void CmdStop()
{
    Console.WriteLine("  Stopping...");
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] Stop skipped."); return; }
    hr_api.HRIF_GrpStop(BOX_ID, RBT_ID);
    Console.WriteLine("  [OK] Stop sent.");
}

void CmdReset()
{
    Console.WriteLine("  Resetting errors...");
    if (g_dryRun) { Console.WriteLine("  [DRY-RUN] Reset skipped."); return; }
    int ret = hr_api.HRIF_GrpReset(BOX_ID, RBT_ID);
    if (CheckOK("GrpReset", ret)) Console.WriteLine("  [OK] Reset done.");
}

void CmdSpeed(List<string> args2)
{
    if (args2.Count < 2) { Console.Error.WriteLine("  Usage: speed <0.1~1.0>"); return; }
    g_override = double.Parse(args2[1]);
    g_override = Math.Clamp(g_override, 0.01, 1.0);
    if (!g_dryRun) hr_api.HRIF_SetOverride(BOX_ID, RBT_ID, g_override);
    Console.WriteLine($"  [OK] Override set to {g_override*100:F0}%");
}

void CmdRecover()
{
    Console.WriteLine("  Recovering to Standby...");
    if (!RecoverToStandby())
        Console.Error.WriteLine("  [FAIL] Recovery timeout. Check pendant.");
}
