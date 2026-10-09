#pragma once

#include <atomic>
#include <memory>
#include <mutex>

template<class T>
class LeakySingleton
{
  public:
    LeakySingleton() = default;

    LeakySingleton(const LeakySingleton&) = delete;
    LeakySingleton& operator=(const LeakySingleton&) = delete;

    LeakySingleton(LeakySingleton&&) = delete;
    LeakySingleton& operator=(LeakySingleton&&) = delete;

    template<class... ArgsT>
    static auto Get(ArgsT&&... args)
        -> std::atomic<std::shared_ptr<decltype(T{ std::forward<ArgsT>(args)... })>>
    {
        auto ptr{ m_Instance.load().lock() };
        if (ptr != nullptr)
        {
            return ptr;
        }

        std::lock_guard lock{ m_CreationMutex };
        ptr = m_Instance.load().lock();
        if (ptr != nullptr)
        {
            return ptr;
        }

        std::shared_ptr<T> new_instance{ new T{ std::forward<ArgsT>(args)... } };
        m_Instance.store(new_instance);
        return new_instance;
    }

  private:
    inline static std::atomic<std::weak_ptr<T>> m_Instance;
    static inline std::mutex m_CreationMutex;
};
