@echo off
REM ============================================
REM  华沿机械臂 SDK 编译脚本 (MinGW)
REM  从 src/cpp/scripts/ 目录运行
REM ============================================

set "ROOT_DIR=..\..\.."
set "DIST_DIR=%ROOT_DIR%\dist"
set "SDK_DIR=%ROOT_DIR%\refs\HuayanRobotLibrary-C++-V1.0.15.0"
set "INCLUDE_DIR=%SDK_DIR%\include"
set "LIB_DIR=%SDK_DIR%\MinGW"
set "SRC_FILE=%ROOT_DIR%\sample\main.cpp"
set "OUT_EXE=%DIST_DIR%\huayan_robot_ctrl.exe"

echo ========================================
echo  华沿机械臂控制程序 - 编译
echo ========================================
echo.

REM 检查SDK目录
if not exist "%INCLUDE_DIR%\HR_Pro.h" (
    echo [ERROR] SDK header not found: %INCLUDE_DIR%\HR_Pro.h
    pause
    exit /b 1
)

if not exist "%LIB_DIR%\libHR_Pro.dll.a" (
    echo [ERROR] SDK library not found: %LIB_DIR%\libHR_Pro.dll.a
    pause
    exit /b 1
)

REM 创建输出目录
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

echo [INFO] Compiling %SRC_FILE% ...
echo.

REM 编译 (Release版, 使用 libHR_Pro.dll)
g++ -Wall -std=c++17 -O2 ^
    -I"%INCLUDE_DIR%" ^
    -L"%LIB_DIR%" ^
    "%SRC_FILE%" ^
    -o "%OUT_EXE%" ^
    -lHR_Pro ^
    -static-libgcc -static-libstdc++

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed!
    pause
    exit /b 1
)

REM 复制DLL到输出目录
echo.
echo [INFO] Copying DLL files...
if exist "%LIB_DIR%\libHR_Pro.dll" (
    copy /Y "%LIB_DIR%\libHR_Pro.dll" "%DIST_DIR%" >nul
    echo   - libHR_Pro.dll copied to %DIST_DIR%
)

echo.
echo ========================================
echo  编译成功!
echo  输出文件: %OUT_EXE%
echo ========================================
echo.
echo 使用方法:
echo   %OUT_EXE% [机器人IP]

echo.
pause
