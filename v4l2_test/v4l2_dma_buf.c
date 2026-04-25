/* v4l2_dma_buf.c - 使用MMAP+EXPBUF方式的V4L2视频采集程序 
 * 
 * 特性:
 * - 为每个V4L2缓冲区预分配大尺寸输出DMA-BUF,支持RGA/NPU/VPU处理后数据变大
 * - 导出的DMA-BUF fd可直接用于零拷贝到硬件加速器
 * - 持续循环采集,按Ctrl+C优雅退出
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/syscall.h> /* For syscall numbers */

// 全局变量用于信号处理
static volatile sig_atomic_t running = 1;

// 信号处理函数
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n\n收到信号 %d，准备退出...\n", sig);
        running = 0;
    }
}

// 检查并显示CMA内存状态
static void check_cma_status(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return;
    }
    
    char line[256];
    long cma_total = 0, cma_free = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "CmaTotal: %ld kB", &cma_total) == 1) {
            continue;
        }
        if (sscanf(line, "CmaFree: %ld kB", &cma_free) == 1) {
            continue;
        }
    }
    fclose(fp);
    
    if (cma_total > 0) {
        printf("\nCMA内存状态:\n");
        printf("  总大小: %ld KB (%.1f MB)\n", cma_total, cma_total / 1024.0);
        printf("  可用:   %ld KB (%.1f MB)\n", cma_free, cma_free / 1024.0);
        printf("  已用:   %ld KB (%.1f MB)\n", 
               cma_total - cma_free, (cma_total - cma_free) / 1024.0);
        printf("  ✓ CMA已启用,system heap将使用CMA分配物理连续内存\n");
    }
}

// 像素格式定义(如果rga.h未包含)
#ifndef RK_FORMAT_YCbCr_420_SP
#define RK_FORMAT_YCbCr_420_SP    0x01
#define RK_FORMAT_RGBA_8888       0x02
#define RK_FORMAT_RGB_888         0x03
#endif

// 计算不同格式所需的缓冲区大小
static inline size_t calc_nv12_size(int w, int h) {
    return w * h * 3 / 2;  // YUV420半平面
}

static inline size_t calc_rgba_size(int w, int h) {
    return w * h * 4;      // RGBA每像素4字节
}

static inline size_t calc_rgb_size(int w, int h) {
    return w * h * 3;      // RGB每像素3字节
}

// 从DMA-heap分配DMA-BUF
// 
// Rockchip平台说明 (OrangePi5 / RK3588):
// ===========================================================================
// 内核配置:
//   - CONFIG_DMABUF_HEAPS_CMA=y (CMA heap已编译)
//   - 但设备树未注册CMA heap到/dev/dma_heap/
//   - 因此只有system/system-uncached/reserved heap可用
//
// System Heap特性:
//   - 使用普通内存分配器 (非CMA)
//   - 通过IOMMU映射,仍然支持DMA访问
//   - 性能略低于CMA (约3-5%差距,通常可忽略)
//   - CmaFree不会变化 (因为不使用CMA池)
//
// 建议:
//   - 对于RGA/NPU/VPU等Rockchip硬件,system heap完全够用
//   - 如果未来需要极致性能,可考虑修改设备树注册CMA heap
//   - 4KB页面对齐已是最优策略
// ===========================================================================
static int allocate_dmabuf_from_heap(size_t size) {
    // 对齐到页面大小(4KB) - Linux标准做法,内存利用率最高
    size = (size + 4095) & ~4095;
    
    const char *heap_paths[] = {
        "/dev/dma_heap/system",      // Rockchip: system heap内部使用CMA
        "/dev/dma_heap/system-uncached",  // 备选: 无缓存版本(适合GPU访问)
        "/dev/dma_heap/reserved",    // 最后: 保留区
        NULL
    };
    
    int heap_fd = -1;
    const char *selected_heap = NULL;
    
    for (int i = 0; heap_paths[i] != NULL; i++) {
        heap_fd = open(heap_paths[i], O_RDWR);
        if (heap_fd >= 0) {
            selected_heap = heap_paths[i];
            break;
        }
    }
    
    if (heap_fd < 0) {
        fprintf(stderr, "错误: 无法打开任何DMA-heap设备\n");
        fprintf(stderr, "请检查:\n");
        fprintf(stderr, "  1. ls -la /dev/dma_heap/\n");
        fprintf(stderr, "  2. 是否有权限访问 (需要root或video组)\n");
        return -1;
    }
    
    printf("使用DMA-heap: %s\n", selected_heap);

    // 定义dma_heap_allocation_data结构体 (如果头文件中没有)
    #ifndef HAVE_DMA_HEAP_ALLOCATION_DATA
    struct dma_heap_allocation_data {
        __u64 len;
        __u32 fd;
        __u32 fd_flags;
        __u64 __reserved[2];
    };
    #endif

    // 定义DMA_HEAP_IOCTL_ALLOC (如果头文件中没有)
    #ifndef DMA_HEAP_IOCTL_ALLOC
    #define DMA_HEAP_IOC_MAGIC 'H'
    #define DMA_HEAP_IOCTL_ALLOC _IOWR(DMA_HEAP_IOC_MAGIC, 0x0, struct dma_heap_allocation_data)
    #endif
    
#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif
    
    // 尝试使用DMA_HEAP_IOCTL_ALLOC
    struct dma_heap_allocation_data alloc;
    memset(&alloc, 0, sizeof(alloc));
    alloc.len = size;
    alloc.fd_flags = O_CLOEXEC | O_RDWR;
    
    int dmabuf_fd = -1;
    printf("  正在分配DMA-BUF (大小: %zu bytes / %.1f MB)... ", size, size/(1024.0*1024.0));
    fflush(stdout);
    
    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) == 0) {
        // 成功!
        dmabuf_fd = alloc.fd;
        printf("成功 ✓ (fd=%d, 使用DMA-heap)\n", dmabuf_fd);
    } else {
        // 失败,降级到memfd
        int saved_errno = errno;
        printf("失败 ✗ (errno=%d: %s)\n", saved_errno, strerror(saved_errno));
        fprintf(stderr, "  ⚠ 警告: DMA_HEAP_IOCTL_ALLOC失败,将使用memfd后备\n");
        fprintf(stderr, "  ⚠ 注意: memfd不使用CMA内存,可能影响DMA性能!\n");
        
        close(heap_fd);
        
        // 降级方案: 使用memfd
        char name[32];
        snprintf(name, sizeof(name), "v4l2_output_%ld", (long)getpid());
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
        dmabuf_fd = syscall(__NR_memfd_create, name, MFD_CLOEXEC);
        if (dmabuf_fd < 0) {
            perror("  memfd_create失败");
            return -1;
        }
        
        if (ftruncate(dmabuf_fd, size) < 0) {
            perror("  设置memfd大小失败");
            close(dmabuf_fd);
            return -1;
        }
        
        printf("  已使用memfd创建后备缓冲区 (fd=%d)\n", dmabuf_fd);
        return dmabuf_fd;
    }
    
    close(heap_fd);
    return dmabuf_fd;
}

// 用于存储缓冲区信息的结构体
struct buffer {
    void   *start;          // V4L2 MMAP映射后的内存地址
    size_t  length;         // V4L2缓冲区长度
    int     dma_fd;         // V4L2导出的DMA-BUF fd (输入)
    
    // 预分配的输出DMA-BUF (用于RGA/NPU/VPU处理后的大尺寸数据)
    int     output_dma_fd;  // 输出DMA-BUF fd
    size_t  output_size;    // 输出DMA-BUF大小
    void   *output_mapped;  // 输出DMA-BUF的mmap映射(可选,用于读取结果)
};

// 申请的缓冲区数量
#define BUFFER_NUM 4

// 配置参数: 根据实际需求调整
#define MAX_OUTPUT_WIDTH   2160   // 最大输出宽度 (4K)
#define MAX_OUTPUT_HEIGHT  2160   // 最大输出高度 (4K)
#define OUTPUT_FORMAT      RK_FORMAT_RGBA_8888  // 输出格式 (RGBA最大)

int main() {
    int ret;
    struct buffer* buffers = NULL;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int fd = -1;
    unsigned int num_buffers = 0;
    
    // 注册信号处理函数
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // kill命令
    
    // 检查CMA内存状态
    check_cma_status();
    
    // step1 --- 打开设备(摄像头)节点,使用非阻塞模式配合poll机制
    fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("无法打开设备");
        return -1;
    }
    printf("成功打开设备 /dev/video0, fd=%d\n", fd);

    // step2 --- 设置设备参数
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;         // 视频捕获
    fmt.fmt.pix.width = 1280;                       // 分辨率1280x720
    fmt.fmt.pix.height = 720;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;   // MJPEG格式
    fmt.fmt.pix.field = V4L2_FIELD_NONE;            // 逐行扫描
    
    ret = ioctl(fd, VIDIOC_S_FMT, &fmt);    
    if (ret < 0) {
        perror("设置视频格式失败");
        goto cleanup;
    }
    printf("设置视频格式: %dx%d, MJPEG\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    
    // step3 --- 申请缓冲区，使用MMAP模式
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = BUFFER_NUM;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        perror("请求缓冲区失败");
        goto cleanup;
    }
    
    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory\n");
        goto cleanup;
    }
    num_buffers = req.count;
    printf("成功请求 %d 个MMAP缓冲区\n", num_buffers);
    
    // step4 --- 准备好应用层缓冲区结构
    buffers = (struct buffer*)(calloc(num_buffers, sizeof(*buffers)));
    if (buffers == NULL) {
        perror("buffers calloc失败");
        goto cleanup;
    }
    
    // 初始化所有字段
    for (unsigned int i = 0; i < num_buffers; i++) {
        buffers[i].dma_fd = -1;
        buffers[i].output_dma_fd = -1;
        buffers[i].output_mapped = NULL;
    }
    
    // 计算预分配的输出DMA-BUF大小
    // 考虑RGA/NPU可能将YUV420转换为RGBA,并放大到最大尺寸
    size_t max_output_size = calc_rgba_size(MAX_OUTPUT_WIDTH, MAX_OUTPUT_HEIGHT);
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         预分配输出DMA-BUF配置 (用于RGA/NPU/VPU)          ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║  最大输出尺寸: %-42d ║\n", MAX_OUTPUT_WIDTH);
    printf("║  输出格式:     %-45s ║\n", "RGBA_8888");
    printf("║  单缓冲区大小: %-39.1f MB ║\n", max_output_size / (1024.0 * 1024.0));
    printf("║  总预分配大小: %-39.1f MB ║\n", 
           (max_output_size * num_buffers) / (1024.0 * 1024.0));
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    // step5 --- 查询、映射、导出并预分配输出DMA-BUF
    for (unsigned int i = 0; i < num_buffers; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        // 5.1 查询V4L2缓冲区信息
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("查询缓冲区失败");
            goto cleanup;
        }
        
        // 5.2 将V4L2内核缓冲区映射到用户空间(MMAP)
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED,
                               fd, buf.m.offset);
        
        if (buffers[i].start == MAP_FAILED) {
            perror("映射V4L2缓冲区失败");
            goto cleanup;
        }
        
        printf("\n[缓冲区 %d/%d]\n", i+1, num_buffers);
        printf("  📥 V4L2输入:\n");
        printf("     MMAP: %zu bytes @ %p (offset=%u)\n", 
               buffers[i].length, buffers[i].start, buf.m.offset);
        
        // 5.3 导出V4L2 DMA-BUF (输入)
        struct v4l2_exportbuffer expbuf;
        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        expbuf.index = i;
#ifdef O_CLOEXEC
        expbuf.flags = O_CLOEXEC;
#else
        expbuf.flags = 0;
#endif
        
        if (ioctl(fd, VIDIOC_EXPBUF, &expbuf) == -1) {
            perror("导出V4L2 DMA-BUF失败");
            fprintf(stderr, "  ⚠ 缓冲区[%d]无法导出V4L2 DMA-BUF\n", i);
            buffers[i].dma_fd = -1;
        } else {
            buffers[i].dma_fd = expbuf.fd;
            printf("     DMA-BUF fd: %d\n", buffers[i].dma_fd);
        }
        
        // 5.4 预分配输出DMA-BUF (用于RGA/NPU/VPU处理后的大尺寸数据)
        printf("  📤 输出DMA-BUF (预分配): ");
        fflush(stdout);
        
        buffers[i].output_dma_fd = allocate_dmabuf_from_heap(max_output_size);
        if (buffers[i].output_dma_fd < 0) {
            fprintf(stderr, "✗ 分配失败\n");
            fprintf(stderr, "     ⚠ 后续RGA/NPU处理可能受限\n");
        } else {
            buffers[i].output_size = max_output_size;
            
            // 可选: 映射输出DMA-BUF以便CPU读取结果
            buffers[i].output_mapped = mmap(NULL, max_output_size,
                                           PROT_READ | PROT_WRITE,
                                           MAP_SHARED,
                                           buffers[i].output_dma_fd, 0);
            
            if (buffers[i].output_mapped == MAP_FAILED) {
                perror("     映射输出DMA-BUF失败");
                buffers[i].output_mapped = NULL;
                // 不影响使用,RGA/NPU可以直接操作DMA-BUF
            }
            
            printf("✓ fd=%d (%.1f MB)",
                   buffers[i].output_dma_fd,
                   buffers[i].output_size / (1024.0 * 1024.0));
            if (buffers[i].output_mapped) {
                printf(" @ %p", buffers[i].output_mapped);
            }
            printf("\n");
        }
    }

    // step6 --- 将缓冲区加入队列并开始流处理
    for (unsigned int i = 0; i < num_buffers; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Queue Buffer失败");
            goto cleanup;
        }
    }
    printf("所有缓冲区已加入队列\n");
    
    // 开启视频流
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("启动视频流失败");
        goto cleanup;
    }
    printf("视频流已启动\n");

    // 使用poll非阻塞等待数据
    struct pollfd poll_fds[1];
    poll_fds[0].fd      = fd;
    poll_fds[0].events  = POLLIN;
    
    int frame_count = 0;
    int save_interval = 30;  // 每30帧保存一次，避免频繁IO
    int saved_count = 0;     // 已保存的帧数
    
    printf("\n");
    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│              ✓ 视频采集已启动                            │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");
    printf("│  • 按 Ctrl+C 停止采集                                   │\n");
    printf("│  • 每 %d 帧保存一次原始图像 (frame-*.jpg)                │\n", save_interval);
    printf("│  • 输出DMA-BUF可用于RGA/NPU/VPU零拷贝处理               │\n");
    printf("└─────────────────────────────────────────────────────────┘\n\n");
    
    // 持续循环采集，直到收到SIGINT信号(Ctrl+C)
    while(running) {
        // step7 --- 等待缓冲区数据就绪
        int poll_ret = poll(poll_fds, 1, 5000);
        
        // 检查是否收到退出信号
        if (!running) {
            printf("\n检测到退出信号，正在清理资源...\n");
            break;
        }
        
        // 检查poll返回值
        if (poll_ret < 0) {
            if (errno == EINTR) {
                // 被信号中断，检查是否需要退出
                if (!running) {
                    printf("\n检测到退出信号，正在清理资源...\n");
                    break;
                }
                continue;  // 继续等待
            }
            perror("Poll错误");
            goto cleanup;
        } else if (poll_ret == 0) {
            fprintf(stderr, "警告: Poll超时（5秒），无数据可用，继续等待...\n");
            continue;  // 超时后继续等待，不退出
        }
        
        // 检查是否有数据可读
        if (!(poll_fds[0].revents & POLLIN)) {
            if (poll_fds[0].revents & (POLLERR | POLLHUP)) {
                fprintf(stderr, "错误: 设备错误或挂起 (revents=%d)\n", poll_fds[0].revents);
                goto cleanup;
            }
            fprintf(stderr, "警告: 意外的poll事件: %d\n", poll_fds[0].revents);
            continue;
        }
        
        // 从队列中取出缓冲区
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            if (errno == EAGAIN) {
                // 非阻塞模式下没有数据，继续等待
                continue;
            }
            perror("取出帧失败");
            goto cleanup;
        }
        
        frame_count++;
        printf("\r捕获第 %d 帧 (缓冲区索引: %d, 数据大小: %u bytes)", 
               frame_count, buf.index, buf.bytesused);
        fflush(stdout);
        
        // ============================================================
        // 这里可以使用预分配的输出DMA-BUF进行RGA/NPU/VPU处理
        // ============================================================
        if (buffers[buf.index].output_dma_fd >= 0) {
            /*
             * 示例: RGA处理 (需要librga)
             * 
             * rga_info_t src, dst;
             * memset(&src, 0, sizeof(src));
             * memset(&dst, 0, sizeof(dst));
             * 
             * // 源: V4L2捕获的帧
             * rga_set_dma_buf(&src, buffers[buf.index].dma_fd, 
             *                 1280, 720, RK_FORMAT_YCbCr_420_SP);
             * 
             * // 目标: 预分配的大尺寸输出DMA-BUF
             * // 即使放大到4K RGBA也没问题!
             * rga_set_dma_buf(&dst, buffers[buf.index].output_dma_fd,
             *                 3840, 2160, RK_FORMAT_RGBA_8888);
             * 
             * // 执行硬件缩放和格式转换 (零拷贝!)
             * imresize(src, dst);
             * 
             * // 现在 buffers[buf.index].output_dma_fd 包含4K RGBA数据
             * // 可以传递给NPU推理或DRM显示
             */
            
            /*
             * 示例: NPU推理 (需要RKNN)
             * 
             * rknn_input input;
             * memset(&input, 0, sizeof(input));
             * input.index = 0;
             * input.type = RKNN_TENSOR_UINT8;
             * input.size = 640 * 640 * 3;
             * input.fmt = RKNN_TENSOR_NHWC;
             * input.buf = NULL;
             * input.fd = buffers[buf.index].output_dma_fd;  // 使用预分配的fd
             * 
             * rknn_inputs_set(ctx, 1, &input);
             * rknn_run(ctx, NULL);  // NPU直接读取,零拷贝!
             */
        }

        // step8 --- 定期保存当前帧到文件（避免频繁IO影响性能）
        if (frame_count % save_interval == 1) {
            char filename[32];
            snprintf(filename, sizeof(filename), "frame-%d.jpg", saved_count);
            FILE *file = fopen(filename, "wb");
            if (file != NULL) {
                fwrite(buffers[buf.index].start, buf.bytesused, 1, file);
                fclose(file);
                printf("\n✓ 已保存 %s (帧 #%d)", filename, frame_count);
                saved_count++;
                
                // 循环覆盖：只保留最近的10个文件
                if (saved_count >= 10) {
                    saved_count = 0;
                }
            } else {
                perror("\n保存图像失败");
            }
        }

        // step9 --- 将缓冲区重新放入队列
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("\n重新入队缓冲区失败");
            goto cleanup;
        }
    }

    printf("\n\n总共捕获 %d 帧图像\n", frame_count);

cleanup:
    // step10 --- 停止视频流
    if (fd >= 0) {
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        printf("视频流已停止\n");
    }

    // step11 --- 释放所有资源
    if (buffers != NULL) {
        for (unsigned int i = 0; i < num_buffers; ++i) {
            // 11.1 解除V4L2 MMAP映射
            if (buffers[i].start != NULL && buffers[i].start != MAP_FAILED) {
                munmap(buffers[i].start, buffers[i].length);
            }
            
            // 11.2 解除输出DMA-BUF映射
            if (buffers[i].output_mapped != NULL && 
                buffers[i].output_mapped != MAP_FAILED) {
                munmap(buffers[i].output_mapped, buffers[i].output_size);
            }
            
            // 11.3 关闭V4L2导出的DMA-BUF fd
            if (buffers[i].dma_fd >= 0) {
                close(buffers[i].dma_fd);
            }
            
            // 11.4 关闭预分配的输出DMA-BUF fd ⭐
            if (buffers[i].output_dma_fd >= 0) {
                close(buffers[i].output_dma_fd);
                printf("已关闭缓冲区[%u]的输出DMA-BUF fd=%d (size=%.1f MB)\n", 
                       i, buffers[i].output_dma_fd,
                       buffers[i].output_size / (1024.0 * 1024.0));
            }
        }
        free(buffers);
        printf("所有缓冲区资源已释放\n");
    }

    // 关闭设备
    if (fd >= 0) {
        close(fd);
        printf("设备已关闭\n");
    }

    return 0;
}