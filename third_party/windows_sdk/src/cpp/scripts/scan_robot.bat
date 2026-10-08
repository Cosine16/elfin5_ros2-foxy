@echo off
REM ============================================
REM  华沿机器人网络扫描工具
REM  支持多子网 ICMP Ping 扫描
REM ============================================

echo ========================================
echo  华沿机器人网络扫描工具
echo ========================================
echo.

REM 尝试自动检测本机IP段
set "found_subnet="
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /c:"192.168." ^| findstr /v "31."') do (
    for /f "tokens=1-3 delims=." %%b in ("%%a") do (
        set "found_subnet=%%b.%%c.%%d"
    )
)

if "%found_subnet%"=="" (
    echo 未自动检测到直连子网，尝试常见子网...
    set "subnets=192.168.156 192.168.0 192.168.1 192.168.10"
) else (
    echo 检测到本机子网: %found_subnet%.x
    set "subnets=%found_subnet% 192.168.156 192.168.0 192.168.1"
)

echo.
echo 正在扫描...
echo.

for %%s in (%subnets%) do (
    echo [扫描 %%s.x]
    for /L %%i in (1,1,254) do (
        ping -n 1 -w 10 %%s.%%i >nul 2>&1 && echo   FOUND: %%s.%%i
    )
)

echo.
echo 扫描完成!
echo.
echo 如果仍未找到机器人，请检查:
echo   1. 机器人是否已开机
echo   2. 网线是否正确连接
echo   3. 本机IP配置: 192.168.156.2 / 255.255.255.0
echo.
pause
