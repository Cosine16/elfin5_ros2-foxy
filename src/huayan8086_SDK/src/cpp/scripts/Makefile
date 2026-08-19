# ============================================
#  华沿机械臂 SDK - Makefile (MinGW)
# ============================================

# Compiler
CXX      := g++
CXXFLAGS := -Wall -std=c++17
LDFLAGS  := -static-libgcc -static-libstdc++

# Project root relative to this Makefile's location
ROOT_DIR  := ..\..\..
DIST_DIR  := $(ROOT_DIR)\dist

# SDK paths
SDK_DIR   := $(ROOT_DIR)\refs\HuayanRobotLibrary-C++-V1.0.15.0
INCLUDE   := -I"$(SDK_DIR)\include"
LIB_DIR   := -L"$(SDK_DIR)\MinGW"

# Targets
TARGETS   := $(DIST_DIR)\huayan_robot_ctrl.exe $(DIST_DIR)\cli.exe $(DIST_DIR)\shutdown.exe

# Build mode (default: release)
MODE ?= release

ifeq ($(MODE),debug)
    CXXFLAGS += -g -O0
    LINK_DLL  = libHR_Prod
    DLL_SRC   = $(SDK_DIR)\MinGW\libHR_Prod.dll
    DLL_DST   = $(DIST_DIR)\libHR_Prod.dll
else
    CXXFLAGS += -O2
    LINK_DLL  = libHR_Pro
    DLL_SRC   = $(SDK_DIR)\MinGW\libHR_Pro.dll
    DLL_DST   = $(DIST_DIR)\libHR_Pro.dll
endif


# ============================================
#  Rules
# ============================================

.PHONY: all release debug clean check info

all: release

release:
	@if not exist "$(DIST_DIR)" mkdir "$(DIST_DIR)"
	$(MAKE) $(TARGETS) MODE=release

debug:
	@if not exist "$(DIST_DIR)" mkdir "$(DIST_DIR)"
	$(MAKE) $(TARGETS) MODE=debug

# Build rules
$(DIST_DIR)\huayan_robot_ctrl.exe: $(ROOT_DIR)\sample\main.cpp $(DLL_DST)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(LIB_DIR) "$(ROOT_DIR)\sample\main.cpp" -o $@ -lHR_Pro $(LDFLAGS)
	@echo [OK] $@ built ($(MODE))

$(DIST_DIR)\cli.exe: ..\apps\cli.cpp $(DLL_DST)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(LIB_DIR) ..\apps\cli.cpp -o $@ -lHR_Pro $(LDFLAGS)
	@echo [OK] $@ built ($(MODE))

$(DIST_DIR)\shutdown.exe: ..\apps\shutdown.cpp $(DLL_DST)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(LIB_DIR) ..\apps\shutdown.cpp -o $@ -lHR_Pro $(LDFLAGS)
	@echo [OK] $@ built ($(MODE))

# Copy DLL
$(DLL_DST):
	@if not exist "$(DIST_DIR)" mkdir "$(DIST_DIR)"
	@if exist "$(DLL_SRC)" ( \
	    copy /Y "$(DLL_SRC)" "$(DLL_DST)" >nul && echo [OK] $(DLL_DST) copied \
	) else ( \
	    echo [ERROR] DLL not found: $(DLL_SRC) && exit /b 1 \
	)

# Check toolchain
check:
	@where g++ >nul 2>&1 && echo [OK] g++ found || echo [ERROR] g++ not in PATH
	@if exist "$(SDK_DIR)\include\HR_Pro.h" (echo [OK] SDK header found) else (echo [ERROR] SDK header not found)
	@if exist "$(SDK_DIR)\MinGW\libHR_Pro.dll.a" (echo [OK] SDK lib found) else (echo [ERROR] SDK lib not found)

# Info
info:
	@echo ==========================================
	@echo  Targets: $(TARGETS)
	@echo  Mode:    $(MODE)
	@echo  CXX:     $(CXX)
	@echo  SDKDIR:  $(SDK_DIR)
	@echo  DISTDIR: $(DIST_DIR)
	@echo ==========================================

# Clean
clean:
	@if exist "$(DIST_DIR)" rmdir /S /Q "$(DIST_DIR)"
	@echo [OK] Cleaned
