#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <signal.h>
#include <time.h>

// 全局变量用于信号处理
volatile sig_atomic_t running = 1;

void signal_handler(int /*signum*/) {
    running = 0;
}

class V4L2Capture {
private:
    int fd;                          // 设备文件描述符
    char* device;                    // 设备路径
    unsigned int width;              // 图像宽度
    unsigned int height;             // 图像高度
    unsigned int fps;                // 帧率
    
    struct Buffer {
        void* start;
        size_t length;
    };
    
    std::vector<Buffer> buffers;     // 缓冲区数组
    unsigned int n_buffers;          // 缓冲区数量
    
public:
    V4L2Capture(const char* dev, unsigned int w, unsigned int h, unsigned int f)
        : fd(-1), device(strdup(dev)), width(w), height(h), fps(f), n_buffers(4) {
    }
    
    ~V4L2Capture() {
        cleanup();
        free(device);
    }
    
    // 打开设备
    bool open() {
        fd = ::open(device, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            perror("无法打开设备");
            return false;
        }
        
        // 查询设备能力
        struct v4l2_capability cap;
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
            perror("查询设备能力失败");
            return false;
        }
        
        printf("设备名称: %s\n", cap.card);
        printf("驱动名称: %s\n", cap.driver);
        printf("总线信息: %s\n", cap.bus_info);
        
        return true;
    }
    
    // 设置视频格式
    bool setFormat() {
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        
        // 获取当前格式
        if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
            perror("获取当前格式失败");
            return false;
        }
        
        printf("当前格式: %dx%d, 像素格式: %c%c%c%c\n",
               fmt.fmt.pix.width, fmt.fmt.pix.height,
               fmt.fmt.pix.pixelformat & 0xFF,
               (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
               (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
               (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
        
        // 设置新格式 - 优先使用MJPEG格式
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        
        if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            // 如果MJPEG不支持，尝试YUYV
            printf("MJPEG格式不支持，尝试YUYV格式...\n");
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
            
            if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
                perror("设置视频格式失败");
                return false;
            }
        }
        
        printf("设置格式: %dx%d, 像素格式: %c%c%c%c\n",
               fmt.fmt.pix.width, fmt.fmt.pix.height,
               fmt.fmt.pix.pixelformat & 0xFF,
               (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
               (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
               (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
        
        width = fmt.fmt.pix.width;
        height = fmt.fmt.pix.height;
        
        return true;
    }
    
    // 设置帧率
    bool setFPS() {
        struct v4l2_streamparm parm;
        memset(&parm, 0, sizeof(parm));
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        
        if (ioctl(fd, VIDIOC_G_PARM, &parm) < 0) {
            perror("获取流参数失败");
            return false;
        }
        
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = fps;
        
        if (ioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
            perror("设置帧率失败");
        } else {
            printf("设置帧率: %d fps\n", fps);
        }
        
        return true;
    }
    
    // 请求缓冲区并映射内存
    bool requestBuffers() {
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = n_buffers;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        
        if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
            perror("请求缓冲区失败");
            return false;
        }
        
        if (req.count < 2) {
            fprintf(stderr, "缓冲区数量不足\n");
            return false;
        }
        
        n_buffers = req.count;
        buffers.resize(n_buffers);
        
        // 映射所有缓冲区
        for (unsigned int i = 0; i < n_buffers; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            
            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
                perror("查询缓冲区失败");
                return false;
            }
            
            buffers[i].length = buf.length;
            buffers[i].start = mmap(NULL, buf.length,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, fd, buf.m.offset);
            
            if (buffers[i].start == MAP_FAILED) {
                perror("内存映射失败");
                return false;
            }
        }
        
        printf("成功映射 %u 个缓冲区\n", n_buffers);
        return true;
    }
    
    // 将缓冲区加入队列
    bool queueAllBuffers() {
        for (unsigned int i = 0; i < n_buffers; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            
            if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                perror("缓冲区入队失败");
                return false;
            }
        }
        return true;
    }
    
    // 开始采集
    bool startStreaming() {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
            perror("启动流失败");
            return false;
        }
        printf("开始采集视频...\n");
        return true;
    }
    
    // 停止采集
    void stopStreaming() {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        printf("停止采集视频\n");
    }
    
    // 捕获一帧数据
    bool captureFrame(std::vector<uint8_t>& frame_data, unsigned int& bytes_used) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        // 出队已填充的缓冲区
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            return false;
        }
        
        // 复制数据
        frame_data.assign((uint8_t*)buffers[buf.index].start,
                         (uint8_t*)buffers[buf.index].start + buf.bytesused);
        bytes_used = buf.bytesused;
        
        // 重新入队缓冲区
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("缓冲区重新入队失败");
            return false;
        }
        
        return true;
    }
    
    // 清理资源
    void cleanup() {
        stopStreaming();
        
        // 解除内存映射
        for (unsigned int i = 0; i < n_buffers; i++) {
            if (buffers[i].start != MAP_FAILED) {
                munmap(buffers[i].start, buffers[i].length);
                buffers[i].start = NULL;
            }
        }
        buffers.clear();
        
        // 关闭设备
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }
    
    unsigned int getWidth() const { return width; }
    unsigned int getHeight() const { return height; }
};

int main(int argc, char** argv) {
    // 配置参数
    const char* device = "/dev/video0";
    unsigned int width = 1920;
    unsigned int height = 1080;
    unsigned int fps = 30;
    int duration = 30;
    const char* output_file = "output.avi";
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            device = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            fps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("用法: %s [选项]\n", argv[0]);
            printf("选项:\n");
            printf("  -d <设备>    视频设备路径 (默认: /dev/video0)\n");
            printf("  -w <宽度>    图像宽度 (默认: 1920)\n");
            printf("  -h <高度>    图像高度 (默认: 1080)\n");
            printf("  -f <帧率>    帧率 (默认: 30)\n");
            printf("  -t <时长>    采集时长（秒） (默认: 30)\n");
            printf("  -o <文件>    输出文件名 (默认: output.avi)\n");
            printf("  --help       显示帮助信息\n");
            return 0;
        }
    }
    
    printf("========================================\n");
    printf("V4L2 视频采集程序\n");
    printf("========================================\n");
    printf("设备: %s\n", device);
    printf("分辨率: %dx%d\n", width, height);
    printf("帧率: %d fps\n", fps);
    printf("采集时长: %d 秒\n", duration);
    printf("输出文件: %s\n", output_file);
    printf("========================================\n\n");
    
    // 注册信号处理器
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 创建V4L2采集对象
    V4L2Capture capture(device, width, height, fps);
    
    // 打开设备
    if (!capture.open()) {
        fprintf(stderr, "错误: 无法打开设备 %s\n", device);
        return -1;
    }
    
    // 设置视频格式
    if (!capture.setFormat()) {
        fprintf(stderr, "错误: 无法设置视频格式\n");
        return -1;
    }
    
    // 设置帧率
    capture.setFPS();
    
    // 请求缓冲区
    if (!capture.requestBuffers()) {
        fprintf(stderr, "错误: 无法请求缓冲区\n");
        return -1;
    }
    
    // 将所有缓冲区加入队列
    if (!capture.queueAllBuffers()) {
        fprintf(stderr, "错误: 无法将缓冲区加入队列\n");
        return -1;
    }
    
    // 开始采集
    if (!capture.startStreaming()) {
        fprintf(stderr, "错误: 无法启动采集\n");
        return -1;
    }
    
    // 初始化OpenCV视频编码器
    cv::VideoWriter writer;
    int codec = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    double video_fps = fps;
    cv::Size frame_size(capture.getWidth(), capture.getHeight());
    
    writer.open(output_file, codec, video_fps, frame_size, true);
    if (!writer.isOpened()) {
        fprintf(stderr, "错误: 无法创建视频文件 %s\n", output_file);
        fprintf(stderr, "提示: 可能需要安装GStreamer或FFmpeg后端\n");
        capture.cleanup();
        return -1;
    }
    
    printf("\n开始录制视频...\n");
    printf("按 Ctrl+C 停止录制\n\n");
    
    // 记录开始时间
    time_t start_time = time(NULL);
    unsigned int frame_count = 0;
    unsigned int dropped_frames = 0;
    
    // 采集循环
    while (running) {
        // 检查是否达到指定时长
        time_t current_time = time(NULL);
        if (difftime(current_time, start_time) >= duration) {
            printf("\n已达到指定采集时长 %d 秒\n", duration);
            break;
        }
        
        // 捕获一帧
        std::vector<uint8_t> frame_data;
        unsigned int bytes_used = 0;
        
        if (!capture.captureFrame(frame_data, bytes_used)) {
            usleep(1000);
            continue;
        }
        
        // 使用OpenCV解码JPEG数据
        cv::Mat frame = cv::imdecode(cv::Mat(frame_data), cv::IMREAD_COLOR);
        
        if (frame.empty()) {
            dropped_frames++;
            continue;
        }
        
        // 写入视频文件
        writer.write(frame);
        frame_count++;
        
        // 每100帧打印一次进度
        if (frame_count % 100 == 0) {
            double elapsed = difftime(time(NULL), start_time);
            printf("\r已采集: %u 帧 (%.1f 秒), 丢弃: %u 帧",
                   frame_count, elapsed, dropped_frames);
            fflush(stdout);
        }
    }
    
    printf("\n\n采集完成!\n");
    printf("总帧数: %u\n", frame_count);
    printf("丢弃帧数: %u\n", dropped_frames);
    printf("实际时长: %.1f 秒\n", difftime(time(NULL), start_time));
    printf("输出文件: %s\n", output_file);
    
    // 清理资源
    writer.release();
    capture.cleanup();
    
    printf("\n程序退出\n");
    return 0;
}
