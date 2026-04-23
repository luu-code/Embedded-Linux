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

**这是一个使用MMAP+EXPBUF方式的V4L2视频采集示例程序，支持预分配大尺寸输出DMA-BUF用于RGA/NPU/VPU零拷贝处理！**

- ✅ 纯C语言实现，无第三方依赖
- ✅ 使用MMAP请求V4L2缓冲区，通过VIDIOC_EXPBUF导出输入DMA-BUF fd
- ✅ **为每个缓冲区预分配大尺寸输出DMA-BUF** (默认4K RGBA, ~32MB/个)
- ✅ **支持RGA/NPU/VPU处理后数据变大** (YUV→RGBA放大,无需重新分配)
- ✅ 导出的DMA-BUF fd可用于零拷贝到DRM/RGA/NPU等硬件
- ✅ 在嵌入式系统中性能更优
- ✅ 减少CPU开销和内存带宽占用
- ✅ 持续循环采集视频帧，按Ctrl+C优雅退出
- ✅ 包含完善的错误处理和资源管理

**技术优势：**
- **预分配大缓冲区**: 每个V4L2缓冲区配对一个大尺寸输出DMA-BUF (可配置)
- **支持数据放大**: RGA可将1080p YUV放大到4K RGBA,无需动态分配
- **零拷贝支持**: 通过`VIDIOC_EXPBUF`导出合法的DMA-BUF fd
- **灵活性**: 既可使用MMAP访问数据，也可使用DMA-BUF fd进行零拷贝
- **高性能**: 特别适合与RGA/DRM/NPU/VPU等硬件协同工作
- **低延迟**: 减少数据处理延迟，适合实时应用
- **嵌入式友好**: 在OrangePi等嵌入式平台上表现更佳

**适用场景：**
- 需要零拷贝到其他硬件(RGA/DRM/NPU/VPU)的视频采集
- **RGA图像放大处理** (如1080p→4K,YUV→RGBA)
- **NPU多尺度推理** (不同分辨率输入)
- **VPU硬件编码** (可能需要更大缓冲区)
- 高性能视频处理流水线
- 嵌入式系统视频处理
- 实时视频流处理
- AI推理前的图像预处理
- 需要降低CPU负载的场景

**编译和运行：**
```bash
# 编译DMA_BUF版本
make dma_buf

# 运行（需要root权限或sudo）
sudo ./v4l2_dma_buf

# 按 Ctrl+C 停止采集

# 清理
make clean
```

**输出示例：**
```
成功打开设备 /dev/video0, fd=3
设置视频格式: 1280x720, MJPEG
成功请求 4 个MMAP缓冲区

预分配输出DMA-BUF配置:
  最大输出尺寸: 3840x2160
  输出格式: RGBA_8888
  单个缓冲区大小: 33177600 bytes (31.6 MB)
  总预分配大小: 132710400 bytes (126.5 MB)

缓冲区[0]:
  V4L2 MMAP: size=460800 bytes, mapped=0x7f8dadd000, offset=0
  V4L2输入DMA-BUF: fd=4
  预分配输出DMA-BUF... 成功
  输出DMA-BUF: fd=8, size=33177600 (31.6 MB), mapped=0x7f8c000000

缓冲区[1]:
  ...

所有缓冲区已加入队列
视频流已启动

开始持续采集图像... (按 Ctrl+C 停止)
提示:
  - 每 30 帧保存一次原始图像
  - 每个缓冲区都预分配了 31.6 MB 的输出DMA-BUF
  - 可用于RGA/NPU/VPU零拷贝处理(支持数据放大)
```

---

## 🔑 DMA-BUF 完整使用指南

### 一、DMA-BUF fd 的申请逻辑

#### 1.1 标准申请流程

```
┌─────────────────────────────────────────────────┐
│  1. VIDIOC_REQBUFS (V4L2_MEMORY_MMAP)          │
│     ↓ 驱动在内核分配缓冲区                        │
│  2. VIDIOC_QUERYBUF                             │
│     ↓ 查询缓冲区信息(offset, length)             │
│  3. mmap()                                      │
│     ↓ 将内核缓冲区映射到用户空间                  │
│  4. VIDIOC_EXPBUF                               │
│     ↓ 导出DMA-BUF文件描述符                      │
│  5. 获得 dma_fd (可用于零拷贝)                   │
└─────────────────────────────────────────────────┘
```

**关键代码示例：**

```c
// Step 1: 请求MMAP缓冲区
struct v4l2_requestbuffers req;
req.count = 4;
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_MMAP;
ioctl(fd, VIDIOC_REQBUFS, &req);

// Step 2-3: 查询并映射每个缓冲区
for (int i = 0; i < req.count; i++) {
    struct v4l2_buffer buf;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    
    ioctl(fd, VIDIOC_QUERYBUF, &buf);
    
    // MMAP映射
    void *start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, buf.m.offset);
    
    // Step 4: 导出DMA-BUF fd
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = i;
    expbuf.flags = O_CLOEXEC;
    
    ioctl(fd, VIDIOC_EXPBUF, &expbuf);
    int dma_fd = expbuf.fd;  // 这就是DMA-BUF文件描述符!
    
    printf("Buffer[%d]: dma_fd=%d\n", i, dma_fd);
}
```

#### 1.2 重要注意事项

⚠️ **不要自行创建DMA-BUF**: 
- ❌ 禁止使用 `memfd_create()` 或 `shm_open()` 创建fd
- ❌ 这些只是普通内存文件，V4L2驱动无法识别
- ✅ 必须通过 `VIDIOC_EXPBUF` 从V4L2驱动导出

⚠️ **DMA-BUF的生命周期**:
- DMA-BUF fd 在缓冲区整个生命周期内有效
- 只要不调用 `VIDIOC_REQBUFS(0)` 或关闭设备，fd就有效
- 程序退出前必须关闭所有dma_fd

---

### 二、RGA 零拷贝使用 DMA-BUF

#### 2.1 RGA 基本用法

Rockchip RGA (Raster Graphic Acceleration) 是硬件图像加速器，支持直接操作 DMA-BUF。

**安装RGA库：**
```bash
sudo apt-get install librga-dev
```

**代码示例：**

```c
#include <rga/RgaApi.h>

// 初始化RGA
rga_info_t src_info, dst_info;
memset(&src_info, 0, sizeof(src_info));
memset(&dst_info, 0, sizeof(dst_info));

// 设置源图像（来自V4L2的DMA-BUF）
int dma_fd = buffers[buf.index].dma_fd;
rga_set_dma_buf(&src_info, dma_fd, width, height, RK_FORMAT_YCbCr_420_SP);

// 设置目标图像（可以是另一个DMA-BUF或MMAP缓冲区）
void *dst_ptr = malloc(width * height * 3 / 2);
rga_set_virtual_addr(&dst_info, dst_ptr, width, height, RK_FORMAT_YCbCr_420_SP);

// 执行图像缩放/格式转换
imresize(src_info, dst_info);

// 清理
free(dst_ptr);
```

#### 2.2 RGA + DMA-BUF 完整示例

``c
// 假设已经采集到一帧，buf.index 是当前缓冲区索引
int capture_and_process(int v4l2_fd, struct buffer* buffers, 
                        struct v4l2_buffer* buf) {
    
    // 1. 从队列取出缓冲区
    ioctl(v4l2_fd, VIDIOC_DQBUF, buf);
    
    // 2. 获取DMA-BUF fd
    int src_dma_fd = buffers[buf->index].dma_fd;
    
    if (src_dma_fd < 0) {
        fprintf(stderr, "DMA-BUF not available\n");
        return -1;
    }
    
    // 3. 使用RGA进行图像处理（零拷贝！）
    rga_info_t src, dst;
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));
    
    // 源：V4L2捕获的DMA-BUF
    rga_set_dma_buf(&src, src_dma_fd, 1280, 720, RK_FORMAT_YCbCr_420_SP);
    
    // 目标：创建新的DMA-BUF用于存储处理结果
    int dst_dma_fd = create_dma_buf(640 * 480 * 3 / 2);  // 缩放后的大小
    rga_set_dma_buf(&dst, dst_dma_fd, 640, 480, RK_FORMAT_YCbCr_420_SP);
    
    // 执行缩放（硬件加速，零拷贝）
    imresize(src, dst);
    
    // 4. 现在 dst_dma_fd 包含缩放后的图像
    //    可以传递给NPU、DRM显示或其他硬件
    
    // 5. 重新入队V4L2缓冲区
    ioctl(v4l2_fd, VIDIOC_QBUF, buf);
    
    // 6. 如果不再需要dst_dma_fd，记得关闭
    close(dst_dma_fd);
    
    return 0;
}
```

---

### 三、NPU 零拷贝使用 DMA-BUF

#### 3.1 Rockchip NPU (RKNN) 用法

Rockchip NPU 支持直接读取 DMA-BUF，避免CPU拷贝。

**安装RKNN工具包：**
```bash
# 从Rockchip官网下载RKNN Toolkit2
# 安装Python API和C API
```

**代码示例：**

```c
#include <rknn_api.h>

// 加载模型
rknn_context ctx;
rknn_init(&ctx, model_data, model_size, 0);

// 准备输入tensor
rknn_input inputs[1];
memset(&inputs, 0, sizeof(inputs));

// 关键：使用DMA-BUF fd作为输入（零拷贝！）
inputs[0].index = 0;
inputs[0].type = RKNN_TENSOR_UINT8;
inputs[0].size = width * height * 3;
inputs[0].fmt = RKNN_TENSOR_NHWC;
inputs[0].buf = NULL;  // 不使用虚拟地址
inputs[0].fd = dma_fd; // 使用DMA-BUF fd！

rknn_inputs_set(ctx, 1, inputs);

// 执行推理（NPU直接读取DMA-BUF，零拷贝）
rknn_run(ctx, NULL);

// 获取输出
rknn_output outputs[1];
memset(&outputs, 0, sizeof(outputs));
outputs[0].want_float = 0;
rknn_outputs_get(ctx, 1, outputs, NULL);

// 处理结果...

// 释放输出
rknn_outputs_release(ctx, 1, outputs);
```

#### 3.2 NPU + RGA 组合 pipeline

典型的AI视觉处理流程：

```
V4L2摄像头 → RGA预处理 → NPU推理 → 结果处理
   (DMA-BUF)   (零拷贝)    (零拷贝)
```

```c
void ai_vision_pipeline(int v4l2_fd, struct buffer* buffers,
                        rknn_context npu_ctx) {
    
    struct v4l2_buffer buf;
    
    while (1) {
        // 1. 等待新帧
        poll(...);
        ioctl(v4l2_fd, VIDIOC_DQBUF, &buf);
        
        int camera_dma_fd = buffers[buf.index].dma_fd;
        
        // 2. RGA预处理（缩放、格式转换、裁剪等）
        int preprocessed_fd = rga_preprocess(camera_dma_fd, 
                                             1920, 1080,  // 输入
                                             640, 640);   // 输出
        
        // 3. NPU推理（零拷贝读取preprocessed_fd）
        rknn_input input;
        input.fd = preprocessed_fd;
        input.size = 640 * 640 * 3;
        rknn_inputs_set(npu_ctx, 1, &input);
        rknn_run(npu_ctx, NULL);
        
        // 4. 获取推理结果
        rknn_output output;
        rknn_outputs_get(npu_ctx, 1, &output, NULL);
        process_result(output.buf);
        rknn_outputs_release(npu_ctx, 1, &output);
        
        // 5. 清理临时DMA-BUF
        close(preprocessed_fd);
        
        // 6. 重新入队
        ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
    }
}
```

---

### 四、DRM 零拷贝显示 DMA-BUF

#### 4.1 DRM Prime Buffer 导入

```c
#include <drm/drm.h>
#include <xf86drm.h>

// 将DMA-BUF fd导入DRM
int import_dmabuf_to_drm(int drm_fd, int dma_buf_fd) {
    struct drm_prime_handle prime_handle;
    memset(&prime_handle, 0, sizeof(prime_handle));
    
    prime_handle.fd = dma_buf_fd;
    prime_handle.flags = DRM_CLOEXEC | DRM_RDWR;
    
    // 导入DMA-BUF到DRM，获得GEM handle
    ioctl(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle);
    
    return prime_handle.handle;  // GEM handle
}

// 创建framebuffer并显示
void display_frame(int drm_fd, int dma_buf_fd, int width, int height) {
    uint32_t gem_handle = import_dmabuf_to_drm(drm_fd, dma_buf_fd);
    
    // 创建framebuffer
    uint32_t fb_id;
    uint32_t pitches[1] = { width * 4 };  // RGBA
    uint32_t handles[1] = { gem_handle };
    uint32_t offsets[1] = { 0 };
    
    drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_ARGB8888,
                  handles, pitches, offsets, &fb_id, 0);
    
    // 显示到plane
    drmModeSetPlane(drm_fd, plane_id, crtc_id, fb_id, 0,
                    0, 0, width, height,  // 显示位置
                    0, 0, width << 16, height << 16);  // 源位置
    
    // 清理
    drmModeRmFB(drm_fd, fb_id);
    drmCloseBufferHandle(drm_fd, gem_handle);
}
```

---

### 五、DMA-BUF 的正确释放

#### 5.1 释放原则

✅ **必须释放的资源：**
1. 所有通过 `VIDIOC_EXPBUF` 获得的 dma_fd
2. 所有通过 `mmap()` 映射的内存
3. V4L2 设备文件描述符

❌ **不要重复释放：**
- 不要在使用中关闭 dma_fd
- 不要在重新入队前关闭 dma_fd
- dma_fd 应该在程序退出时统一关闭

#### 5.2 标准释放流程

``c
cleanup:
    // Step 1: 停止视频流
    if (fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
    }

    // Step 2: 释放所有缓冲区资源
    if (buffers != NULL) {
        for (unsigned int i = 0; i < num_buffers; ++i) {
            // 2.1 解除MMAP映射
            if (buffers[i].start != NULL && buffers[i].start != MAP_FAILED) {
                munmap(buffers[i].start, buffers[i].length);
                buffers[i].start = NULL;
            }
            
            // 2.2 关闭DMA-BUF fd
            if (buffers[i].dma_fd >= 0) {
                close(buffers[i].dma_fd);
                printf("Closed DMA-BUF fd=%d for buffer[%u]\n", 
                       buffers[i].dma_fd, i);
                buffers[i].dma_fd = -1;
            }
        }
        free(buffers);
    }

    // Step 3: 关闭V4L2设备
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    
    printf("All resources released successfully\n");
```

#### 5.3 常见错误及避免

❌ **错误1：在使用中关闭dma_fd**
```c
// 错误示例
int dma_fd = buffers[buf.index].dma_fd;
close(dma_fd);  // ❌ 这会导致后续使用该缓冲区时出错！
ioctl(fd, VIDIOC_QBUF, &buf);  // 失败！
```

✅ **正确做法：**
```c
// 只在程序退出时关闭
// 使用过程中保持fd打开
```

❌ **错误2：忘记关闭dma_fd导致资源泄漏**
```c
// 错误示例
for (int i = 0; i < 100; i++) {
    int new_fd = create_some_dma_buf();
    use_fd(new_fd);
    // 忘记 close(new_fd)  // ❌ 资源泄漏！
}
```

✅ **正确做法：**
```c
for (int i = 0; i < 100; i++) {
    int new_fd = create_some_dma_buf();
    use_fd(new_fd);
    close(new_fd);  // ✅ 及时释放
}
```

❌ **错误3：重复关闭同一个fd**
```c
close(dma_fd);
close(dma_fd);  // ❌ 未定义行为！
```

✅ **正确做法：**
```c
if (dma_fd >= 0) {
    close(dma_fd);
    dma_fd = -1;  // 标记为已关闭
}
```

---

### 六、调试技巧

#### 6.1 查看系统DMA-BUF使用情况

```bash
# 查看所有DMA-BUF缓冲区信息
cat /sys/kernel/debug/dma_buf/bufinfo

# 查看DMA-BUF统计信息
cat /sys/kernel/debug/dma_buf/stat

# 监控DMA-BUF数量变化（检测泄漏）
watch -n 1 'cat /sys/kernel/debug/dma_buf/bufinfo | grep "^D" | wc -l'
```

#### 6.2 验证零拷贝是否生效

```c
// 方法1：测量处理时间
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);

// 使用DMA-BUF进行处理
process_with_dmabuf(dma_fd);

clock_gettime(CLOCK_MONOTONIC, &end);
double elapsed = (end.tv_sec - start.tv_sec) + 
                 (end.tv_nsec - start.tv_nsec) / 1e9;
printf("Processing time: %.3f ms\n", elapsed * 1000);

// 方法2：监控CPU使用率
// 零拷贝应该显著降低CPU使用率
top -p $(pgrep your_program)
```

#### 6.3 常见问题排查

**问题1：VIDIOC_EXPBUF 失败**
```
Error: 导出DMA-BUF失败: Invalid argument
```
解决：
- 检查内核是否支持 EXPBUF：`zcat /proc/config.gz | grep VIDEOBUF2`
- 确保使用了 `V4L2_MEMORY_MMAP` 而不是 `V4L2_MEMORY_DMABUF`
- 检查驱动是否实现了 `vidioc_expbuf` 回调

**问题2：RGA/NPU 无法读取 DMA-BUF**
```
Error: Failed to import DMA-BUF
```
解决：
- 确认DMA-BUF是由V4L2驱动导出的（不是memfd/shm）
- 检查格式是否匹配（YUV420SP, RGB等）
- 验证DMA-BUF大小是否正确

**问题3：DMA-BUF fd 无效**
```
Error: Bad file descriptor
```
解决：
- 检查是否在关闭后仍在使用
- 确认没有重复关闭
- 验证fd值是否 >= 0

---

### 七、完整示例项目结构

```
v4l2_test/
├── v4l2_sample.c          # 基础MMAP示例
├── v4l2_dma_buf.c         # MMAP+EXPBUF示例（本文件）
├── v4l2_capture.cpp       # OpenCV完整采集程序
├── README.md              # 本文档
├── Makefile               # 编译配置
└── examples/              # 更多示例（可选）
    ├── rga_example.c      # RGA零拷贝示例
    ├── npu_example.c      # NPU零拷贝示例
    └── drm_display.c      # DRM显示示例
```

---

### 八、性能对比

| 方案 | CPU占用 | 延迟 | 适用场景 |
|------|---------|------|----------|
| MMAP拷贝 | ~30-50% | 中等 | 通用场景 |
| DMA-BUF + RGA | ~10-20% | 低 | 图像预处理 |
| DMA-BUF + NPU | ~5-15% | 最低 | AI推理 |
| DMA-BUF + DRM | ~5-10% | 最低 | 直接显示 |

---

### 九、参考资源

- [Linux DMA-BUF Documentation](https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html)
- [V4L2 DMA-BUF Import](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dmabuf.html)
- [Rockchip RGA Documentation](https://wiki.radxa.com/Rockchip/rga)
- [RKNN Toolkit2](https://github.com/airockchip/rknn-toolkit2)
- [DRM Prime Buffer Sharing](https://dri.freedesktop.org/docs/drm/gpu/drm-mm.html#prime-buffer-sharing)

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

```
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

**这是一个使用MMAP+EXPBUF方式的V4L2视频采集示例程序，支持预分配大尺寸输出DMA-BUF用于RGA/NPU/VPU零拷贝处理！**

- ✅ 纯C语言实现，无第三方依赖
- ✅ 使用MMAP请求V4L2缓冲区，通过VIDIOC_EXPBUF导出输入DMA-BUF fd
- ✅ **为每个缓冲区预分配大尺寸输出DMA-BUF** (默认4K RGBA, ~32MB/个)
- ✅ **支持RGA/NPU/VPU处理后数据变大** (YUV→RGBA放大,无需重新分配)
- ✅ 导出的DMA-BUF fd可用于零拷贝到DRM/RGA/NPU等硬件
- ✅ 在嵌入式系统中性能更优
- ✅ 减少CPU开销和内存带宽占用
- ✅ 持续循环采集视频帧，按Ctrl+C优雅退出
- ✅ 包含完善的错误处理和资源管理

**技术优势：**
- **预分配大缓冲区**: 每个V4L2缓冲区配对一个大尺寸输出DMA-BUF (可配置)
- **支持数据放大**: RGA可将1080p YUV放大到4K RGBA,无需动态分配
- **零拷贝支持**: 通过`VIDIOC_EXPBUF`导出合法的DMA-BUF fd
- **灵活性**: 既可使用MMAP访问数据，也可使用DMA-BUF fd进行零拷贝
- **高性能**: 特别适合与RGA/DRM/NPU/VPU等硬件协同工作
- **低延迟**: 减少数据处理延迟，适合实时应用
- **嵌入式友好**: 在OrangePi等嵌入式平台上表现更佳

**适用场景：**
- 需要零拷贝到其他硬件(RGA/DRM/NPU/VPU)的视频采集
- **RGA图像放大处理** (如1080p→4K,YUV→RGBA)
- **NPU多尺度推理** (不同分辨率输入)
- **VPU硬件编码** (可能需要更大缓冲区)
- 高性能视频处理流水线
- 嵌入式系统视频处理
- 实时视频流处理
- AI推理前的图像预处理
- 需要降低CPU负载的场景

**编译和运行：**
```bash
# 编译DMA_BUF版本
make dma_buf

# 运行（需要root权限或sudo）
sudo ./v4l2_dma_buf

# 按 Ctrl+C 停止采集

# 清理
make clean
```

**输出示例：**
```
成功打开设备 /dev/video0, fd=3
设置视频格式: 1280x720, MJPEG
成功请求 4 个MMAP缓冲区

预分配输出DMA-BUF配置:
  最大输出尺寸: 3840x2160
  输出格式: RGBA_8888
  单个缓冲区大小: 33177600 bytes (31.6 MB)
  总预分配大小: 132710400 bytes (126.5 MB)

缓冲区[0]:
  V4L2 MMAP: size=460800 bytes, mapped=0x7f8dadd000, offset=0
  V4L2输入DMA-BUF: fd=4
  预分配输出DMA-BUF... 成功
  输出DMA-BUF: fd=8, size=33177600 (31.6 MB), mapped=0x7f8c000000

缓冲区[1]:
  ...

所有缓冲区已加入队列
视频流已启动

开始持续采集图像... (按 Ctrl+C 停止)
提示:
  - 每 30 帧保存一次原始图像
  - 每个缓冲区都预分配了 31.6 MB 的输出DMA-BUF
  - 可用于RGA/NPU/VPU零拷贝处理(支持数据放大)
```

---

## 🔑 DMA-BUF 完整使用指南

### 一、DMA-BUF fd 的申请逻辑

#### 1.1 标准申请流程

```
┌─────────────────────────────────────────────────┐
│  1. VIDIOC_REQBUFS (V4L2_MEMORY_MMAP)          │
│     ↓ 驱动在内核分配缓冲区                        │
│  2. VIDIOC_QUERYBUF                             │
│     ↓ 查询缓冲区信息(offset, length)             │
│  3. mmap()                                      │
│     ↓ 将内核缓冲区映射到用户空间                  │
│  4. VIDIOC_EXPBUF                               │
│     ↓ 导出DMA-BUF文件描述符                      │
│  5. 获得 dma_fd (可用于零拷贝)                   │
└─────────────────────────────────────────────────┘
```

**关键代码示例：**

```c
// Step 1: 请求MMAP缓冲区
struct v4l2_requestbuffers req;
req.count = 4;
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_MMAP;
ioctl(fd, VIDIOC_REQBUFS, &req);

// Step 2-3: 查询并映射每个缓冲区
for (int i = 0; i < req.count; i++) {
    struct v4l2_buffer buf;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    
    ioctl(fd, VIDIOC_QUERYBUF, &buf);
    
    // MMAP映射
    void *start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, buf.m.offset);
    
    // Step 4: 导出DMA-BUF fd
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = i;
    expbuf.flags = O_CLOEXEC;
    
    ioctl(fd, VIDIOC_EXPBUF, &expbuf);
    int dma_fd = expbuf.fd;  // 这就是DMA-BUF文件描述符!
    
    printf("Buffer[%d]: dma_fd=%d\n", i, dma_fd);
}
```

#### 1.2 重要注意事项

⚠️ **不要自行创建DMA-BUF**: 
- ❌ 禁止使用 `memfd_create()` 或 `shm_open()` 创建fd
- ❌ 这些只是普通内存文件，V4L2驱动无法识别
- ✅ 必须通过 `VIDIOC_EXPBUF` 从V4L2驱动导出

⚠️ **DMA-BUF的生命周期**:
- DMA-BUF fd 在缓冲区整个生命周期内有效
- 只要不调用 `VIDIOC_REQBUFS(0)` 或关闭设备，fd就有效
- 程序退出前必须关闭所有dma_fd

---

### 二、RGA 零拷贝使用 DMA-BUF

#### 2.1 RGA 基本用法

Rockchip RGA (Raster Graphic Acceleration) 是硬件图像加速器，支持直接操作 DMA-BUF。

**安装RGA库：**
```bash
sudo apt-get install librga-dev
```

**代码示例：**

```c
#include <rga/RgaApi.h>

// 初始化RGA
rga_info_t src_info, dst_info;
memset(&src_info, 0, sizeof(src_info));
memset(&dst_info, 0, sizeof(dst_info));

// 设置源图像（来自V4L2的DMA-BUF）
int dma_fd = buffers[buf.index].dma_fd;
rga_set_dma_buf(&src_info, dma_fd, width, height, RK_FORMAT_YCbCr_420_SP);

// 设置目标图像（可以是另一个DMA-BUF或MMAP缓冲区）
void *dst_ptr = malloc(width * height * 3 / 2);
rga_set_virtual_addr(&dst_info, dst_ptr, width, height, RK_FORMAT_YCbCr_420_SP);

// 执行图像缩放/格式转换
imresize(src_info, dst_info);

// 清理
free(dst_ptr);
```

#### 2.2 RGA + DMA-BUF 完整示例

``c
// 假设已经采集到一帧，buf.index 是当前缓冲区索引
int capture_and_process(int v4l2_fd, struct buffer* buffers, 
                        struct v4l2_buffer* buf) {
    
    // 1. 从队列取出缓冲区
    ioctl(v4l2_fd, VIDIOC_DQBUF, buf);
    
    // 2. 获取DMA-BUF fd
    int src_dma_fd = buffers[buf->index].dma_fd;
    
    if (src_dma_fd < 0) {
        fprintf(stderr, "DMA-BUF not available\n");
        return -1;
    }
    
    // 3. 使用RGA进行图像处理（零拷贝！）
    rga_info_t src, dst;
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));
    
    // 源：V4L2捕获的DMA-BUF
    rga_set_dma_buf(&src, src_dma_fd, 1280, 720, RK_FORMAT_YCbCr_420_SP);
    
    // 目标：创建新的DMA-BUF用于存储处理结果
    int dst_dma_fd = create_dma_buf(640 * 480 * 3 / 2);  // 缩放后的大小
    rga_set_dma_buf(&dst, dst_dma_fd, 640, 480, RK_FORMAT_YCbCr_420_SP);
    
    // 执行缩放（硬件加速，零拷贝）
    imresize(src, dst);
    
    // 4. 现在 dst_dma_fd 包含缩放后的图像
    //    可以传递给NPU、DRM显示或其他硬件
    
    // 5. 重新入队V4L2缓冲区
    ioctl(v4l2_fd, VIDIOC_QBUF, buf);
    
    // 6. 如果不再需要dst_dma_fd，记得关闭
    close(dst_dma_fd);
    
    return 0;
}
```

---

### 三、NPU 零拷贝使用 DMA-BUF

#### 3.1 Rockchip NPU (RKNN) 用法

Rockchip NPU 支持直接读取 DMA-BUF，避免CPU拷贝。

**安装RKNN工具包：**
```bash
# 从Rockchip官网下载RKNN Toolkit2
# 安装Python API和C API
```

**代码示例：**

```c
#include <rknn_api.h>

// 加载模型
rknn_context ctx;
rknn_init(&ctx, model_data, model_size, 0);

// 准备输入tensor
rknn_input inputs[1];
memset(&inputs, 0, sizeof(inputs));

// 关键：使用DMA-BUF fd作为输入（零拷贝！）
inputs[0].index = 0;
inputs[0].type = RKNN_TENSOR_UINT8;
inputs[0].size = width * height * 3;
inputs[0].fmt = RKNN_TENSOR_NHWC;
inputs[0].buf = NULL;  // 不使用虚拟地址
inputs[0].fd = dma_fd; // 使用DMA-BUF fd！

rknn_inputs_set(ctx, 1, inputs);

// 执行推理（NPU直接读取DMA-BUF，零拷贝）
rknn_run(ctx, NULL);

// 获取输出
rknn_output outputs[1];
memset(&outputs, 0, sizeof(outputs));
outputs[0].want_float = 0;
rknn_outputs_get(ctx, 1, outputs, NULL);

// 处理结果...

// 释放输出
rknn_outputs_release(ctx, 1, outputs);
```

#### 3.2 NPU + RGA 组合 pipeline

典型的AI视觉处理流程：

```
V4L2摄像头 → RGA预处理 → NPU推理 → 结果处理
   (DMA-BUF)   (零拷贝)    (零拷贝)
```

```c
void ai_vision_pipeline(int v4l2_fd, struct buffer* buffers,
                        rknn_context npu_ctx) {
    
    struct v4l2_buffer buf;
    
    while (1) {
        // 1. 等待新帧
        poll(...);
        ioctl(v4l2_fd, VIDIOC_DQBUF, &buf);
        
        int camera_dma_fd = buffers[buf.index].dma_fd;
        
        // 2. RGA预处理（缩放、格式转换、裁剪等）
        int preprocessed_fd = rga_preprocess(camera_dma_fd, 
                                             1920, 1080,  // 输入
                                             640, 640);   // 输出
        
        // 3. NPU推理（零拷贝读取preprocessed_fd）
        rknn_input input;
        input.fd = preprocessed_fd;
        input.size = 640 * 640 * 3;
        rknn_inputs_set(npu_ctx, 1, &input);
        rknn_run(npu_ctx, NULL);
        
        // 4. 获取推理结果
        rknn_output output;
        rknn_outputs_get(npu_ctx, 1, &output, NULL);
        process_result(output.buf);
        rknn_outputs_release(npu_ctx, 1, &output);
        
        // 5. 清理临时DMA-BUF
        close(preprocessed_fd);
        
        // 6. 重新入队
        ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
    }
}
```

---

### 四、DRM 零拷贝显示 DMA-BUF

#### 4.1 DRM Prime Buffer 导入

``c
#include <drm/drm.h>
#include <xf86drm.h>

// 将DMA-BUF fd导入DRM
int import_dmabuf_to_drm(int drm_fd, int dma_buf_fd) {
    struct drm_prime_handle prime_handle;
    memset(&prime_handle, 0, sizeof(prime_handle));
    
    prime_handle.fd = dma_buf_fd;
    prime_handle.flags = DRM_CLOEXEC | DRM_RDWR;
    
    // 导入DMA-BUF到DRM，获得GEM handle
    ioctl(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle);
    
    return prime_handle.handle;  // GEM handle
}

// 创建framebuffer并显示
void display_frame(int drm_fd, int dma_buf_fd, int width, int height) {
    uint32_t gem_handle = import_dmabuf_to_drm(drm_fd, dma_buf_fd);
    
    // 创建framebuffer
    uint32_t fb_id;
    uint32_t pitches[1] = { width * 4 };  // RGBA
    uint32_t handles[1] = { gem_handle };
    uint32_t offsets[1] = { 0 };
    
    drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_ARGB8888,
                  handles, pitches, offsets, &fb_id, 0);
    
    // 显示到plane
    drmModeSetPlane(drm_fd, plane_id, crtc_id, fb_id, 0,
                    0, 0, width, height,  // 显示位置
                    0, 0, width << 16, height << 16);  // 源位置
    
    // 清理
    drmModeRmFB(drm_fd, fb_id);
    drmCloseBufferHandle(drm_fd, gem_handle);
}
```

---

### 五、DMA-BUF 的正确释放

#### 5.1 释放原则

✅ **必须释放的资源：**
1. 所有通过 `VIDIOC_EXPBUF` 获得的 dma_fd
2. 所有通过 `mmap()` 映射的内存
3. V4L2 设备文件描述符

❌ **不要重复释放：**
- 不要在使用中关闭 dma_fd
- 不要在重新入队前关闭 dma_fd
- dma_fd 应该在程序退出时统一关闭

#### 5.2 标准释放流程

``c
cleanup:
    // Step 1: 停止视频流
    if (fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
    }

    // Step 2: 释放所有缓冲区资源
    if (buffers != NULL) {
        for (unsigned int i = 0; i < num_buffers; ++i) {
            // 2.1 解除MMAP映射
            if (buffers[i].start != NULL && buffers[i].start != MAP_FAILED) {
                munmap(buffers[i].start, buffers[i].length);
                buffers[i].start = NULL;
            }
            
            // 2.2 关闭DMA-BUF fd
            if (buffers[i].dma_fd >= 0) {
                close(buffers[i].dma_fd);
                printf("Closed DMA-BUF fd=%d for buffer[%u]\n", 
                       buffers[i].dma_fd, i);
                buffers[i].dma_fd = -1;
            }
        }
        free(buffers);
    }

    // Step 3: 关闭V4L2设备
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    
    printf("All resources released successfully\n");
```

#### 5.3 常见错误及避免

❌ **错误1：在使用中关闭dma_fd**
```c
// 错误示例
int dma_fd = buffers[buf.index].dma_fd;
close(dma_fd);  // ❌ 这会导致后续使用该缓冲区时出错！
ioctl(fd, VIDIOC_QBUF, &buf);  // 失败！
```

✅ **正确做法：**
```c
// 只在程序退出时关闭
// 使用过程中保持fd打开
```

❌ **错误2：忘记关闭dma_fd导致资源泄漏**
```c
// 错误示例
for (int i = 0; i < 100; i++) {
    int new_fd = create_some_dma_buf();
    use_fd(new_fd);
    // 忘记 close(new_fd)  // ❌ 资源泄漏！
}
```

✅ **正确做法：**
```c
for (int i = 0; i < 100; i++) {
    int new_fd = create_some_dma_buf();
    use_fd(new_fd);
    close(new_fd);  // ✅ 及时释放
}
```

❌ **错误3：重复关闭同一个fd**
```c
close(dma_fd);
close(dma_fd);  // ❌ 未定义行为！
```

✅ **正确做法：**
```c
if (dma_fd >= 0) {
    close(dma_fd);
    dma_fd = -1;  // 标记为已关闭
}
```

---

### 六、调试技巧

#### 6.1 查看系统DMA-BUF使用情况

```bash
# 查看所有DMA-BUF缓冲区信息
cat /sys/kernel/debug/dma_buf/bufinfo

# 查看DMA-BUF统计信息
cat /sys/kernel/debug/dma_buf/stat

# 监控DMA-BUF数量变化（检测泄漏）
watch -n 1 'cat /sys/kernel/debug/dma_buf/bufinfo | grep "^D" | wc -l'
```

#### 6.2 验证零拷贝是否生效

```c
// 方法1：测量处理时间
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);

// 使用DMA-BUF进行处理
process_with_dmabuf(dma_fd);

clock_gettime(CLOCK_MONOTONIC, &end);
double elapsed = (end.tv_sec - start.tv_sec) + 
                 (end.tv_nsec - start.tv_nsec) / 1e9;
printf("Processing time: %.3f ms\n", elapsed * 1000);

// 方法2：监控CPU使用率
// 零拷贝应该显著降低CPU使用率
top -p $(pgrep your_program)
```

#### 6.3 常见问题排查

**问题1：VIDIOC_EXPBUF 失败**
```
Error: 导出DMA-BUF失败: Invalid argument
```
解决：
- 检查内核是否支持 EXPBUF：`zcat /proc/config.gz | grep VIDEOBUF2`
- 确保使用了 `V4L2_MEMORY_MMAP` 而不是 `V4L2_MEMORY_DMABUF`
- 检查驱动是否实现了 `vidioc_expbuf` 回调

**问题2：RGA/NPU 无法读取 DMA-BUF**
```
Error: Failed to import DMA-BUF
```
解决：
- 确认DMA-BUF是由V4L2驱动导出的（不是memfd/shm）
- 检查格式是否匹配（YUV420SP, RGB等）
- 验证DMA-BUF大小是否正确

**问题3：DMA-BUF fd 无效**
```
Error: Bad file descriptor
```
解决：
- 检查是否在关闭后仍在使用
- 确认没有重复关闭
- 验证fd值是否 >= 0

---

### 七、完整示例项目结构

```
v4l2_test/
├── v4l2_sample.c          # 基础MMAP示例
├── v4l2_dma_buf.c         # MMAP+EXPBUF示例（本文件）
├── v4l2_capture.cpp       # OpenCV完整采集程序
├── README.md              # 本文档
├── Makefile               # 编译配置
└── examples/              # 更多示例（可选）
    ├── rga_example.c      # RGA零拷贝示例
    ├── npu_example.c      # NPU零拷贝示例
    └── drm_display.c      # DRM显示示例
```

---

### 八、性能对比

| 方案 | CPU占用 | 延迟 | 适用场景 |
|------|---------|------|----------|
| MMAP拷贝 | ~30-50% | 中等 | 通用场景 |
| DMA-BUF + RGA | ~10-20% | 低 | 图像预处理 |
| DMA-BUF + NPU | ~5-15% | 最低 | AI推理 |
| DMA-BUF + DRM | ~5-10% | 最低 | 直接显示 |

---

### 九、参考资源

- [Linux DMA-BUF Documentation](https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html)
- [V4L2 DMA-BUF Import](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dmabuf.html)
- [Rockchip RGA Documentation](https://wiki.radxa.com/Rockchip/rga)
- [RKNN Toolkit2](https://github.com/airockchip/rknn-toolkit2)
- [DRM Prime Buffer Sharing](https://dri.freedesktop.org/docs/drm/gpu/drm-mm.html#prime-buffer-sharing)

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
