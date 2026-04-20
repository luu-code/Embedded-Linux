# V4L2 视频采集程序

这是一个使用V4L2子系统采集视频流的示例项目，提供了两种不同的缓冲区管理方式实现。

## 📁 项目文件说明

### 1. **v4l2_sample.c** - MMAP方式的V4L2示例程序 ⭐

**这是一个使用内存映射(MMAP)方式的V4L2视频采集示例程序！**

- ✅ 纯C语言实现，无第三方依赖
- ✅ 代码简洁清晰，适合学习V4L2基本原理
- ✅ 演示完整的V4L2工作流程
- ✅ 捕获10帧图像并保存为JPEG文件
- ✅ 使用内存映射(MMAP)方式高效采集
- ✅ 包含完善的错误处理和资源管理

**适用场景：**
- 学习V4L2 API的基本使用方法
- 快速测试摄像头是否正常工作
- 理解V4L2的核心概念和工作流程
- 作为开发更复杂应用的起点

**编译和运行：**
```bash
# 编译MMAP版本
make mmap

# 运行（需要root权限或sudo）
sudo ./v4l2_sample

# 清理
make clean
```

**输出：**
程序会在当前目录生成 `frame-0.jpg` 到 `frame-9.jpg` 共10个JPEG图像文件。

---

### 2. **v4l2_dma_buf.c** - DMA_BUF方式的V4L2示例程序 🚀

**这是一个使用DMA_BUF方式的V4L2视频采集示例程序，提供零拷贝高性能传输！**

- ✅ 纯C语言实现，无第三方依赖
- ✅ 使用DMA_BUF实现零拷贝数据传输
- ✅ 在嵌入式系统中性能更优
- ✅ 减少CPU开销和内存带宽占用
- ✅ 捕获10帧图像并保存为JPEG文件
- ✅ 包含完善的错误处理和资源管理

**技术优势：**
- **零拷贝**: 数据直接从硬件传输到用户空间，无需内核态拷贝
- **高性能**: 特别适合高分辨率、高帧率的视频采集
- **低延迟**: 减少数据处理延迟，适合实时应用
- **嵌入式友好**: 在OrangePi等嵌入式平台上表现更佳

**适用场景：**
- 高性能视频采集应用
- 嵌入式系统视频处理
- 实时视频流处理
- 需要降低CPU负载的场景

**编译和运行：**
```bash
# 编译DMA_BUF版本
make dma_buf

# 运行（需要root权限或sudo）
sudo ./v4l2_dma_buf

# 清理
make clean
```

**输出：**
程序会在当前目录生成 `dma_frame-0.jpg` 到 `dma_frame-9.jpg` 共10个JPEG图像文件。

---

### 3. **v4l2_capture.cpp** - 完整的视频采集程序

功能更强大的C++实现，支持：
- ✅ 通过V4L2子系统采集视频流
- ✅ 支持MJPEG和YUYV格式自动切换
- ✅ 使用OpenCV进行JPEG解码
- ✅ 保存为AVI格式视频文件
- ✅ 可配置分辨率、帧率、采集时长
- ✅ 支持命令行参数
- ✅ 支持Ctrl+C优雅退出

详细说明请参考下方的"完整视频采集程序"部分。

---

## 完整视频采集程序 (v4l2_capture.cpp)

### 功能特性

- ✅ 通过V4L2子系统采集视频流
- ✅ 支持MJPEG和YUYV格式自动切换
- ✅ 使用OpenCV进行JPEG解码
- ✅ 保存为AVI格式视频文件
- ✅ 可配置分辨率、帧率、采集时长
- ✅ 支持命令行参数
- ✅ 支持Ctrl+C优雅退出
- ✅ 内存映射(MMAP)方式高效采集

### 依赖项

- OpenCV 3.x 或 4.x
- Linux系统（支持V4L2）
- CMake 3.10+

### 编译方法

#### 在主机上交叉编译（用于OrangePi5）

```bash
# 设置交叉编译环境变量
export ARCH=arm64
export CROSS_COMPILE=/home/lu/Downloads/orangepi-build/toolchains/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

# 创建构建目录
mkdir -p build && cd build

# 配置CMake（需要指定OpenCV路径）
cmake .. \
  -DCMAKE_CXX_COMPILER=${CROSS_COMPILE}g++ \
  -DCMAKE_C_COMPILER=${CROSS_COMPILE}gcc \
  -DCMAKE_PREFIX_PATH=/path/to/opencv/install

# 编译
make -j$(nproc)
```

#### 在OrangePi5开发板上直接编译

```bash
# 首先安装OpenCV（如果尚未安装）
sudo apt-get update
sudo apt-get install libopencv-dev

# 创建构建目录并编译
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 使用方法

#### 基本用法

```bash
./v4l2_capture
```

默认参数：
- 设备: /dev/video0
- 分辨率: 1920x1080
- 帧率: 30 fps
- 采集时长: 30 秒
- 输出文件: output.avi

#### 自定义参数

```bash
# 指定设备和分辨率
./v4l2_capture -d /dev/video1 -w 1280 -h 720 -f 25 -t 60 -o myvideo.avi

# 查看帮助信息
./v4l2_capture --help
```

#### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| -d   | 视频设备路径 | /dev/video0 |
| -w   | 图像宽度 | 1920 |
| -h   | 图像高度 | 1080 |
| -f   | 帧率 | 30 |
| -t   | 采集时长（秒） | 30 |
| -o   | 输出文件名 | output.avi |
| --help | 显示帮助信息 | - |

### 部署到OrangePi5

1. **编译完成后传输可执行文件**：
```bash
scp build/v4l2_capture orangepi@<IP地址>:/home/orangepi/
```

2. **在OrangePi5上运行**：
```bash
chmod +x v4l2_capture
sudo ./v4l2_capture
```

## MMAP vs DMA_BUF 对比

### 两种缓冲区管理方式的区别

| 特性 | MMAP (v4l2_sample.c) | DMA_BUF (v4l2_dma_buf.c) |
|------|---------------------|-------------------------|
| **内存类型** | V4L2_MEMORY_MMAP | V4L2_MEMORY_DMABUF |
| **数据拷贝** | 内核态到用户态拷贝 | 零拷贝（直接访问） |
| **性能** | 良好 | 更优（特别是高分辨率） |
| **CPU占用** | 中等 | 更低 |
| **实现复杂度** | 简单 | 稍复杂 |
| **适用场景** | 通用场景 | 高性能/嵌入式场景 |
| **依赖** | 无 | 需要DMA_BUF支持 |

### 工作原理对比

#### MMAP方式工作流程：
1. 驱动程序在内核空间分配缓冲区
2. 使用`mmap()`将内核缓冲区映射到用户空间
3. 数据从摄像头→内核缓冲区→用户空间（通过映射）
4. 存在一次内核态到用户态的数据拷贝

#### DMA_BUF方式工作流程：
1. 应用程序创建DMA_BUF文件描述符
2. 将DMA_BUF传递给V4L2驱动
3. 数据从摄像头→DMA_BUF（零拷贝）
4. 应用程序直接访问DMA_BUF映射的内存
5. 完全避免数据拷贝，性能最优

### 如何选择？

**选择MMAP如果：**
- 快速原型开发
- 对性能要求不高
- 代码简洁性优先
- 兼容性要求高

**选择DMA_BUF如果：**
- 需要最高性能
- 处理高分辨率视频（1080p及以上）
- 在嵌入式系统上运行
- 需要降低CPU负载
- 实时性要求高

## 注意事项

1. **权限问题**：访问视频设备通常需要root权限或使用sudo
2. **摄像头支持**：确保摄像头支持MJPEG或YUYV格式
3. **存储空间**：采集的图像会占用磁盘空间
4. **性能优化**：如果遇到丢帧，可以尝试降低分辨率或帧率
5. **DMA_BUF支持**：确保系统内核支持DMA_BUF（Linux 3.8+）

## 技术细节

### v4l2_sample.c (MMAP) 工作流程：
1. 打开V4L2设备
2. 设置视频格式（MJPEG 1280x720）
3. 请求MMAP缓冲区并进行内存映射
4. 将缓冲区加入队列
5. 启动视频流
6. 循环采集10帧数据
7. 每帧保存为JPEG文件
8. 停止视频流并清理资源

### v4l2_dma_buf.c (DMA_BUF) 工作流程：
1. 打开V4L2设备
2. 设置视频格式（MJPEG 1280x720）
3. 创建DMA_BUF文件描述符
4. 调整DMA_BUF大小并映射到用户空间
5. 将DMA_BUF加入V4L2队列
6. 启动视频流
7. 循环采集10帧数据（零拷贝）
8. 每帧保存为JPEG文件
9. 停止视频流并清理资源（解除映射、关闭fd）

### v4l2_capture.cpp 工作流程：
1. 打开V4L2设备
2. 查询设备能力
3. 设置视频格式（优先MJPEG，失败则用YUYV）
4. 设置帧率
5. 请求缓冲区并进行内存映射
6. 启动视频流
7. 循环采集帧数据
8. 使用OpenCV解码JPEG数据
9. 写入视频文件
10. 清理资源并退出

## 学习建议

如果你是V4L2初学者，建议按以下顺序学习：

1. **先阅读 v4l2_sample.c**：理解V4L2的基本API调用流程
2. **编译运行 v4l2_sample**：验证摄像头是否正常工作
3. **再研究 v4l2_dma_buf.c**：学习DMA_BUF零拷贝技术
4. **最后研究 v4l2_capture.cpp**：学习如何实现更复杂的功能
5. **根据自己的需求修改代码**：基于示例开发自己的应用

## 常见问题

### Q: MMAP和DMA_BUF哪个更好？
A: 取决于你的需求。MMAP简单易用，适合大多数场景；DMA_BUF性能更优，适合高性能要求的场景。在嵌入式系统（如OrangePi）上，推荐使用DMA_BUF以获得更好的性能。

### Q: DMA_BUF在某些设备上不支持怎么办？
A: 如果系统不支持DMA_BUF，可以回退到MMAP方式。可以先尝试DMA_BUF，失败后再使用MMAP作为备选方案。
