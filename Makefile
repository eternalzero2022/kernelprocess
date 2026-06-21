# ============================================
# 内核模块配置
# ============================================
obj-m := message_queue.o

# 设备名称（统一）
DEVICE_NAME := queue
DEVICE_PATH := /dev/$(DEVICE_NAME)

# ============================================
# 测试程序编译配置
# ============================================
CC = gcc
CFLAGS = -Wall -Wextra
TEST_PROGRAMS = test_simple test_normal reader writer continuous_reader continuous_writer

# ============================================
# 目标
# ============================================

# 默认目标：编译内核模块
all:
	@echo "Building kernel module..."
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
	@echo "Done. Use 'make test' to build test programs."

# 编译测试程序
test: $(TEST_PROGRAMS)
	@echo "Test programs built."

test_simple: test_simple.c
	$(CC) $(CFLAGS) -o $@ $<

test_normal: test_normal.c
	$(CC) $(CFLAGS) -o $@ $<

reader: reader.c
	$(CC) $(CFLAGS) -o $@ $<

writer: writer.c
	$(CC) $(CFLAGS) -o $@ $<

continuous_reader: continuous_reader.c
	$(CC) $(CFLAGS) -o $@ $<

continuous_writer: continuous_writer.c
	$(CC) $(CFLAGS) -o $@ $<

# ============================================
# 模块管理
# ============================================

# 加载模块
install:
	@echo "Loading module..."
	sudo insmod message_queue.ko
	@echo "Module loaded. Use 'make mknod' to create device file."

# 卸载模块
remove:
	@echo "Unloading module..."
	-sudo rmmod message_queue
	@echo "Module unloaded."

# 重新加载（卸载 → 加载）
reload: remove install
	@echo "Module reloaded."

# ============================================
# 设备文件管理
# ============================================

# 自动获取设备号并创建设备文件
mknod:
	@echo "Creating device file..."
	@MAJOR=$$(dmesg | grep "module loaded with device major number" | tail -1 | grep -o '[0-9]\+'); \
	if [ -z "$$MAJOR" ]; then \
		echo "Error: Cannot find major number. Is module loaded?"; \
		echo "Run 'make install' first."; \
		exit 1; \
	fi; \
	echo "Using major number: $$MAJOR"; \
	sudo mknod $(DEVICE_PATH) c $$MAJOR 0; \
	sudo chmod 666 $(DEVICE_PATH); \
	echo "Device created: $(DEVICE_PATH)"

# 删除设备文件
rmnod:
	@echo "Removing device file..."
	-sudo rm -f $(DEVICE_PATH)
	@echo "Device removed."

# 一键加载（模块 + 设备）
load: install mknod
	@echo "Module loaded and device created."

# 一键卸载（模块 + 设备）
unload: rmnod remove
	@echo "Module unloaded and device removed."

# ============================================
# 测试运行
# ============================================

# 运行简单版本测试
test-simple: test_simple
	@echo "=== Running Simple Version Test ==="
	./test_simple

# 运行中级版本测试
test-normal: test_normal
	@echo "=== Running Normal Version Test ==="
	./test_normal

# 运行高级版本测试（使用 shell 脚本）
test-advanced:
	@echo "=== Running Advanced Version Test ==="
	./test_advanced.sh

# 运行所有测试
test-all: test-simple test-normal test-advanced
	@echo "All tests completed."

# ============================================
# 其他工具
# ============================================

# 查看模块状态
check:
	@echo "=== Module Status ==="
	@lsmod | grep message_queue || echo "Module not loaded"
	@echo ""
	@echo "=== Device Status ==="
	@ls -l $(DEVICE_PATH) 2>/dev/null || echo "Device not found"
	@echo ""
	@echo "=== Registered Devices ==="
	@cat /proc/devices | grep message_queue || echo "Not registered"

# 查看内核日志
log:
	sudo dmesg | tail -n 10

# 清空内核日志
log-clear:
	sudo dmesg -c > /dev/null
	@echo "Kernel log cleared."

# 查看队列状态（如果支持）
status: check
	@echo ""
	@echo "=== Queue Status ==="
	@if [ -c $(DEVICE_PATH) ]; then \
		./continuous_reader 1 0 2>/dev/null || echo "Queue is empty or device not ready"; \
	else \
		echo "Device not found"; \
	fi

# ============================================
# 清理
# ============================================

clean:
	@echo "Cleaning build files..."
	rm -f $(TEST_PROGRAMS)
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
	@echo "Clean complete."

# 深度清理（包括设备文件）
clean-all: clean rmnod remove
	@echo "Full clean complete."

# ============================================
# 帮助信息
# ============================================

help:
	@echo "================================================"
	@echo "Message Queue Makefile Help"
	@echo "================================================"
	@echo ""
	@echo "Build:"
	@echo "  make              - Build kernel module"
	@echo "  make test         - Build test programs"
	@echo "  make clean        - Clean build files"
	@echo "  make clean-all    - Clean everything (including device)"
	@echo ""
	@echo "Module management:"
	@echo "  make load         - Load module + create device (一键加载)"
	@echo "  make unload       - Remove device + unload module (一键卸载)"
	@echo "  make install      - Load module only"
	@echo "  make remove       - Unload module only"
	@echo "  make reload       - Reload module"
	@echo "  make mknod        - Create device file (auto-detect major)"
	@echo "  make rmnod        - Remove device file"
	@echo "  make check        - Check module/device status"
	@echo ""
	@echo "Testing:"
	@echo "  make test-simple  - Run simple version test"
	@echo "  make test-normal  - Run normal version test"
	@echo "  make test-advanced- Run advanced version test"
	@echo "  make test-all     - Run all tests"
	@echo ""
	@echo "Debug:"
	@echo "  make log          - Show last 10 kernel messages"
	@echo "  make log-clear    - Clear kernel log"
	@echo "  make status       - Show queue status"
	@echo "================================================"

# ============================================
# 声明伪目标
# ============================================
.PHONY: all test clean clean-all \
        install remove reload \
        mknod rmnod load unload \
        check log log-clear status \
        test-simple test-normal test-advanced test-all \
        help