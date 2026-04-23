/* 
 * dmabuf_usage_example.c - DMA-BUF使用示例
 * 
 * 这个文件展示了如何从v4l2_dma_buf.c中获取DMA-BUF fd，
 * 并将其用于RGA、NPU、DRM等硬件的零拷贝操作。
 * 
 * 注意：这是一个参考示例，需要根据实际硬件和库进行调整。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

// ============================================================================
// 示例1: 将DMA-BUF传递给RGA进行图像处理
// ============================================================================
#ifdef USE_RGA
#include <rga/RgaApi.h>

int example_rga_processing(int v4l2_dma_fd, int width, int height) {
    printf("=== RGA零拷贝处理示例 ===\n");
    
    // 初始化RGA上下文
    rga_info_t src_info, dst_info;
    memset(&src_info, 0, sizeof(src_info));
    memset(&dst_info, 0, sizeof(dst_info));
    
    // 设置源图像（来自V4L2的DMA-BUF，零拷贝！）
    rga_set_dma_buf(&src_info, v4l2_dma_fd, width, height, RK_FORMAT_YCbCr_420_SP);
    
    // 创建目标缓冲区（可以是另一个DMA-BUF或普通内存）
    int dst_size = (width / 2) * (height / 2) * 3 / 2;  // 缩放后的大小
    void *dst_buffer = malloc(dst_size);
    if (!dst_buffer) {
        perror("malloc失败");
        return -1;
    }
    
    // 设置目标图像
    rga_set_virtual_addr(&dst_info, dst_buffer, width/2, height/2, RK_FORMAT_YCbCr_420_SP);
    
    // 执行硬件缩放（零拷贝，CPU不参与数据传输）
    printf("执行RGA硬件缩放: %dx%d -> %dx%d\n", width, height, width/2, height/2);
    int ret = imresize(src_info, dst_info);
    if (ret != 0) {
        fprintf(stderr, "RGA处理失败: %d\n", ret);
        free(dst_buffer);
        return -1;
    }
    
    printf("✓ RGA处理完成（零拷贝）\n");
    
    // 清理
    free(dst_buffer);
    return 0;
}
#endif

// ============================================================================
// 示例2: 将DMA-BUF传递给NPU进行AI推理
// ============================================================================
#ifdef USE_RKNN
#include <rknn_api.h>

int example_npu_inference(int v4l2_dma_fd, int width, int height, 
                          const char* model_path) {
    printf("=== NPU零拷贝推理示例 ===\n");
    
    // 加载RKNN模型
    rknn_context ctx;
    FILE* fp = fopen(model_path, "rb");
    if (!fp) {
        perror("无法打开模型文件");
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    int model_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    void* model_data = malloc(model_size);
    fread(model_data, 1, model_size, fp);
    fclose(fp);
    
    // 初始化RKNN上下文
    int ret = rknn_init(&ctx, model_data, model_size, 0, NULL);
    free(model_data);
    
    if (ret < 0) {
        fprintf(stderr, "RKNN初始化失败: %d\n", ret);
        return -1;
    }
    
    // 准备输入tensor（使用DMA-BUF fd，零拷贝！）
    rknn_input inputs[1];
    memset(&inputs, 0, sizeof(inputs));
    
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = width * height * 3;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].buf = NULL;      // 不使用虚拟地址
    inputs[0].fd = v4l2_dma_fd; // 使用DMA-BUF fd！（关键）
    
    printf("设置NPU输入: DMA-BUF fd=%d, size=%d\n", v4l2_dma_fd, inputs[0].size);
    
    // 设置输入
    ret = rknn_inputs_set(ctx, 1, inputs);
    if (ret < 0) {
        fprintf(stderr, "设置输入失败: %d\n", ret);
        rknn_destroy(ctx);
        return -1;
    }
    
    // 执行推理（NPU直接读取DMA-BUF，零拷贝！）
    printf("执行NPU推理（零拷贝）...\n");
    ret = rknn_run(ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "推理失败: %d\n", ret);
        rknn_destroy(ctx);
        return -1;
    }
    
    // 获取输出
    rknn_output outputs[1];
    memset(&outputs, 0, sizeof(outputs));
    outputs[0].want_float = 0;
    
    ret = rknn_outputs_get(ctx, 1, outputs, NULL);
    if (ret < 0) {
        fprintf(stderr, "获取输出失败: %d\n", ret);
        rknn_destroy(ctx);
        return -1;
    }
    
    // 处理推理结果
    printf("✓ NPU推理完成（零拷贝）\n");
    printf("  输出大小: %d bytes\n", outputs[0].size);
    // ... 在这里处理outputs[0].buf ...
    
    // 释放输出
    rknn_outputs_release(ctx, 1, outputs);
    
    // 销毁上下文
    rknn_destroy(ctx);
    
    return 0;
}
#endif

// ============================================================================
// 示例3: 将DMA-BUF传递给DRM进行显示
// ============================================================================
#ifdef USE_DRM
#include <drm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int example_drm_display(int drm_fd, int v4l2_dma_fd, int width, int height) {
    printf("=== DRM零拷贝显示示例 ===\n");
    
    // 将DMA-BUF fd导入DRM
    struct drm_prime_handle prime_handle;
    memset(&prime_handle, 0, sizeof(prime_handle));
    
    prime_handle.fd = v4l2_dma_fd;
    prime_handle.flags = DRM_CLOEXEC | DRM_RDWR;
    
    int ret = ioctl(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle);
    if (ret < 0) {
        perror("DRM导入DMA-BUF失败");
        return -1;
    }
    
    uint32_t gem_handle = prime_handle.handle;
    printf("DMA-BUF已导入到DRM: GEM handle=%u\n", gem_handle);
    
    // 创建framebuffer
    uint32_t fb_id;
    uint32_t pitches[1] = { width * 4 };  // RGBA格式
    uint32_t handles[1] = { gem_handle };
    uint32_t offsets[1] = { 0 };
    
    ret = drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_ARGB8888,
                        handles, pitches, offsets, &fb_id, 0);
    if (ret < 0) {
        perror("创建framebuffer失败");
        drmCloseBufferHandle(drm_fd, gem_handle);
        return -1;
    }
    
    printf("✓ Framebuffer创建成功: fb_id=%u\n", fb_id);
    
    // 这里可以调用drmModeSetPlane将fb显示到屏幕
    // drmModeSetPlane(drm_fd, plane_id, crtc_id, fb_id, ...);
    
    // 清理
    drmModeRmFB(drm_fd, fb_id);
    drmCloseBufferHandle(drm_fd, gem_handle);
    
    printf("✓ DRM显示资源已释放\n");
    return 0;
}
#endif

// ============================================================================
// 示例4: 完整的视频处理pipeline
// ============================================================================
void example_full_pipeline() {
    printf("\n=== 完整视频处理Pipeline示例 ===\n");
    printf("V4L2摄像头 → RGA预处理 → NPU推理 → DRM显示\n");
    printf("   (DMA-BUF)  (零拷贝)   (零拷贝)  (零拷贝)\n\n");
    
    /*
     * 伪代码流程：
     * 
     * 1. 初始化V4L2捕获（使用v4l2_dma_buf.c的代码）
     *    - 获得 buffers[].dma_fd
     * 
     * 2. 初始化RGA
     *    rga_init();
     * 
     * 3. 初始化NPU
     *    rknn_init(&ctx, model);
     * 
     * 4. 初始化DRM
     *    drm_fd = open("/dev/dri/card0", O_RDWR);
     * 
     * 5. 主循环
     *    while (running) {
     *        // 从V4L2获取帧
     *        ioctl(v4l2_fd, VIDIOC_DQBUF, &buf);
     *        int camera_fd = buffers[buf.index].dma_fd;
     *        
     *        // RGA预处理（缩放、格式转换）
     *        int processed_fd = rga_preprocess(camera_fd, ...);
     *        
     *        // NPU推理
     *        rknn_input input = {.fd = processed_fd, ...};
     *        rknn_inputs_set(ctx, 1, &input);
     *        rknn_run(ctx, NULL);
     *        rknn_outputs_get(ctx, ...);
     *        
     *        // DRM显示（可选）
     *        drm_display(drm_fd, processed_fd, ...);
     *        
     *        // 清理临时DMA-BUF
     *        close(processed_fd);
     *        
     *        // 重新入队V4L2缓冲区
     *        ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
     *    }
     * 
     * 6. 清理资源
     *    - 关闭所有DMA-BUF fd
     *    - 停止V4L2流
     *    - 销毁NPU上下文
     *    - 关闭DRM设备
     */
    
    printf("请参考README.md中的完整示例代码\n");
}

// ============================================================================
// 辅助函数：创建DMA-BUF
// ============================================================================
#ifdef USE_DMA_HEAP
int create_dma_buf(size_t size) {
    /*
     * 从DMA-heap分配DMA-BUF
     * 适用于需要创建新的DMA-BUF缓冲区的场景
     */
    
    // 方法1: 使用ION（旧版Rockchip）
    #ifdef USE_ION
    int ion_fd = open("/dev/ion", O_RDWR);
    struct ion_allocation_data alloc;
    alloc.len = size;
    alloc.heap_id_mask = ION_HEAP_TYPE_DMA_MASK;
    alloc.flags = 0;
    
    ioctl(ion_fd, ION_IOC_ALLOC, &alloc);
    
    struct ion_fd_data fd_data;
    fd_data.handle = alloc.handle;
    ioctl(ion_fd, ION_IOC_SHARE, &fd_data);
    
    int dma_fd = fd_data.fd;
    close(ion_fd);
    
    return dma_fd;
    #endif
    
    // 方法2: 使用DMA-heap（新版Linux）
    #ifdef USE_DMA_HEAP_NEW
    int heap_fd = open("/dev/dma_heap/system", O_RDWR);
    struct dma_heap_allocation_data alloc;
    alloc.len = size;
    alloc.fd_flags = O_CLOEXEC | O_RDWR;
    
    ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
    close(heap_fd);
    
    return alloc.fd;
    #endif
    
    return -1;
}
#endif

// ============================================================================
// 主函数：演示各种用法
// ============================================================================
int main(int argc, char** argv) {
    printf("DMA-BUF使用示例程序\n");
    printf("==================\n\n");
    
    printf("注意: 这是一个参考示例，展示了如何在不同场景下使用DMA-BUF。\n");
    printf("要编译特定功能的示例，需要定义相应的宏：\n");
    printf("  -DUSE_RGA    : 启用RGA示例\n");
    printf("  -DUSE_RKNN   : 启用NPU示例\n");
    printf("  -DUSE_DRM    : 启用DRM示例\n");
    printf("  -DUSE_DMA_HEAP: 启用DMA-heap分配示例\n\n");
    
    // 演示完整pipeline概念
    example_full_pipeline();
    
    printf("\n实际使用时，请参考:\n");
    printf("1. v4l2_dma_buf.c - V4L2捕获和DMA-BUF导出\n");
    printf("2. README.md - 详细的使用文档和API说明\n");
    printf("3. Rockchip官方文档 - RGA/NPU/DRM的具体API\n");
    
    return 0;
}
