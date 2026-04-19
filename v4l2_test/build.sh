#!/bin/bash

# V4L2视频采集程序编译脚本
# 支持主机交叉编译和开发板本地编译

set -e

echo "========================================"
echo "V4L2 视频采集程序编译脚本"
echo "========================================"
echo ""

# 检测编译模式
if [ "$1" == "native" ]; then
    MODE="native"
    echo "编译模式: 本地编译（开发板）"
else
    MODE="cross"
    echo "编译模式: 交叉编译（主机）"
fi

echo ""

# 创建构建目录
BUILD_DIR="build"
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# 清理旧的构建文件
rm -rf *

if [ "${MODE}" == "cross" ]; then
    # 交叉编译配置
    echo "配置交叉编译环境..."
    
    # 检查交叉编译工具链
    CROSS_COMPILE="${CROSS_COMPILE:-/home/lu/Downloads/orangepi-build/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-}"
    
    if [ ! -f "${CROSS_COMPILE}gcc" ]; then
        echo "警告: 未找到交叉编译工具链，请设置CROSS_COMPILE环境变量"
        echo "当前路径: ${CROSS_COMPILE}"
        exit 1
    fi
    
    # 检查OpenCV路径
    OPENCV_DIR="${OPENCV_DIR:-}"
    if [ -z "${OPENCV_DIR}" ]; then
        echo "警告: 未设置OPENCV_DIR环境变量"
        echo "请使用: export OPENCV_DIR=/path/to/opencv/install"
        echo ""
        echo "尝试使用系统默认路径..."
    fi
    
    # 配置CMake
    cmake .. \
        -DCMAKE_CXX_COMPILER=${CROSS_COMPILE}g++ \
        -DCMAKE_C_COMPILER=${CROSS_COMPILE}gcc \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_PREFIX_PATH="${OPENCV_DIR}" \
        || {
            echo "CMake配置失败！"
            echo "请确保已正确设置交叉编译环境和OpenCV路径"
            exit 1
        }
else
    # 本地编译配置
    echo "配置本地编译环境..."
    
    # 检查是否安装了OpenCV
    if ! pkg-config --exists opencv4 2>/dev/null; then
        if ! pkg-config --exists opencv 2>/dev/null; then
            echo "错误: 未检测到OpenCV库"
            echo "请先安装OpenCV: sudo apt-get install libopencv-dev"
            exit 1
        fi
    fi
    
    # 配置CMake
    cmake ..
fi

# 编译
echo ""
echo "开始编译..."
make -j$(nproc)

# 检查编译结果
if [ -f "v4l2_capture" ]; then
    echo ""
    echo "========================================"
    echo "编译成功！"
    echo "========================================"
    echo "可执行文件: build/v4l2_capture"
    
    if [ "${MODE}" == "cross" ]; then
        echo ""
        echo "提示: 这是ARM64架构的可执行文件"
        echo "需要部署到OrangePi5开发板上运行"
        echo ""
        echo "部署示例:"
        echo "  scp v4l2_capture orangepi@<IP>:/home/orangepi/"
    else
        echo ""
        echo "可以在本地运行:"
        echo "  sudo ./v4l2_capture"
    fi
else
    echo ""
    echo "编译失败！"
    exit 1
fi
