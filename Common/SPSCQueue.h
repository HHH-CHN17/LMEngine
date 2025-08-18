#pragma once

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <ostream>
#include <thread>

using namespace std;

template<typename T>
class SPSCQueue
{
    // 注意，我们期望push时按值传递，pop时按引用返回，所以value_type必须是T的退化类型
    using value_type = std::decay_t<T>;
private:
    struct node
    {
        std::unique_ptr<value_type> data_;
        node* next_;
        node() :
            next_(nullptr)
        {}
    };
    std::atomic<node*> head_;
    std::atomic<node*> tail_;
    const size_t capacity_;
    std::atomic<size_t> size_;
private:
    node* pop_head()
    {
        node* const old_head = head_.load(std::memory_order_relaxed);
        // 这里从 tail 读取需要 acquire，以同步 push 中对 node->data / node->next 的写
        if (old_head == tail_.load(std::memory_order_acquire))
        {
            return nullptr;
        }
        size_.fetch_sub(1, std::memory_order_relaxed);
        head_.store(old_head->next_, std::memory_order_release);
        return old_head;
    }

public:
    explicit SPSCQueue(size_t capacity) :
        head_(new node),
        tail_(head_.load(std::memory_order_relaxed)),
        size_(0),
        capacity_(capacity)
    {}

    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;

    ~SPSCQueue()
    {
        // 析构假设没有并发访问（否则就是 UB），使用 relaxed 足够
        while (node* const old_head = head_.load(std::memory_order_relaxed))
        {
            head_.store(old_head->next_, std::memory_order_relaxed);
            delete old_head;
        }
    }

    // pop函数在队列空时直接返回nullptr，非阻塞
    std::unique_ptr<value_type> pop()
    {
        node* old_head = pop_head();
        if (!old_head)
        {
            return std::unique_ptr<value_type>{};
        }

        std::unique_ptr<value_type> res{std::move(old_head->data_)};
        delete old_head;
        return res;
    }

    // 这里必须要增加一个函数模板形参U
    // 如果使用 T，由于T在实例化类模板时已经确定
    // 所以在调用push时，T&&不是万能引用
    template<typename U,
        std::enable_if_t<
            std::is_same_v<value_type, std::decay_t<U>>,    // 退化U应当和value_type类型相同
        int> = 0
    >
    bool push(U&& new_value, bool block)
    {
        std::unique_ptr<value_type> new_data = std::make_unique<value_type>(std::forward<U>(new_value));
        node* p = new node;

        if (size_.load(std::memory_order_relaxed) >= capacity_) {
            if (!block) {
                // 非阻塞模式，队列已满，立即返回失败
                return false;
            }
            // 阻塞模式，自旋等待直到有空间
            while (size_.load(std::memory_order_relaxed) >= capacity_) {
                std::this_thread::yield();
            }
        }

        node* const old_tail = tail_.load(std::memory_order_relaxed);
        old_tail->data_.swap(new_data);
        old_tail->next_ = p;

        size_.fetch_add(1, std::memory_order_relaxed);
        tail_.store(p, std::memory_order_release);
        return true;
    }

    size_t size() const
    {
        return size_.load(std::memory_order_relaxed);
    }
};

inline void test_spsc_queue_stability(const size_t queue_capacity, const size_t num_items)
{
    std::cout << "\n--- Running SPSC Stability Test ---\n"
              << "Queue Capacity: " << queue_capacity << "\n"
              << "Items to Process: " << num_items << "\n"
              << "---------------------------------" << std::endl;

    SPSCQueue<int> q(queue_capacity);

    // 生产者线程
    std::thread producer([&]() {
        for (size_t i = 0; i < num_items; ++i) {
            q.push(static_cast<int>(i), true);
        }
    });

    // 消费者线程
    std::vector<int> collected_items;
    collected_items.reserve(num_items);
    std::thread consumer([&]() {
        for (size_t i = 0; i < num_items; ++i) {
            std::unique_ptr<int> item;
            while (!(item = q.pop())) {
                std::this_thread::yield();
            }
            collected_items.push_back(*item);
        }
    });

    producer.join();
    consumer.join();

    // --- 验证结果 ---
    std::cout << "Verifying results..." << std::endl;
    assert(collected_items.size() == num_items);
    assert(q.size() == 0);

    bool integrity_ok = true;
    for (size_t i = 0; i < num_items; ++i) {
        if (collected_items[i] != static_cast<int>(i)) {
            integrity_ok = false;
            break;
        }
    }
    assert(integrity_ok);

    std::cout << "Test PASSED!" << std::endl;
}

/*
    test_spsc_queue_stability(100, 1000000); // 高竞争场景
    test_spsc_queue_stability(10000, 100000); // 低竞争场景

    std::cout << "\nAll test scenarios passed successfully!" << std::endl;*/
