# ============================================
#  华沿机械臂 SDK - CMakeLists.txt
#  MinGW: cmake -G "MinGW Makefiles" -B build
#  MSVC:  cmake -G "Visual Studio 17 2022" -B build
# ============================================
cmake_minimum_required(VERSION 3.15)
project(huayan_robot_ctrl VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
#  SDK Configuration
# ============================================
set(SDK_DIR "${CMAKE_SOURCE_DIR}/refs/HuayanRobotLibrary-C++-V1.0.15.0")

# Detect platform
if(MINGW)
    set(SDK_LIB_DIR "${SDK_DIR}/MinGW")
    set(SDK_DLL     "${SDK_LIB_DIR}/libHR_Pro.dll")
    set(SDK_DLL_DBG "${SDK_LIB_DIR}/libHR_Prod.dll")
elseif(MSVC)
    set(SDK_LIB_DIR "${SDK_DIR}/MSVC")
    set(SDK_DLL     "${SDK_LIB_DIR}/HR_Pro.dll")
    set(SDK_DLL_DBG "${SDK_LIB_DIR}/HR_Prod.dll")
else()
    message(FATAL_ERROR "Unsupported compiler. Use MinGW or MSVC.")
endif()

# Imported library
add_library(HR_Pro SHARED IMPORTED)
set_target_properties(HR_Pro PROPERTIES
    IMPORTED_LOCATION             "${SDK_DLL}"
    IMPORTED_LOCATION_DEBUG       "${SDK_DLL_DBG}"
    IMPORTED_IMPLIB               "${SDK_LIB_DIR}/libHR_Pro.dll.a"
    IMPORTED_IMPLIB_DEBUG         "${SDK_LIB_DIR}/libHR_Prod.dll.a"
)


# ============================================
#  Output directory
# ============================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/dist")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist")

# ============================================
#  Targets
# ============================================

# --- Sample demo program ---
add_executable(huayan_robot_ctrl
    sample/main.cpp
)
target_include_directories(huayan_robot_ctrl PRIVATE
    "${SDK_DIR}/include"
)
target_link_libraries(huayan_robot_ctrl PRIVATE HR_Pro)

# --- Interactive CLI tool ---
add_executable(cli
    src/cpp/apps/cli.cpp
)
target_include_directories(cli PRIVATE
    "${SDK_DIR}/include"
)
target_link_libraries(cli PRIVATE HR_Pro)

# --- Shutdown program ---
add_executable(shutdown
    src/cpp/apps/shutdown.cpp
)
target_include_directories(shutdown PRIVATE
    "${SDK_DIR}/include"
)
target_link_libraries(shutdown PRIVATE HR_Pro)


# ============================================
#  Post-build: Copy DLL to output
# ============================================
function(copy_dll target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${SDK_DLL_DBG}>$<$<CONFIG:Release>:${SDK_DLL}>"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copying HR_Pro DLL to output dir"
    )
endfunction()

copy_dll(huayan_robot_ctrl)
copy_dll(cli)
copy_dll(shutdown)


# ============================================
#  Compiler Options
# ============================================
if(MINGW)
    target_link_options(huayan_robot_ctrl PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(cli PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(shutdown PRIVATE -static-libgcc -static-libstdc++)
endif()
