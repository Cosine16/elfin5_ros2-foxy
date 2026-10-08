import sys
import os
import time
import csv
# 引入python的SDK
_sdk_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'refs', 'HuayanRobotPythonSDK_V1.1.1.1')
sys.path.append(_sdk_path)

from CPS import CPSClient  # type: ignore[import-untyped]

#--------------------------------新增代码位---------------------------------

#--------------------------------连接机器人原始代码位---------------------------------
def Connect(IP):
    nPort = 10003
    cps = CPSClient()

    nRet = cps.HRIF_Connect(0, IP, nPort)
    print(nRet)
    if nRet != 0:
        print("连接失败")
    else:
        print("连接成功")
    nRet1 = cps.g_clients[0].sendAndRecv('SetTCPByName,0,TCP_2,;', result := [])
    print(nRet1)
    while True:
        time_start = time.time()
        print(time_start)
        nRet = cps.HRIF_GrpReset(0, 0)
        time.sleep(1)
        result1 = []
        nRet = cps.HRIF_ReadRobotState(0, 0, result1)
        print(result1)
        print(result1[7] + "******0：未处于急停状态/1：已处于急停状态******")
        if result1[7] == '1':
            print("疑似物理急停未松开")
            break
        if result1[9] == '1':
            nRet = cps.HRIF_GrpEnable(0, 0)
        elif result1[9] == '0':
            nRet = cps.HRIF_Connect2Controller(0)
            print("机器人正在上电中请稍等.......")
            time.sleep(15)
        if result1[1] == '1':
            print("机器运动前准备成功")
            time_end = time.time()
            print(time_end)
            t = time_start - time_end
            print(t)
            break

    time.sleep(1)
    dOverride = 0.6
    nRet = cps.HRIF_SetOverride(0, 0, dOverride)

    result = []
    nRet = cps.HRIF_ReadActJointPos(0, 0, result)
    print(result)
    print(nRet)

    result = []
    nRet = cps.HRIF_ReadActJointPos(0, 0, result)
    print('实际位置坐标 :' + str(result))

    t90 = 90
    nMoveType = 0
    Point = [0, 0, 0, 0, 0, 0]
    rawACSt90 = [t90, t90, t90, t90, t90, t90]
    sTcpName = "TCP"
    sUcsName = "Base"
    dVelocity = 50
    dAcc = 50
    dRadius = 50
    nIsUseJoint = 1
    nIsSeek = 0
    nIOBit = 0
    nIOState = 0
    stdCmdID = "0"
    t_90 = -90
    rawACSt_90 = [t_90, t_90, t_90, t_90, t_90, t_90]
    i=0
    for i in range(10):
        i = i + 1
        nRet = cps.HRIF_WayPoint(0, 0, nMoveType, Point, rawACSt90, sTcpName, sUcsName, dVelocity, dAcc, dRadius,
                                 nIsUseJoint, nIsSeek, nIOBit, nIOState, stdCmdID)
        nRet = cps.HRIF_WayPoint(0, 0, nMoveType, Point, rawACSt_90, sTcpName, sUcsName, dVelocity, dAcc, dRadius,
                                 nIsUseJoint, nIsSeek, nIOBit, nIOState, stdCmdID)
        print(i)
if __name__ == '__main__':
    t1 = time.time()
    IP = input("请输入机器人的IP地址：")
    Connect(IP)
    result = []
    t2 = time.time()
    print(t2 - t1)
