# V4L2 简单示例程序 Makefile
# 这是一个最简单的V4L2视频采集示例

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = 

# 目标文件
TARGET = v4l2_sample

# 源文件
SRC = v4l2_sample.c

# 默认目标
all: $(TARGET)

# 编译目标
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# 清理
clean:
	rm -f $(TARGET) *.jpg

# 安装（可选）
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# 卸载（可选）
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# 帮助信息
help:
	@echo "V4L2 简单示例程序 - Makefile"
	@echo ""
	@echo "可用目标："
	@echo "  make          - 编译程序"
	@echo "  make clean    - 清理编译文件"
	@echo "  make install  - 安装到系统"
	@echo "  make uninstall - 从系统卸载"
	@echo "  make help     - 显示此帮助信息"
	@echo ""
	@echo "使用方法："
	@echo "  ./v4l2_sample"
	@echo ""
	@echo "说明："
	@echo "  这是一个最简单的V4L2视频采集示例程序"
	@echo "  程序会捕获10帧图像并保存为JPEG文件"

.PHONY: all clean install uninstall help
