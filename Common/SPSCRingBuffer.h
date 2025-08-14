#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <algorithm> 
#include <cstring>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
// x86 - 64上为64字节 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │...
// L1缓存行大小通常是64字节
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif


class SpscRingBuffer
{
public:
    /**
     * @brief 构造一个环形缓冲区。
     * @param capacity 缓冲区的总容量（字节），会被调整为2的幂以优化模运算。
     */
    explicit SpscRingBuffer(size_t capacity)
        : capacity_(next_power_of_2(capacity)), // 使用2的幂次容量
        mask_(capacity_ - 1),              // 用于位运算代替取模
        buffer_(capacity_)
    {
    }

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    /**
     * @brief [生产者线程调用] 向缓冲区写入数据。
     * @param data 指向要写入数据的指针。
     * @param bytes 要写入的字节数。
     * @return 实际写入的字节数（可能小于请求数，如果缓冲区空间不足）。
     */
    [[nodiscard]] size_t write(const char* data, size_t bytes) noexcept
    {
        // 使用 memory_order_relaxed，因为只有生产者线程会修改 head_
        const size_t current_head = head_.load(std::memory_order_relaxed);
        // 使用 memory_order_acquire，确保能看到消费者线程对 tail_ 的最新修改
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        const size_t free_space = capacity_ - (current_head - current_tail);
        const size_t bytes_to_write = std::min(bytes, free_space);

        if (bytes_to_write == 0) {
            return 0;
        }

        // 将逻辑索引转换为物理索引
        // 使用位与操作代替取模，因为容量是2的幂
        size_t head_idx = current_head & mask_;

        // 数据可能需要分两次拷贝（当写操作跨越了物理缓冲区的末尾时）
        size_t part1_size = std::min(bytes_to_write, capacity_ - head_idx);
        memcpy(buffer_.data() + head_idx, data, part1_size);

        if (bytes_to_write > part1_size) {
            size_t part2_size = bytes_to_write - part1_size;
            memcpy(buffer_.data(), data + part1_size, part2_size);
        }

        // 使用 memory_order_release，确保在更新 head_ 之前，
        // 上面的 memcpy 操作对所有其他线程都可见。
        // 这是生产者和消费者之间的同步点。
        head_.store(current_head + bytes_to_write, std::memory_order_release);

        return bytes_to_write;
    }

    /**
     * @brief [消费者线程调用] 从缓冲区读取数据。
     * @param data 指向用于存放读取数据的缓冲区的指针。
     * @param bytes 要读取的字节数。
     * @return 实际读取的字节数（必须和bytes相等，否则return 0）。
     */
    [[nodiscard]] size_t read(char* data, size_t bytes) noexcept
    {
        // 使用 memory_order_acquire，确保能看到生产者线程对 head_ 的最新修改
        const size_t current_head = head_.load(std::memory_order_acquire);
        // 使用 memory_order_relaxed，因为只有消费者线程会修改 tail_
        const size_t current_tail = tail_.load(std::memory_order_relaxed);

        const size_t bytes_available = current_head - current_tail;
        const size_t bytes_to_read = std::min(bytes, bytes_available);

        if (bytes_to_read != bytes) {
            return 0;
        }

        size_t tail_idx = current_tail & mask_;

        size_t part1_size = std::min(bytes_to_read, capacity_ - tail_idx);
        memcpy(data, buffer_.data() + tail_idx, part1_size);

        if (bytes_to_read > part1_size) {
            size_t part2_size = bytes_to_read - part1_size;
            memcpy(data + part1_size, buffer_.data(), part2_size);
        }

        // 使用 memory_order_release，确保在更新 tail_ 之前，
        // 其他线程（即生产者）能看到这块空间已经被释放。
        tail_.store(current_tail + bytes_to_read, std::memory_order_release);

        return bytes_to_read;
    }

    // 返回当前可读的数据量
    [[nodiscard]] size_t get_size() const noexcept {
        return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
    }

    // 返回当前可写的空余空间
    [[nodiscard]] size_t get_free_space() const noexcept {
        return capacity_ - get_size();
    }

    // 返回总容量
    [[nodiscard]] size_t get_capacity() const noexcept {
        return capacity_;
    }

private:
    // 辅助函数，计算大于等于v的最小的2的幂
    static size_t next_power_of_2(size_t v) {
        // 对于已经是2的幂的数，直接返回
        if (v > 0 && (v & (v - 1)) == 0) {
            return v;
        }
        // 否则，找到下一个2的幂
        size_t p = 1;
        while (p < v) {
            p <<= 1;
        }
        return p;
    }

private:
    // C++17 标准中用于避免伪共享的常量
    static constexpr size_t CACHELINE_SIZE = hardware_destructive_interference_size;

    // 生产者和消费者在不同线程上修改，使用 alignas 避免伪共享
    alignas(CACHELINE_SIZE) std::atomic<size_t> head_ = { 0 };
    alignas(CACHELINE_SIZE) std::atomic<size_t> tail_ = { 0 };

    const size_t capacity_;
    const size_t mask_; // 用于位运算代替取模，mask = capacity - 1

    std::vector<char> buffer_;
};
