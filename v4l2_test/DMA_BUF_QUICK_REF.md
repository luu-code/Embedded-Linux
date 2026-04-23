# DMA-BUF 快速参考手册

## 🚀 快速开始

### 1. 编译和运行
```bash
make dma_buf
sudo ./v4l2_dma_buf
# 按 Ctrl+C 退出
```

### 2. 获取 DMA-BUF fd
在代码中访问:
```c
int dma_fd = buffers[buf.index].dma_fd;
```

---

## 📋 DMA-BUF 生命周期

```
申请 → 使用 → 释放
  ↓       ↓      ↓
EXPBUF  RGA/NPU  close()
        DRM
```

### ✅ 正确做法
```c
// 1. 程序启动时导出（一次性）
ioctl(fd, VIDIOC_EXPBUF, &expbuf);
int dma_fd = expbuf.fd;

// 2. 使用过程中保持打开
while (running) {
    use_dma_buf(dma_fd);  // RGA/NPU/DRM
}

// 3. 程序退出时关闭
close(dma_fd);
```

### ❌ 错误做法
```c
// 错误1: 使用中关闭
use_dma_buf(dma_fd);
close(dma_fd);  // ❌ 后续使用会失败！
use_dma_buf(dma_fd);  // Bad file descriptor

// 错误2: 重复关闭
close(dma_fd);
close(dma_fd);  // ❌ 未定义行为！

// 错误3: 忘记关闭
for (int i=0; i<100; i++) {
    int fd = create_dmabuf();
    use(fd);
    // 忘记 close(fd)  // ❌ 资源泄漏！
}
```

---

## 🔧 RGA 零拷贝示例

```c
#include <rga/RgaApi.h>

// 从V4L2获取DMA-BUF fd
int src_fd = buffers[buf.index].dma_fd;

// 设置RGA源（零拷贝！）
rga_info_t src, dst;
rga_set_dma_buf(&src, src_fd, width, height, RK_FORMAT_YCbCr_420_SP);

// 设置目标
void* dst_ptr = malloc(dst_size);
rga_set_virtual_addr(&dst, dst_ptr, dst_w, dst_h, format);

// 执行硬件处理（零拷贝）
imresize(src, dst);

// 清理
free(dst_ptr);
// 注意：不要关闭 src_fd，它由V4L2管理
```

---

## 🧠 NPU 零拷贝示例

```c
#include <rknn_api.h>

// 准备输入（使用DMA-BUF fd）
rknn_input input;
input.index = 0;
input.type = RKNN_TENSOR_UINT8;
input.size = width * height * 3;
input.fmt = RKNN_TENSOR_NHWC;
input.buf = NULL;       // 不使用虚拟地址
input.fd = dma_fd;      // 使用DMA-BUF fd！（关键）

// 设置输入并推理
rknn_inputs_set(ctx, 1, &input);
rknn_run(ctx, NULL);  // NPU直接读取DMA-BUF

// 获取结果
rknn_output output;
rknn_outputs_get(ctx, 1, &output, NULL);
// ... 处理结果 ...
rknn_outputs_release(ctx, 1, &output);
```

---

## 🖥️ DRM 显示示例

```c
#include <xf86drm.h>
#include <xf86drmMode.h>

// 导入DMA-BUF到DRM
struct drm_prime_handle prime;
prime.fd = dma_fd;
prime.flags = DRM_CLOEXEC | DRM_RDWR;
ioctl(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime);

// 创建framebuffer
uint32_t fb_id;
uint32_t handles[] = { prime.handle };
uint32_t pitches[] = { width * 4 };
uint32_t offsets[] = { 0 };
drmModeAddFB2(drm_fd, w, h, DRM_FORMAT_ARGB8888,
              handles, pitches, offsets, &fb_id, 0);

// 显示
drmModeSetPlane(drm_fd, plane_id, crtc_id, fb_id, ...);

// 清理
drmModeRmFB(drm_fd, fb_id);
drmCloseBufferHandle(drm_fd, prime.handle);
```

---

## 🐛 调试技巧

### 查看DMA-BUF使用情况
```bash
# 查看所有DMA-BUF
cat /sys/kernel/debug/dma_buf/bufinfo

# 监控数量变化（检测泄漏）
watch -n 1 'cat /sys/kernel/debug/dma_buf/bufinfo | grep "^D" | wc -l'
```

### 验证零拷贝生效
```c
// 方法1: 测量时间
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);
process_with_dmabuf(fd);
clock_gettime(CLOCK_MONOTONIC, &end);
printf("Time: %.3f ms\n", elapsed_ms);

// 方法2: 监控CPU
top -p $(pgrep your_program)
// 零拷贝应该显著降低CPU使用率
```

---

## ⚠️ 常见问题

### Q1: VIDIOC_EXPBUF 失败
**错误**: `Invalid argument`

**解决**:
- 确保使用 `V4L2_MEMORY_MMAP` 请求缓冲区
- 检查内核支持: `zcat /proc/config.gz | grep VIDEOBUF2`
- 确认驱动实现了 expbuf 回调

### Q2: RGA/NPU 无法读取
**错误**: `Failed to import DMA-BUF`

**解决**:
- 确认fd是从 `VIDIOC_EXPBUF` 获得的（不是memfd/shm）
- 检查格式匹配（YUV420SP, RGB等）
- 验证DMA-BUF大小正确

### Q3: fd 无效
**错误**: `Bad file descriptor`

**解决**:
- 检查是否在关闭后仍在使用
- 确认没有重复关闭
- 验证 fd >= 0

---

## 📊 性能对比

| 方案 | CPU占用 | 延迟 | 适用场景 |
|------|---------|------|----------|
| MMAP拷贝 | 30-50% | 中 | 通用 |
| DMA-BUF + RGA | 10-20% | 低 | 预处理 |
| DMA-BUF + NPU | 5-15% | 最低 | AI推理 |
| DMA-BUF + DRM | 5-10% | 最低 | 显示 |

---

## 🔗 相关文档

- [完整README](README.md) - 详细使用指南
- [示例代码](dmabuf_usage_example.c) - RGA/NPU/DRM示例
- [Linux DMA-BUF文档](https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html)
- [Rockchip RGA](https://wiki.radxa.com/Rockchip/rga)
- [RKNN Toolkit2](https://github.com/airockchip/rknn-toolkit2)

---

**提示**: 遇到问题时，先检查 `/sys/kernel/debug/dma_buf/bufinfo` 确认DMA-BUF状态！
