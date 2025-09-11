#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <new>
#include <numeric>
#include <thread>

class SPSCRingBuffer
{
public:
    /**
     * @brief 构造一个环形缓冲区。
     * @param capacity 缓冲区的总容量（字节）。
     */
    explicit SPSCRingBuffer(size_t capacity)
        : capacity_(capacity),
          buffer_(capacity),
          size_(0),
          head_idx_(0),
          tail_idx_(0)
    {
    }

    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    /**
     * @brief [生产者线程调用] 向缓冲区写入数据（非阻塞）。
     * @param data 指向要写入数据的指针。
     * @param bytes 要写入的字节数。
     * @return 实际写入的字节数（可能小于请求数，如果缓冲区空间不足）。
     */
    [[nodiscard]] size_t write(const char* data, size_t bytes) noexcept
    {
        // 1. 基于原子计数器 size_ 计算可用空间
        // 使用 relaxed 是安全的，因为只有生产者在 write 中会增加 size，
        // 我们不需要立即看到消费者减少 size 的最新结果，只写入当前已知的可用空间即可。
        const size_t current_size = size_.load(std::memory_order_relaxed);
        const size_t free_space = capacity_ - current_size;
        const size_t bytes_to_write = std::min(bytes, free_space);

        if (bytes_to_write == 0) {
            return 0;
        }

        // 2. 获取当前头部逻辑索引并计算物理索引
        const size_t current_head = head_idx_.load(std::memory_order_relaxed);
        const size_t head_idx = current_head % capacity_;

        // 3. 拷贝数据（可能因回绕而分两次）
        size_t part1_size = std::min(bytes_to_write, capacity_ - head_idx);
        memcpy(buffer_.data() + head_idx, data, part1_size);

        if (bytes_to_write > part1_size) {
            size_t part2_size = bytes_to_write - part1_size;
            memcpy(buffer_.data(), data + part1_size, part2_size);
        }

        size_.fetch_add(bytes_to_write, std::memory_order_relaxed);
        head_idx_.store(current_head + bytes_to_write, std::memory_order_release);

        return bytes_to_write;
    }

    /**
     * @brief [消费者线程调用] 从缓冲区读取数据。
     * @param data 指向用于存放读取数据的缓冲区的指针。
     * @param bytes 期望读取的字节数。
     * @return 实际读取的字节数。如果可用数据少于`bytes`，则返回0。
     */
    [[nodiscard]] size_t read(char* data, size_t bytes) noexcept
    {
        // 1. 获取头部索引 (Acquire语义)
        // 这是关键的同步点：确保能看到生产者 release 的 head_idx_ 更新，
        // 从而保证能看到最新的数据。
        const size_t current_head = head_idx_.load(std::memory_order_acquire);
        const size_t current_tail = tail_idx_.load(std::memory_order_relaxed);

		// 注意无符号数的减法：a - b = (a + b的补数) % 2^n，其中b的补数=2^n - b
		// 无符号整数减法通过其固有的回绕特性，可以准确地计算出了两者之间的逻辑距离！
		// 不过前提是缓冲区的容量 capacity_ 必须小于或等于无符号索引类型最大值的一半。原因见：【无锁数据结构：C++实战】
        const size_t bytes_available = current_head - current_tail;
        if (bytes_available < bytes) {
            return 0; // 数据不足，不进行部分读取
        }

        // 2. 计算物理索引
        const size_t tail_idx = current_tail % capacity_;

        // 3. 拷贝数据（可能因回绕而分两次）
        size_t part1_size = std::min(bytes, capacity_ - tail_idx);
        memcpy(data, buffer_.data() + tail_idx, part1_size);

        if (bytes > part1_size) {
            size_t part2_size = bytes - part1_size;
            memcpy(data + part1_size, buffer_.data(), part2_size);
        }

        // 4. 更新尾部逻辑索引 (Release语义)
        // 确保生产者在下一次 write 时能看到这部分空间已被释放
        tail_idx_.store(current_tail + bytes, std::memory_order_release);

        // 5. 减少队列大小
        size_.fetch_sub(bytes, std::memory_order_relaxed);

        return bytes;
    }

    size_t size() const noexcept
    {
        return size_.load(std::memory_order_relaxed);
	}

private:
    const size_t capacity_;
    std::vector<char> buffer_;

    // 使用独立的原子 size 计数器，避免索引回绕问题
    std::atomic<size_t> size_;

    // 要把环形数组想象成一条蛇
    std::atomic<size_t> head_idx_;	// 写入索引，队列的逻辑尾部
    std::atomic<size_t> tail_idx_;	// 读取索引，逻辑头部
};


/**
 * @brief 对SPSC环形字节缓冲区进行并发稳定性和数据完整性测试。
 */
inline void test_ring_buffer_stability()
{
    std::cout << "\n--- Running SPSCRingBuffer Stability Test ---" << std::endl;

    const size_t BUFFER_CAPACITY = 1024 * 16;      // 16 KB 缓冲区
    const size_t TOTAL_DATA_SIZE = 1024 * 1024 * 10; // 10 MB 数据总量

    SPSCRingBuffer buffer(BUFFER_CAPACITY);

    // 准备源数据 (0, 1, 2, ..., 255, 0, ...)
    std::vector<char> source_data(TOTAL_DATA_SIZE);
    std::iota(source_data.begin(), source_data.end(), 0);

    // 准备目标缓冲区
    std::vector<char> dest_data(TOTAL_DATA_SIZE);

    // 生产者线程
    std::thread producer([&]() {
        size_t bytes_written = 0;
        while (bytes_written < TOTAL_DATA_SIZE) {
            size_t written = buffer.write(source_data.data() + bytes_written, TOTAL_DATA_SIZE - bytes_written);
            bytes_written += written;
        }
    });

    // 消费者线程
    std::thread consumer([&]() {
        size_t bytes_read = 0;
        while (bytes_read < TOTAL_DATA_SIZE) {
            // 我们要求一次性读取，所以如果read返回0，就继续尝试
            size_t read_bytes = buffer.read(dest_data.data() + bytes_read, 256); // 尝试一次读256字节
            if(read_bytes > 0) {
                 bytes_read += read_bytes;
            } else {
                 std::this_thread::yield(); // 如果没读到数据，让出CPU
            }
        }
    });

    producer.join();
    consumer.join();

    // 验证结果
    std::cout << "Verifying results..." << std::endl;
    assert(source_data == dest_data);
    std::cout << "  [OK] Data integrity test passed." << std::endl;
    std::cout << "Test PASSED!" << std::endl;
}
