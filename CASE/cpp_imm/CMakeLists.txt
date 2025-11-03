# CMake 最低版本要求, 推荐 3.12 以获得更好的平台检测能力
cmake_minimum_required(VERSION 3.12)

# 定义项目名称和支持的语言
project(InfluenceMaximizationAPI LANGUAGES CXX C)

# 设置C++标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 定义源文件 ---
set(SOURCES
    ../cpp_imm/influence_calculator.cpp
    ../cpp_imm/sfmt/SFMT.c
)

# --- 添加可执行文件 ---
add_executable(influence_api_server ${SOURCES})

# --- 配置头文件搜索路径 ---
target_include_directories(influence_api_server PUBLIC
    ../cpp_imm
    ../cpp_imm/sfmt
)

# --- 【核心修改】跨平台的库链接 ---
# 根据操作系统来链接正确的 UUID 库
if(WIN32)
    # 在 Windows 上, UUID 功能在 Rpcrt4.lib 中
    message(STATUS "Windows detected. Linking with Rpcrt4.lib for UUID.")
    target_link_libraries(influence_api_server PRIVATE Rpcrt4.lib)
elseif(APPLE)
    # 在 macOS 上, UUID 功能通常由核心框架提供，CMake 可以自动找到
    message(STATUS "macOS detected. Linking with system UUID library.")
    # 一般情况下，现代Xcode工具链会自动处理，无需显式链接。
    # 如果遇到链接错误 "symbol not found for _uuid_generate", 
    # 可以尝试 find_library(UUID_LIB uuid) 并链接 ${UUID_LIB}
else()
    # 在 Linux 上, 使用 pkg-config 是标准做法
    message(STATUS "Linux/Unix detected. Finding UUID via pkg-config.")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(UUID REQUIRED uuid)
    target_link_libraries(influence_api_server PRIVATE ${UUID_LIBRARIES})
endif()

# --- 【核心修改】跨平台的编译选项 ---
if(MSVC)
    # Visual Studio MSVC 编译器的选项
    message(STATUS "MSVC compiler detected. Using /W3 and /O2 flags.")
    target_compile_options(influence_api_server PRIVATE /W3 /O2)
else()
    # GCC/Clang 编译器的选项
    message(STATUS "GCC/Clang compiler detected. Using -Wall and -O3 flags.")
    target_compile_options(influence_api_server PRIVATE -Wall -O3)
endif()

# --- 安装指令 (可选) ---
install(TARGETS influence_api_server
        DESTINATION bin)

# --- 打印构建信息 ---
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Executable 'influence_api_server' will be built.")