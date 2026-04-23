# V4L2 简单示例程序 Makefile
# 支持MMAP和DMA_BUF两种缓冲区管理方式

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = 

# 目标文件
TARGET_MMAP = v4l2_sample
TARGET_DMA = v4l2_dma_buf
TARGET_EXAMPLE = dmabuf_usage_example

# 源文件
SRC_MMAP = v4l2_sample.c
SRC_DMA = v4l2_dma_buf.c
SRC_EXAMPLE = dmabuf_usage_example.c

# 默认目标
all: $(TARGET_MMAP) $(TARGET_DMA)

# 编译MMAP版本
$(TARGET_MMAP): $(SRC_MMAP)
	$(CC) $(CFLAGS) -o $(TARGET_MMAP) $(SRC_MMAP) $(LDFLAGS)

# 编译DMA_BUF版本
$(TARGET_DMA): $(SRC_DMA)
	$(CC) $(CFLAGS) -o $(TARGET_DMA) $(SRC_DMA) $(LDFLAGS)

# 编译DMA-BUF使用示例（需要指定使用的硬件）
example: $(SRC_EXAMPLE)
	@echo "编译DMA-BUF使用示例..."
	@echo "请选择要启用的功能："
	@echo "  make example_rga   - 启用RGA支持"
	@echo "  make example_npu   - 启用NPU支持"
	@echo "  make example_drm   - 启用DRM支持"
	@echo "  make example_all   - 启用所有功能"

example_rga: $(SRC_EXAMPLE)
	$(CC) $(CFLAGS) -DUSE_RGA -o $(TARGET_EXAMPLE) $(SRC_EXAMPLE) $(LDFLAGS) -lrga

example_npu: $(SRC_EXAMPLE)
	$(CC) $(CFLAGS) -DUSE_RKNN -o $(TARGET_EXAMPLE) $(SRC_EXAMPLE) $(LDFLAGS) -lrknnrt

example_drm: $(SRC_EXAMPLE)
	$(CC) $(CFLAGS) -DUSE_DRM -o $(TARGET_EXAMPLE) $(SRC_EXAMPLE) $(LDFLAGS) -ldrm

example_all: $(SRC_EXAMPLE)
	$(CC) $(CFLAGS) -DUSE_RGA -DUSE_RKNN -DUSE_DRM -o $(TARGET_EXAMPLE) $(SRC_EXAMPLE) $(LDFLAGS) -lrga -lrknnrt -ldrm

# 单独编译目标
mmap: $(TARGET_MMAP)

dma_buf: $(TARGET_DMA)

# 清理
clean:
	rm -f $(TARGET_MMAP) $(TARGET_DMA) $(TARGET_EXAMPLE) *.jpg

# 安装（可选）
install: $(TARGET_MMAP) $(TARGET_DMA)
	cp $(TARGET_MMAP) /usr/local/bin/
	cp $(TARGET_DMA) /usr/local/bin/

# 卸载（可选）
uninstall:
	rm -f /usr/local/bin/$(TARGET_MMAP)
	rm -f /usr/local/bin/$(TARGET_DMA)

# 帮助信息
help:
	@echo "V4L2 视频采集程序 - Makefile"
	@echo ""
	@echo "可用目标："
	@echo "  make          - 编译所有程序（MMAP + DMA_BUF）"
	@echo "  make mmap     - 仅编译MMAP版本 (v4l2_sample)"
	@echo "  make dma_buf  - 仅编译DMA_BUF版本 (v4l2_dma_buf)"
	@echo "  make example_rga   - 编译RGA示例 (需要librga)"
	@echo "  make example_npu   - 编译NPU示例 (需要librknnrt)"
	@echo "  make example_drm   - 编译DRM示例 (需要libdrm)"
	@echo "  make example_all   - 编译完整示例 (需要所有库)"
	@echo "  make clean    - 清理编译文件"
	@echo "  make install  - 安装到系统"
	@echo "  make uninstall - 从系统卸载"
	@echo "  make help     - 显示此帮助信息"
	@echo ""
	@echo "使用方法："
	@echo "  ./v4l2_sample      # MMAP方式采集"
	@echo "  ./v4l2_dma_buf     # DMA_BUF方式采集（持续循环，按Ctrl+C退出）"
	@echo "  ./dmabuf_usage_example  # DMA-BUF使用示例"
	@echo ""
	@echo "说明："
	@echo "  v4l2_sample:   使用内存映射(MMAP)方式的V4L2视频采集示例"
	@echo "  v4l2_dma_buf:  使用MMAP+EXPBUF方式，导出DMA-BUF fd用于零拷贝"
	@echo "                 支持持续采集，导出的fd可用于RGA/NPU/DRM等硬件"

.PHONY: all clean install uninstall help mmap dma_buf example example_rga example_npu example_drm example_all
