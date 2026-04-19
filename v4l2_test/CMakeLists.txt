cmake_minimum_required(VERSION 3.10)
project(v4l2_capture)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找OpenCV包
find_package(OpenCV REQUIRED)

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "未找到OpenCV库，请先安装OpenCV")
endif()

# 打印OpenCV信息
message(STATUS "OpenCV版本: ${OpenCV_VERSION}")
message(STATUS "OpenCV包含目录: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV库: ${OpenCV_LIBS}")

# 添加可执行文件
add_executable(v4l2_capture v4l2_capture.cpp)

# 链接OpenCV库
target_link_libraries(v4l2_capture ${OpenCV_LIBS})

# 包含OpenCV头文件目录
target_include_directories(v4l2_capture PRIVATE ${OpenCV_INCLUDE_DIRS})

# 编译选项
target_compile_options(v4l2_capture PRIVATE -Wall -Wextra)

# 安装目标
install(TARGETS v4l2_capture DESTINATION bin)
