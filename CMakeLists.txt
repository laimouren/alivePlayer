# 设置最低CMake版本
cmake_minimum_required(VERSION 2.8)

# 设置项目名称
project(ALIVEPLAYER)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)

# 添加源文件
add_executable(ALIVEPLAYER src/main.cpp)