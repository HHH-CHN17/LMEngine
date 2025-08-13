#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <iostream>
#include <condition_variable>
#include <thread>
#include <type_traits>

#include "OpenGLWidget/SceneManger/Object/Frame/GLFrame.h"

/*
 * 代码阅读时注意以下几个方面：
 * 1. 初始化
 * 2. push()
 * 3. pop()
 * 4. 队列满
 */

 /*
  * 后续优化需要加入内存池，原因如下：
  * 从分析结果来看：
  * 1. pop性能基本与mutex，atomic_flag保持一致，以下仅讨论push的性能问题
  * 2. 当node较小时（20bytes），lock_free_queue性能不如mutex，atomic_flag
  * 3. 当node较大（1MB），且数据长度较短时（<=200,000），lock_free_queue性能为mutex，atomic_flag的2-3倍
  * 4. 当node较大（1MB），且数据长度较长时（>200,000），lock_free_queue性能严重下滑，后者性能是前者的2倍左右
  * 5. 推测是由于内存分配导致的，所以后面需要加内存池进行优化。
  */

template<typename T, size_t LENGTH = 100>
class lock_free_queue
{
    // 注意，我们期望
    using value_type = std::decay_t<T>;
private:
    struct node;

    struct counted_node_ptr
    {
        int external_count;
        node* ptr;
        //typename MemoryPool<node, 1024>::smart_ptr ptr; counted_node_ptr 必须为 trivially copyable ，所以不能用 smart_ptr，内存安全靠无锁队列实现;
        // 这两个构造函数必须删掉，不然当T为函数指针时push()会出错，我也不知道为什么
//        counted_node_ptr() noexcept {
//            external_count = 0;
//            ptr = nullptr;
//        }
//        explicit counted_node_ptr(CountNodeInit) noexcept {
//            external_count = 1;
//            //ptr = new node{};
//            ptr = nodeMemPool_.new_element();
//        }
    };

    struct node_counter
    {
        unsigned internal_count : 30;
        unsigned external_counters : 2; // 2
    };

    struct node
    {
        std::atomic<value_type*> atpData_;
        std::atomic<node_counter> atstCount_; // 3
        std::atomic<counted_node_ptr> atstNext_;
        node() noexcept
        {
            atpData_.store(nullptr);

            node_counter new_count;
            new_count.internal_count = 0;
            new_count.external_counters = 2; // 4
            atstCount_.store(new_count);

            counted_node_ptr new_next;
            new_next.external_count = 0;
            new_next.ptr = nullptr;
            atstNext_.store(new_next);
        }
        void release_ref() noexcept
        {
            node_counter old_counter = atstCount_.load(std::memory_order_relaxed);
            node_counter new_counter;
            do
            {
                new_counter = old_counter;
                --new_counter.internal_count; // 1
            } while (!atstCount_.compare_exchange_strong(old_counter, new_counter, // 2
                std::memory_order_acquire, std::memory_order_relaxed));
            if (!new_counter.internal_count && !new_counter.external_counters)
            {
                delete this; // 3
            }
        }
    };

    std::atomic<counted_node_ptr> atHead_;
    std::atomic<counted_node_ptr> atTail_; // 1
    std::atomic<bool> atRunFlag_;
    std::atomic<size_t> nCurrLen_;

    static_assert(std::is_trivially_copyable_v<counted_node_ptr>
        && std::is_trivially_copyable_v<node_counter>
        && std::is_trivially_copyable_v<node>, "must be trivially_copyable");

private:

    static void increase_external_count(std::atomic<counted_node_ptr>& counter, counted_node_ptr& old_counter)
    {
        counted_node_ptr new_counter;
        do
        {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!counter.compare_exchange_strong(old_counter, new_counter,
            std::memory_order_acquire, std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }

    static void free_external_counter(counted_node_ptr& old_node_ptr)
    {
        node* const ptr = old_node_ptr.ptr;
        int const count_increase = old_node_ptr.external_count - 2;
        //std::cout << count_increase << std::endl;
        node_counter old_counter = ptr->atstCount_.load(std::memory_order_relaxed);
        node_counter new_counter;
        do
        {
            new_counter = old_counter;
            --new_counter.external_counters; // 1
            new_counter.internal_count += count_increase; // 2
            //std::cout << new_counter.internal_count << std::endl;
        } while (!ptr->atstCount_.compare_exchange_strong( // 3
            old_counter, new_counter,
            std::memory_order_acquire, std::memory_order_relaxed));
        if (!new_counter.internal_count && !new_counter.external_counters)
        {
            delete ptr; // 4
            //std::cout << "delete node" << std::endl;
        }
    }

    void set_new_tail(counted_node_ptr& old_tail, counted_node_ptr const& new_tail)
    {
        node* const current_tail_ptr = old_tail.ptr; // 获取数据域
        //该while仅用于防止假性失败
        while (!atTail_.compare_exchange_weak(old_tail, new_tail) && old_tail.ptr == current_tail_ptr) {

        }
        // ⇽---  3
        if (old_tail.ptr == current_tail_ptr) {
            ++nCurrLen_;// nCurrLen在push结束后加一，因为可以同时有多个线程准备改变tail指向，但只有一个线程能成功。
            free_external_counter(old_tail);
        }
        else {
            current_tail_ptr->release_ref();
        }

    }

public:
    lock_free_queue() :
        atHead_(counted_node_ptr{}), atTail_(atHead_.load()), atRunFlag_(true), nCurrLen_(0) {
        // 判断：是否需要保护数据初始化过程。

        counted_node_ptr cnpHead;
        cnpHead.external_count = 1;
        cnpHead.ptr = new node{};
        atHead_.store(cnpHead);
        atTail_.store(atHead_.load());
    }

    ~lock_free_queue() {
        atRunFlag_.store(false);
        while (pop()) {
            // do nothing
        }
        auto head_counted_node = atHead_.load();
        delete head_counted_node.ptr;
    }

    // 这里必须要增加一个函数模板形参U
	// 如果使用 T，由于T在实例化类模板时已经确定
	// 所以在调用push时，T&&不是万能引用
    template<typename U,
		std::enable_if_t<
			std::is_same_v<value_type, std::decay_t<U>>,    // 退化U应当和value_type类型相同
		int> = 0
	>
    void push(U&& new_value)
	{
        //std::cout << "pushing" << std::endl;
        if (!atRunFlag_) 
        {
            std::cout << "terminate!" << std::endl;
            return;
        }

        auto new_data = std::make_unique<value_type>(std::forward<U>(new_value));
        counted_node_ptr new_tail;
        new_tail.ptr = new node;
        new_tail.external_count = 1;
        counted_node_ptr old_tail = atTail_.load();
        for (;;)
        {
            increase_external_count(atTail_, old_tail);
            // std::cout << "pushing: extcount incount extcounter "
            //     << atTail_.load().external_count << " "
            //     << atTail_.load().ptr->atstCount_.load().internal_count << " "
            //     << atTail_.load().ptr->atstCount_.load().external_counters << std::endl;
            value_type* old_data = nullptr;

            /*  // 必须先释放 tail 的引用再等待，
                // 否则在 head == tail 的情况下，可能阻止 pop 回收节点，
				// 导致队列卡住（引用计数占用导致的阻塞，而非锁死）
            while (nCurrLen_ > LENGTH)
            {
                std::cout << "queue fill out \n";
                std::this_thread::yield();
            }*/

            if (nCurrLen_.load(std::memory_order_relaxed) >= LENGTH) 
            {
                free_external_counter(old_tail);
                // yield() 只是一个调度提示，不能保证线程进入休眠状态，OS 可能立即重新调度当前线程或者什么都不做
                std::this_thread::yield();
                // 重新psuh
                old_tail = atTail_.load(); // 重新获取tail
                continue;
            }

            //⇽---  6
            if (old_tail.ptr->atpData_.compare_exchange_strong(old_data, new_data.get()))
            {
                counted_node_ptr old_next{};
                //⇽---  7 更新tail
                if (!old_tail.ptr->atstNext_.compare_exchange_strong(old_next, new_tail))
                {
                    //⇽---  8
                    delete new_tail.ptr;
                    new_tail = old_next;   // ⇽---  9
                }
                set_new_tail(old_tail, new_tail);
                new_data.release();
                break;
            }
            else    // ⇽---  10
            {
                counted_node_ptr old_next{};
                // ⇽--- 11 协助更新 tail
                if (old_tail.ptr->atstNext_.compare_exchange_strong(old_next, new_tail))
                {
                    // ⇽--- 12
                    old_next = new_tail;
                    // ⇽---  13
                    new_tail.ptr = new node;
                }
                //  ⇽---  14
                set_new_tail(old_tail, old_next);
            }
        }
    }

    std::unique_ptr<value_type> pop()
    {
        //std::cout << "poping" << std::endl;
        counted_node_ptr old_head = atHead_.load(std::memory_order_relaxed);
        for (;;)
        {
            increase_external_count(atHead_, old_head);
            node* const ptr = old_head.ptr;
            if (ptr == atTail_.load().ptr)
            {
                return std::unique_ptr<value_type>{};
            }
            counted_node_ptr next = ptr->atstNext_.load();   //  ⇽---  2
            if (atHead_.compare_exchange_strong(old_head, next))
            {
                --nCurrLen_;
                value_type* const res = ptr->atpData_.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<value_type>{res};
            }
            ptr->release_ref();
        }
    }

    bool empty() {
        if (atTail_.load().ptr == atHead_.load().ptr)
            return true;
        return false;
    }

    bool isFull()
    {
        return nCurrLen_.load(std::memory_order_relaxed) >= LENGTH;
    }
};