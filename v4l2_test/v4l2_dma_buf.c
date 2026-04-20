/* v4l2_dma_buf.c - 使用DMA_BUF方式的V4L2视频采集程序 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <linux/dma-buf.h>

// 用于存储缓冲区信息的结构体
struct buffer {
    int     fd;         // DMA_BUF文件描述符
    void   *start;      // 映射后的内存地址
    size_t  length;     // 缓冲区长度
};

// 申请的缓冲区数量
#define BUFFER_NUM 4

int main() {
    int ret;
    struct buffer* buf_app = NULL;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int fd = -1;
    
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
    
    // step3 --- 申请在内核层存放设备数据的buf，memory设置为DMA_BUF模式
    struct v4l2_requestbuffers buf_req;
    memset(&buf_req, 0, sizeof(buf_req));
    buf_req.count = BUFFER_NUM;
    buf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_req.memory = V4L2_MEMORY_DMABUF;  // 使用DMA_BUF内存类型
    
    ret = ioctl(fd, VIDIOC_REQBUFS, &buf_req);
    if (ret < 0) {
        perror("请求DMA_BUF缓冲区失败");
        goto cleanup;
    }
    printf("成功请求 %d 个DMA_BUF缓冲区\n", buf_req.count);
    
    // step4 --- 准备好应用层缓冲区结构
    buf_app = (struct buffer*)(calloc(BUFFER_NUM, sizeof(*buf_app)));
    if (buf_app == NULL) {
        perror("buf_app calloc失败");
        goto cleanup;
    }
    
    // step5 --- 为每个缓冲区创建DMA_BUF并映射
    for (unsigned int i = 0; i < buf_req.count; i++) {
        // 创建DMA_BUF文件描述符
        // 这里我们使用memfd_create来创建一个匿名文件作为DMA_BUF的替代
        // 在实际嵌入式系统中，可能需要从其他驱动获取真正的DMA_BUF fd
        
        // 方法1: 使用memfd_create（需要Linux 3.17+）
        char buf_name[32];
        snprintf(buf_name, sizeof(buf_name), "v4l2_buf_%d", i);
        
        #ifdef __NR_memfd_create
        int dma_fd = syscall(__NR_memfd_create, buf_name, MFD_CLOEXEC);
        #else
        // 降级方案：使用shm_open
        int dma_fd = shm_open(buf_name, O_RDWR | O_CREAT | O_TRUNC, 0600);
        #endif
        
        if (dma_fd < 0) {
            perror("创建DMA_BUF失败");
            goto cleanup;
        }
        
        // 设置缓冲区大小（先查询需要的长度）
        struct v4l2_buffer buf_query;
        memset(&buf_query, 0, sizeof(buf_query));
        buf_query.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf_query.memory = V4L2_MEMORY_DMABUF;
        buf_query.index = i;
        
        // 先查询缓冲区信息以获取所需大小
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf_query) == -1) {
            perror("查询缓冲区失败");
            close(dma_fd);
            goto cleanup;
        }
        
        // 调整DMA_BUF大小
        if (ftruncate(dma_fd, buf_query.length) < 0) {
            perror("设置DMA_BUF大小失败");
            close(dma_fd);
            goto cleanup;
        }
        
        // 将DMA_BUF映射到用户空间
        void *mapped = mmap(NULL, buf_query.length,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           dma_fd, 0);
        
        if (mapped == MAP_FAILED) {
            perror("映射DMA_BUF失败");
            close(dma_fd);
            goto cleanup;
        }
        
        // 保存缓冲区信息
        buf_app[i].fd = dma_fd;
        buf_app[i].start = mapped;
        buf_app[i].length = buf_query.length;
        
        printf("缓冲区[%d]: fd=%d, size=%zu bytes, mapped=%p\n", 
               i, dma_fd, buf_app[i].length, mapped);
    }

    // step6 --- 将DMA_BUF加入队列并开始流处理
    for (unsigned int i = 0; i < buf_req.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = i;
        buf.m.fd = buf_app[i].fd;  // 传递DMA_BUF的文件描述符
        
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
    const int max_frames = 10;  // 最多捕获10帧
    
    printf("\n开始采集图像...\n");
    while(frame_count < max_frames) {
        // step7 --- 等待缓冲区数据就绪
        int poll_ret = poll(poll_fds, 1, 5000);
        
        // 检查poll返回值
        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断，重试
            }
            perror("Poll错误");
            goto cleanup;
        } else if (poll_ret == 0) {
            fprintf(stderr, "Poll超时（5秒），无数据可用\n");
            goto cleanup;
        }
        
        // 检查是否有数据可读
        if (!(poll_fds[0].revents & POLLIN)) {
            fprintf(stderr, "意外的poll事件: %d\n", poll_fds[0].revents);
            goto cleanup;
        }
        
        // 从队列中取出缓冲区
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_DMABUF;
        
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("取出帧失败");
            goto cleanup;
        }
        
        printf("捕获第 %d 帧 (缓冲区索引: %d, 数据大小: %u bytes)\n", 
               frame_count, buf.index, buf.bytesused);

        // step8 --- 保存当前帧到文件
        char filename[32];
        snprintf(filename, sizeof(filename), "dma_frame-%d.jpg", frame_count);
        FILE *file = fopen(filename, "wb");
        if (file != NULL) {
            // 从映射的DMA_BUF内存中读取数据
            fwrite(buf_app[buf.index].start, buf.bytesused, 1, file);
            fclose(file);
            printf("已保存 %s\n", filename);
        } else {
            perror("保存图像失败");
        }

        // step9 --- 将缓冲区重新放入队列
        buf.m.fd = buf_app[buf.index].fd;  // 重新设置DMA_BUF fd
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("重新入队缓冲区失败");
            goto cleanup;
        }
        
        frame_count++;
    }

    printf("\n总共捕获 %d 帧图像\n", frame_count);

cleanup:
    // step10 --- 停止视频流
    if (fd >= 0) {
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        printf("视频流已停止\n");
    }

    // step11 --- 释放所有资源
    if (buf_app != NULL) {
        for (int i = 0; i < BUFFER_NUM; ++i) {
            // 解除内存映射
            if (buf_app[i].start != NULL && buf_app[i].start != MAP_FAILED) {
                munmap(buf_app[i].start, buf_app[i].length);
            }
            // 关闭DMA_BUF文件描述符
            if (buf_app[i].fd >= 0) {
                close(buf_app[i].fd);
            }
        }
        free(buf_app);
        printf("缓冲区资源已释放\n");
    }

    // 关闭设备
    if (fd >= 0) {
        close(fd);
        printf("设备已关闭\n");
    }

    return 0;
}
