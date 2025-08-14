#pragma once

#include <memory>
#include <mutex>
#include <iostream>
#include <atomic>
#include <exception>

// 懒汉式单例的基类
template<typename T>    //T 是子类
class Singleton_Lazy_Base {
private:
    static void Destory(T* p_sgl) {
        delete p_sgl;
    }

    inline static std::unique_ptr<T, void(*)(T*)> up{ nullptr, Singleton_Lazy_Base<T>::Destory };
    inline static std::once_flag of{};

public:

    template<typename ...Args>
    static T& GetInstance(Args&&... args) {
        // &args...表示按引用捕获参数包
        std::call_once(of, [&args...]() {
            up.reset(new T(std::forward<Args>(args)...));
            }
        );
        return *up;
    }

    Singleton_Lazy_Base(const Singleton_Lazy_Base&) = delete;
    Singleton_Lazy_Base(Singleton_Lazy_Base&&) = delete;
    Singleton_Lazy_Base& operator=(const Singleton_Lazy_Base&) = delete;

protected:
    Singleton_Lazy_Base() = default;
    ~Singleton_Lazy_Base() { std::cout << "~Singleton" << std::endl; }
};
