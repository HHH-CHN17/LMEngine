#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>


#include <atomic>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <type_traits>
#include <limits>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <chrono>
#include <random>

template<typename T>
class MPMCQueue
{
public:
    using value_type = T;

private:
    struct node
    {
        std::unique_ptr<T> data;
        node* next;
        node() : next(nullptr) {}
    };

    std::atomic<node*> head;
    std::atomic<node*> tail;
    std::mutex head_mtx_;
    std::mutex tail_mtx_;

    // 限制队列长度成员
    const size_t capacity_;
    std::atomic<size_t> size_{0};
    std::condition_variable not_full_cv_;

    node* pop_head()
    {
        std::lock_guard<std::mutex> lg{head_mtx_};
        node* const old_head = head.load(std::memory_order_relaxed);

        // 读取 tail 需要 acquire，以便与 push() 中的 release 同步，确保
        // 当看到 tail != head 时，old_head->next 和 data 已就绪。
        if (old_head == tail.load(std::memory_order_acquire))
        {
            return nullptr;
        }
        size_.fetch_sub(1, std::memory_order_relaxed);
        head.store(old_head->next, std::memory_order_release);
        not_full_cv_.notify_one();
        return old_head;
    }

public:
    // capacity 默认为无穷大（即不限制）
    explicit MPMCQueue(size_t capacity = std::numeric_limits<size_t>::max())
        : head(new node),
          tail(head.load(std::memory_order_relaxed)),
          capacity_(capacity)
    {}

    MPMCQueue(const MPMCQueue& other) = delete;
    MPMCQueue& operator=(const MPMCQueue& other) = delete;

    ~MPMCQueue()
    {
        // 假设在析构时没有并发访问（否则就是 UB）
        while (node* const old_head = head.load(std::memory_order_relaxed))
        {
            head.store(old_head->next, std::memory_order_relaxed);
            delete old_head;
        }
    }

    std::unique_ptr<T> pop()
    {
        node* old_head = pop_head();
        if (!old_head)
        {
            return std::unique_ptr<T>{};
        }
        std::unique_ptr<T> res{std::move(old_head->data)};
        delete old_head;
        return res;
    }

    template<typename U,
             std::enable_if_t<
                 std::is_same_v<value_type, std::decay_t<U>>,
             int> = 0
    >
    bool push(U&& new_value, bool block)
    {
        std::unique_ptr<T> new_data = std::make_unique<T>(std::forward<U>(new_value));
        node* p = new node;
        {
            std::unique_lock<std::mutex> ulock{tail_mtx_};

            if (block)
            {
                // 阻塞：使用条件变量，不用自旋
                not_full_cv_.wait(ulock, [&]{
                    return size_.load(std::memory_order_acquire) < capacity_;
                });
            }
            else
            {
                // 非阻塞：如果满了就直接返回（不插入）
                if (size_.load(std::memory_order_relaxed) >= capacity_)
                    return false;
            }
            node* const old_tail = tail.load(std::memory_order_relaxed);
            old_tail->data.swap(new_data);
            old_tail->next = p;

            size_.fetch_add(1, std::memory_order_relaxed);
            tail.store(p, std::memory_order_release);

        }
        return true;
    }

    // 方便查询当前大小（注意：只是近似值，瞬时）
    size_t size() const noexcept
    {
        return size_.load(std::memory_order_acquire);
    }

    bool empty() const noexcept
    {
        return size() == 0;
    }
};



inline void test_queue_basic(size_t num_producers,
                      size_t num_consumers,
                      size_t items_per_producer,
                      size_t queue_capacity)
{
    MPMCQueue<int> q(queue_capacity);

    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};
    std::atomic<bool> producers_done{false};

    // 消费者
    auto consumer = [&] {
        size_t local = 0;
        while (!producers_done.load(std::memory_order_acquire) || !q.empty()) {
            auto item = q.pop();
            if (item) {
                ++local;
            } else {
                std::this_thread::yield();
            }
        }
        consumed.fetch_add(local);
    };

    // 生产者
    auto producer = [&](int id) {
        for (size_t i = 0; i < items_per_producer; ++i) {
            int value = id * 1000000 + static_cast<int>(i);
            bool ok = q.push(value, true);
            if (ok) {
                produced.fetch_add(1);
            }
        }
    };

    // 启动消费者
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < num_consumers; ++i)
        consumers.emplace_back(consumer);

    // 启动生产者
    std::vector<std::thread> producers;
    for (size_t i = 0; i < num_producers; ++i)
        producers.emplace_back(producer, static_cast<int>(i));

    for (auto& t : producers) t.join();
    producers_done.store(true, std::memory_order_release);
    for (auto& t : consumers) t.join();

    // 打印结果
    std::cout << "Produced: " << produced
              << "  Consumed: " << consumed
              << "  Capacity: " << queue_capacity << "\n";

    // 验证
    assert(produced == num_producers * items_per_producer);
    assert(consumed == produced);
    assert(q.empty());

    std::cout << "Test passed.\n";
}

/*test_queue_basic(4, 4, 10000, 1000);
test_queue_basic(2, 6, 5000, 50);
test_queue_basic(8, 2, 2000, 500);*/
