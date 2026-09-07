#pragma once

#include <stdexcept>
#include <variant>

template<typename T>
class FlexibleRef
{
  public:
    FlexibleRef(T& obj)
        : m_Ptr(&obj)
    {
    }
    FlexibleRef(const T& obj)
        : m_Ptr(&obj)
    {
    }

    FlexibleRef(T&&) = delete;
    FlexibleRef(const T&&) = delete;

    FlexibleRef(const FlexibleRef&) = default;
    FlexibleRef(FlexibleRef&&) = default;

    bool IsConst() const
    {
        return std::holds_alternative<const T*>(m_Ptr);
    }
    bool IsMutable() const
    {
        return !IsConst();
    }

    const T& Get() const
    {
        if (IsConst())
        {
            return *std::get<const T*>(m_Ptr);
        }
        else
        {
            return *std::get<T*>(m_Ptr);
        }
    }

    T& GetMutable()
    {
        if (IsConst())
        {
            throw std::runtime_error("Attempted to modify a const reference!");
        }
        return *std::get<T*>(m_Ptr);
    }

    T* TryGetMutable()
    {
        if (IsConst())
        {
            return nullptr;
        }
        return std::get<T*>(m_Ptr);
    }

    const T* operator->() const
    { return &Get(); }

  private:
    std::variant<const T*, T*> m_Ptr;
};
