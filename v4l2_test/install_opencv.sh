#!/bin/bash

# OpenCV安装脚本（用于OrangePi5开发板）
# 此脚本会在OrangePi5上安装OpenCV库

set -e

echo "========================================"
echo "在OrangePi5上安装OpenCV"
echo "========================================"
echo ""

# 检查是否为root用户
if [ "$EUID" -ne 0 ]; then 
    echo "请使用sudo运行此脚本"
    echo "执行: sudo $0"
    exit 1
fi

# 更新软件包列表
echo "正在更新软件包列表..."
apt-get update

# 安装OpenCV开发库
echo ""
echo "正在安装OpenCV开发库..."
apt-get install -y libopencv-dev

# 安装视频编码相关库
echo ""
echo "正在安装视频编码库..."
apt-get install -y ffmpeg gstreamer1.0-plugins-good

# 验证安装
echo ""
echo "验证OpenCV安装..."
if pkg-config --exists opencv4; then
    OPENCV_VERSION=$(pkg-config --modversion opencv4)
    echo "✓ OpenCV 4 已安装 (版本: ${OPENCV_VERSION})"
elif pkg-config --exists opencv; then
    OPENCV_VERSION=$(pkg-config --modversion opencv)
    echo "✓ OpenCV 已安装 (版本: ${OPENCV_VERSION})"
else
    echo "✗ OpenCV安装失败"
    exit 1
fi

echo ""
echo "========================================"
echo "OpenCV安装完成！"
echo "========================================"
echo ""
echo "现在可以编译v4l2_capture程序了:"
echo "  cd v4l2_test"
echo "  mkdir -p build && cd build"
echo "  cmake .."
echo "  make"
echo ""
