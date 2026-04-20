/* v4l2_sample.c 是一个最简单的v4l2程序 */
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

// 用于存储缓冲区信息的结构体
struct buffer {
    void   *start;
    size_t  length;
};
// 申请的缓冲区数量
#define BUFFER_NUM 4


int main() {
    int ret;
    struct buffer* buf_app = NULL;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int fd = -1;
    
    // step1 --- 打开设备(摄像头)节点,在后面使用了 poll 机制,所以可以使用 O_NONBLOCK 来非阻塞的打开设备
    fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("无法打开设备");
        return -1;
    }

    // step2 --- 设置设备参数
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;         // 视频捕获
    fmt.fmt.pix.width = 1280;                       //  分辨率1280x720
    fmt.fmt.pix.height = 720;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;   // MJPEG的格式
    fmt.fmt.pix.field = V4L2_FIELD_NONE;            // 视频采集的扫描方式
    ret = ioctl(fd, VIDIOC_S_FMT, &fmt);    
    if (ret < 0)
    {
        perror("设置视频格式失败");
        goto cleanup;
    }        
    
    // step3 --- 申请在内核层存放设备数据的buf，memory设置为内存映射的模式
    struct v4l2_requestbuffers buf_req;
    memset(&buf_req, 0, sizeof(buf_req));
    buf_req.count = BUFFER_NUM;
    buf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &buf_req);
    if (ret < 0)
    {
        perror("请求缓冲区失败");
        goto cleanup;
    }
    
    // step4 --- 准备好我们的应用层buf,callo 可以自动将所有位初始化为0
    buf_app = (struct buffer*)(calloc(BUFFER_NUM, sizeof(*buf_app)));
    if (buf_app == NULL)
    {
        perror("buf_app calloc buffer");
        goto cleanup;
    }
    
    // step5 --- 内存映射
    // 查询前面申请的内核层存放设备数据的buf是否成功
    for (unsigned int i = 0; i < buf_req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("查询缓冲区失败");
            goto cleanup;
        }
        // 没问题就将应用层buf与内核层buf进行内存映射
        buf_app[i].length = buf.length;
        buf_app[i].start = mmap(NULL, buf.length,
                        PROT_READ | PROT_WRITE, 
                        MAP_SHARED,
                        fd, buf.m.offset);
        if (buf_app[i].start == MAP_FAILED) {
            perror("mmap");
            buf_app[i].start = NULL;
            goto cleanup;
        }
    }

    // step6 --- 为申请的内核层buf申请消息队列，并开始流处理
    for (unsigned int i = 0; i < buf_req.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Queue Buffer");
            goto cleanup;
        }
    }
    
    // 开启视频流并设置类型
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("Start Capture");
        goto cleanup;
    }

    // 使用poll非阻塞的等待设备节点
    struct pollfd poll_fds[1];
    poll_fds[0].fd      = fd;
    poll_fds[0].events  = POLLIN;
    
    int frame_count = 0;
    const int max_frames = 10;  // 最多捕获10帧
    
    while(frame_count < max_frames)
    {
        // step7 --- 从缓冲区取出一帧图像
        int poll_ret = poll(poll_fds, 1, 5000);
        
        // 检查poll返回值
        if (poll_ret < 0) {
            // poll出错
            perror("Poll error");
            goto cleanup;
        } else if (poll_ret == 0) {
            // 超时，设备未准备好
            fprintf(stderr, "Poll timeout after 5 seconds, no data available\n");
            // 可以选择继续等待或退出
            // continue;  // 继续尝试下一帧
            goto cleanup;  // 或者退出程序
        }
        
        // 检查是否有数据可读
        if (!(poll_fds[0].revents & POLLIN)) {
            fprintf(stderr, "Unexpected poll revents: %d\n", poll_fds[0].revents);
            goto cleanup;
        }
        
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        // 等待缓冲区准备好
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Retrieving Frame");
            goto cleanup;
        }
        
        printf("Captured Frame %d (buffer index: %d)\n", frame_count, buf.index);

        // step8 --- 对图像进行操作，这里是保存当前帧到文件
        char filename[32];
        snprintf(filename, sizeof(filename), "frame-%d.jpg", frame_count);
        FILE *file = fopen(filename, "wb");
        if (file != NULL) {
            fwrite(buf_app[buf.index].start, buf.bytesused, 1, file);
            fclose(file);
            printf("Saved %s\n", filename);
        } else {
            perror("Saving Image");
        }

        // step9 --- 将缓冲区重新放入队列
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Requeue Buffer");
            goto cleanup;
        }
        
        frame_count++;
    }

    printf("Captured %d frames in total\n", frame_count);

cleanup:
    // step10 --- 停止流处理
    if (fd >= 0) {
        ioctl(fd, VIDIOC_STREAMOFF, &type);
    }

    // step11 --- 释放所有资源，解除内存映射
    if (buf_app != NULL) {
        for (int i = 0; i < BUFFER_NUM; ++i) {
            if (buf_app[i].start != NULL && buf_app[i].start != MAP_FAILED) {
                munmap(buf_app[i].start, buf_app[i].length);
            }
        }
        free(buf_app);
    }

    if (fd >= 0) {
        close(fd);
    }

    return 0;
}