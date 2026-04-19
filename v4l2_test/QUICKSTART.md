# 快速开始指南

## 📋 项目结构

```
v4l2_test/
├── v4l2_capture.cpp      # V4L2视频采集主程序
├── CMakeLists.txt        # CMake构建配置
├── build.sh              # 编译脚本
├── deploy.sh             # 部署脚本
├── install_opencv.sh     # OpenCV安装脚本
└── README.md             # 项目文档
```

## 🚀 使用流程

### 方案一：在OrangePi5开发板上直接编译（推荐）

#### 步骤1：传输源代码到开发板

```bash
# 在主机上执行
scp -r v4l2_test orangepi@<IP地址>:/home/orangepi/
```

#### 步骤2：在OrangePi5上安装OpenCV

```bash
# SSH登录到OrangePi5
ssh orangepi@<IP地址>

# 进入项目目录
cd v4l2_test

# 运行安装脚本
sudo ./install_opencv.sh
```

#### 步骤3：编译程序

```bash
# 使用编译脚本（本地编译模式）
./build.sh native

# 或者手动编译
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

#### 步骤4：运行程序

```bash
# 基本用法（采集30秒1080p视频）
sudo ./build/v4l2_capture

# 自定义参数
sudo ./build/v4l2_capture -w 1280 -h 720 -f 25 -t 60 -o test.avi
```

---

### 方案二：在主机上交叉编译

#### 步骤1：准备交叉编译环境

```bash
# 设置环境变量
export ARCH=arm64
export CROSS_COMPILE=/home/lu/Downloads/orangepi-build/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

# 设置OpenCV路径（需要先交叉编译OpenCV）
export OPENCV_DIR=/path/to/opencv/install
```

#### 步骤2：编译程序

```bash
# 使用编译脚本（交叉编译模式）
./build.sh

# 或者手动编译
mkdir -p build && cd build
cmake .. \
  -DCMAKE_CXX_COMPILER=${CROSS_COMPILE}g++ \
  -DCMAKE_C_COMPILER=${CROSS_COMPILE}gcc \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_PREFIX_PATH="${OPENCV_DIR}"
make -j$(nproc)
```

#### 步骤3：部署到开发板

```bash
scp build/v4l2_capture orangepi@<IP地址>:/home/orangepi/
```

#### 步骤4：在OrangePi5上运行

```bash
ssh orangepi@<IP地址>
sudo ./v4l2_capture
```

---

## 📝 常用命令示例

### 采集默认视频（1920x1080, 30fps, 30秒）
```bash
sudo ./v4l2_capture
```

### 采集720p视频60秒
```bash
sudo ./v4l2_capture -w 1280 -h 720 -t 60
```

### 指定摄像头设备
```bash
sudo ./v4l2_capture -d /dev/video1
```

### 降低帧率以减少CPU占用
```bash
sudo ./v4l2_capture -f 15
```

### 自定义输出文件
```bash
sudo ./v4l2_capture -o my_video.avi
```

### 完整参数示例
```bash
sudo ./v4l2_capture -d /dev/video0 -w 1920 -h 1080 -f 30 -t 30 -o output.avi
```

---

## 🔧 故障排查

### 问题1：找不到摄像头设备
```bash
# 检查可用的视频设备
ls -l /dev/video*

# 查看摄像头信息
v4l2-ctl --list-devices
```

### 问题2：OpenCV未安装
```bash
# 在OrangePi5上运行
sudo apt-get update
sudo apt-get install libopencv-dev
```

### 问题3：权限不足
```bash
# 使用sudo运行
sudo ./v4l2_capture

# 或者将用户添加到video组
sudo usermod -a -G video $USER
```

### 问题4：视频格式不支持
程序会自动尝试MJPEG和YUYV两种格式，如果都不支持，请检查：
```bash
# 查看摄像头支持的格式
v4l2-ctl --list-formats -d /dev/video0
```

### 问题5：编译时找不到OpenCV
```bash
# 确认OpenCV已正确安装
pkg-config --modversion opencv4
# 或
pkg-config --modversion opencv

# 查看OpenCV编译选项
pkg-config --cflags opencv4
pkg-config --libs opencv4
```

---

## 📊 性能优化建议

1. **降低分辨率**：如果遇到丢帧，尝试使用1280x720或640x480
2. **降低帧率**：设置为15或20 fps可以减少CPU占用
3. **使用高速存储**：将输出文件写入SSD或高速SD卡
4. **关闭其他应用**：释放系统资源给视频采集程序

---

## 📖 技术说明

### 视频采集流程
```
V4L2设备 → MMAP缓冲区 → JPEG数据 → OpenCV解码 → AVI文件
```

### 支持的像素格式
- **MJPEG** (优先)：压缩格式，带宽需求低，需要解码
- **YUYV** (备选)：未压缩格式，带宽需求高，处理简单

### 输出格式
- 容器格式：AVI
- 编码格式：MJPEG
- 适用场景：长时间录制、嵌入式设备

---

## 💡 提示

- 程序会在达到指定时长后自动停止
- 按 `Ctrl+C` 可以提前终止录制
- 录制的视频文件保存在当前工作目录
- 建议使用绝对路径指定输出文件

---

## 📞 需要帮助？

如有问题，请检查：
1. README.md 中的常见问题部分
2. OrangePi5官方文档
3. V4L2和OpenCV官方文档

祝使用愉快！🎉
