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
// x86 - 64��Ϊ64�ֽ� �� L1_CACHE_BYTES �� L1_CACHE_SHIFT �� __cacheline_aligned ��...
// L1�����д�Сͨ����64�ֽ�
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif


class SpscRingBuffer
{
public:
    /**
     * @brief ����һ�����λ�������
     * @param capacity �����������������ֽڣ����ᱻ����Ϊ2�������Ż�ģ���㡣
     */
    explicit SpscRingBuffer(size_t capacity)
        : capacity_(next_power_of_2(capacity)), // ʹ��2���ݴ�����
        mask_(capacity_ - 1),              // ����λ�������ȡģ
        buffer_(capacity_)
    {
    }

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    /**
     * @brief [�������̵߳���] �򻺳���д�����ݡ�
     * @param data ָ��Ҫд�����ݵ�ָ�롣
     * @param bytes Ҫд����ֽ�����
     * @return ʵ��д����ֽ���������С��������������������ռ䲻�㣩��
     */
    [[nodiscard]] size_t write(const char* data, size_t bytes) noexcept
    {
        // ʹ�� memory_order_relaxed����Ϊֻ���������̻߳��޸� head_
        const size_t current_head = head_.load(std::memory_order_relaxed);
        // ʹ�� memory_order_acquire��ȷ���ܿ����������̶߳� tail_ �������޸�
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        const size_t free_space = capacity_ - (current_head - current_tail);
        const size_t bytes_to_write = std::min(bytes, free_space);

        if (bytes_to_write == 0) {
            return 0;
        }

        // ���߼�����ת��Ϊ��������
        // ʹ��λ���������ȡģ����Ϊ������2����
        size_t head_idx = current_head & mask_;

        // ���ݿ�����Ҫ�����ο�������д������Խ������������ĩβʱ��
        size_t part1_size = std::min(bytes_to_write, capacity_ - head_idx);
        memcpy(buffer_.data() + head_idx, data, part1_size);

        if (bytes_to_write > part1_size) {
            size_t part2_size = bytes_to_write - part1_size;
            memcpy(buffer_.data(), data + part1_size, part2_size);
        }

        // ʹ�� memory_order_release��ȷ���ڸ��� head_ ֮ǰ��
        // ����� memcpy ���������������̶߳��ɼ���
        // ���������ߺ�������֮���ͬ���㡣
        head_.store(current_head + bytes_to_write, std::memory_order_release);

        return bytes_to_write;
    }

    /**
     * @brief [�������̵߳���] �ӻ�������ȡ���ݡ�
     * @param data ָ�����ڴ�Ŷ�ȡ���ݵĻ�������ָ�롣
     * @param bytes Ҫ��ȡ���ֽ�����
     * @return ʵ�ʶ�ȡ���ֽ����������bytes��ȣ�����return 0����
     */
    [[nodiscard]] size_t read(char* data, size_t bytes) noexcept
    {
        // ʹ�� memory_order_acquire��ȷ���ܿ����������̶߳� head_ �������޸�
        const size_t current_head = head_.load(std::memory_order_acquire);
        // ʹ�� memory_order_relaxed����Ϊֻ���������̻߳��޸� tail_
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

        // ʹ�� memory_order_release��ȷ���ڸ��� tail_ ֮ǰ��
        // �����̣߳��������ߣ��ܿ������ռ��Ѿ����ͷš�
        tail_.store(current_tail + bytes_to_read, std::memory_order_release);

        return bytes_to_read;
    }

    // ���ص�ǰ�ɶ���������
    [[nodiscard]] size_t get_size() const noexcept {
        return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
    }

    // ���ص�ǰ��д�Ŀ���ռ�
    [[nodiscard]] size_t get_free_space() const noexcept {
        return capacity_ - get_size();
    }

    // ����������
    [[nodiscard]] size_t get_capacity() const noexcept {
        return capacity_;
    }

private:
    // ����������������ڵ���v����С��2����
    static size_t next_power_of_2(size_t v) {
        // �����Ѿ���2���ݵ�����ֱ�ӷ���
        if (v > 0 && (v & (v - 1)) == 0) {
            return v;
        }
        // �����ҵ���һ��2����
        size_t p = 1;
        while (p < v) {
            p <<= 1;
        }
        return p;
    }

private:
    // C++17 ��׼�����ڱ���α����ĳ���
    static constexpr size_t CACHELINE_SIZE = hardware_destructive_interference_size;

    // �����ߺ��������ڲ�ͬ�߳����޸ģ�ʹ�� alignas ����α����
    alignas(CACHELINE_SIZE) std::atomic<size_t> head_ = { 0 };
    alignas(CACHELINE_SIZE) std::atomic<size_t> tail_ = { 0 };

    const size_t capacity_;
    const size_t mask_; // ����λ�������ȡģ��mask = capacity - 1

    std::vector<char> buffer_;
};
