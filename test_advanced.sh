#!/bin/bash
# test_advanced.sh - 高级版本测试脚本

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 配置参数
NUM_WRITERS=3
MSGS_PER_WRITER=20
NUM_READERS=2
READER_DURATION=15  # 读者运行15秒

# 计算总消息数
TOTAL_MSGS=$((NUM_WRITERS * MSGS_PER_WRITER))

echo -e "${MAGENTA}========== Advanced Version Test ==========${NC}"
echo -e "Test: Multiple writers (${NUM_WRITERS}), multiple readers (${NUM_READERS})"
echo -e "Each writer writes ${MSGS_PER_WRITER} messages (total ${TOTAL_MSGS} messages)"
echo -e "Readers will run for ${READER_DURATION} seconds"
echo -e "Features: Mutex protection, blocking when full, priority order"
echo ""

# 检查设备
if [ ! -c /dev/queue ]; then
    echo -e "${RED}Device /dev/queue not found!${NC}"
    echo "Please load the kernel module first:"
    echo "  sudo insmod msgqueue.ko"
    echo "  sudo mknod /dev/queue c \$(dmesg | grep 'major number' | tail -1 | grep -o '[0-9]\\+') 0"
    echo "  sudo chmod 666 /dev/queue"
    exit 1
fi

# 完全清空队列
echo -e "${BLUE}Step 1: Clearing existing messages...${NC}"
while true; do
    # 尝试非阻塞读取
    output=$(./continuous_reader 1 0 2>&1)
    if echo "$output" | grep -q "Read 0 messages"; then
        break
    fi
done
echo -e "${GREEN}Queue cleared.${NC}"
echo ""

# 编译测试程序（如果需要）
echo -e "${BLUE}Step 2: Building test programs...${NC}"
gcc -o continuous_reader continuous_reader.c -Wall
gcc -o continuous_writer continuous_writer.c -Wall
if [ $? -ne 0 ]; then
    echo -e "${RED}Compilation failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Build complete.${NC}"
echo ""

# 启动读者（后台运行）
echo -e "${BLUE}Step 3: Starting ${NUM_READERS} readers (will run for ${READER_DURATION} seconds)...${NC}"
READER_PIDS=()
for i in $(seq 1 $NUM_READERS); do
    ./continuous_reader $READER_DURATION $i &
    READER_PIDS+=($!)
    echo "  Reader $i started (PID: ${READER_PIDS[-1]})"
done

# 等待读者进入等待状态
sleep 1

# 启动写者（后台运行）
echo -e "\n${BLUE}Step 4: Starting ${NUM_WRITERS} writers (each writes ${MSGS_PER_WRITER} messages)...${NC}"
WRITER_PIDS=()
for i in $(seq 1 $NUM_WRITERS); do
    ./continuous_writer $MSGS_PER_WRITER $i &
    WRITER_PIDS+=($!)
    echo "  Writer $i started (PID: ${WRITER_PIDS[-1]})"
done

# 等待所有写者完成
echo -e "\n${BLUE}Step 5: Waiting for writers to complete...${NC}"
for pid in "${WRITER_PIDS[@]}"; do
    wait $pid
    echo "  Writer $pid finished"
done

echo -e "\n${GREEN}All writers finished. Waiting for readers to finish...${NC}"

# 等待读者完成（剩余时间）
for pid in "${READER_PIDS[@]}"; do
    wait $pid
    echo "  Reader $pid finished"
done

# 等待1秒，确保所有消息都被处理
sleep 1

# 检查是否还有残留消息
echo -e "\n${BLUE}Step 6: Checking for remaining messages...${NC}"
REMAINING=0
for i in {1..20}; do
    output=$(./continuous_reader 1 0 2>&1)
    if echo "$output" | grep -q "Read 0 messages"; then
        break
    fi
    if echo "$output" | grep -q "Read [1-9]"; then
        REMAINING=$((REMAINING + 1))
    fi
done

if [ $REMAINING -eq 0 ]; then
    echo -e "${GREEN}No remaining messages in queue.${NC}"
else
    echo -e "${YELLOW}Warning: $REMAINING messages still in queue.${NC}"
fi

# 总结
echo -e "\n${MAGENTA}========== Test Summary ==========${NC}"
echo -e "Total messages expected: ${TOTAL_MSGS}"
echo -e "Readers ran for: ${READER_DURATION} seconds"
echo -e "All messages should have been consumed by readers."

echo -e "\n${MAGENTA}========== Advanced Version Test Complete ==========${NC}"