/**
 * 华沿机器人 - 简易关闭程序
 * 尝试断开连接、断电、关机
 */
#include <iostream>
#include <windows.h>
#include "HR_Pro.h"

int main(int argc, char* argv[]) {
    const char* ip = (argc >= 2) ? argv[1] : "192.168.10.10";
    
    std::cout << "Connecting to " << ip << ":10003 ..." << std::endl;
    int ret = HRIF_Connect(0, ip, 10003);
    if (ret != 0) {
        std::cerr << "Connect failed: " << ret << std::endl;
        std::cerr << "(Robot may already be off or disconnected)" << std::endl;
        system("pause");
        return 1;
    }
    std::cout << "Connected." << std::endl;
    
    // 尝试关机 (OSCmd type 1: shutdown)
    std::cout << "\nTrying HRIF_ShutdownRobot ..." << std::endl;
    ret = HRIF_ShutdownRobot(0);
    if (ret == 0) {
        std::cout << "Shutdown command sent. Robot will power off." << std::endl;
    } else {
        std::cout << "ShutdownRobot failed (ret=" << ret << ")" << std::endl;
        
        // 尝试断电
        std::cout << "\nTrying HRIF_Blackout ..." << std::endl;
        ret = HRIF_Blackout(0);
        if (ret == 0) {
            std::cout << "Blackout OK." << std::endl;
        } else {
            std::cout << "Blackout failed (ret=" << ret << ")" << std::endl;
        }
    }
    
    // 断开
    std::cout << "\nDisconnecting ..." << std::endl;
    HRIF_DisConnect(0);
    std::cout << "Done." << std::endl;
    
    system("pause");
    return 0;
}
