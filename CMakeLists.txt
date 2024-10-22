# 设置最低CMake版本
cmake_minimum_required(VERSION 2.8)

# 设置项目名称
project(ALIVEPLAYER)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)

# # 找ffmpeg的头文件和库文件
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
)

# 添加源文件
add_executable(ALIVEPLAYER src/main.cpp)

# 包含FFmpeg的头文件
target_include_directories(ALIVEPLAYER PRIVATE ${FFMPEG_INCLUDE_DIRS})

# 链接FFmpeg的库
target_link_libraries(ALIVEPLAYER PRIVATE PkgConfig::FFMPEG)