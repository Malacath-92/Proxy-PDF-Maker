#pragma once

#include <atomic>
#include <type_traits>

// NOLINTBEGIN

namespace detail
{
template<typename BitFieldTy>
struct BitFieldOperatorsEnabled
{
    static constexpr bool value = false;
};

template<typename BitFieldTy>
struct AtomicBitFieldOperatorsEnabled
{
    static constexpr bool value = false;
};
} // namespace detail

// Call this on the enum class type that shall have the operators enabled
#define ENABLE_BITFIELD_OPERATORS(bitfield)           \
    template<>                                        \
    struct detail::BitFieldOperatorsEnabled<bitfield> \
    {                                                 \
        static constexpr bool value = true;           \
    }

// Call this on the enum class type that shall also overload the operators for atomics
#define ENABLE_ATOMIC_BITFIELD_OPERATORS(bitfield)          \
    template<>                                              \
    struct detail::AtomicBitFieldOperatorsEnabled<bitfield> \
    {                                                       \
        static constexpr bool value = true;                 \
    }

/*
        Operators taking two parameters and returning the same type
*/
#define MAKE_BINARY_BITFIELD_OPERATOR(op)                                                              \
    template<typename BitFieldTy>                                                                      \
    inline constexpr std::enable_if_t<detail::BitFieldOperatorsEnabled<BitFieldTy>::value, BitFieldTy> \
    operator op(const BitFieldTy & lhs, const BitFieldTy & rhs)                                        \
    {                                                                                                  \
        using BaseTy = std::underlying_type_t<BitFieldTy>;                                             \
        return static_cast<BitFieldTy>(                                                                \
            static_cast<BaseTy>(lhs) op static_cast<BaseTy>(rhs));                                     \
    }

MAKE_BINARY_BITFIELD_OPERATOR(&)
MAKE_BINARY_BITFIELD_OPERATOR(|)
MAKE_BINARY_BITFIELD_OPERATOR(^)

#undef MAKE_BINARY_BITFIELD_OPERATOR

/*
        Operators taking two parameters and returning the same type, while mutating the first parameter
*/
#define MAKE_BINARY_ASSIGN_BITFIELD_OPERATOR(op)                                                        \
    template<typename BitFieldTy>                                                                       \
    inline constexpr std::enable_if_t<detail::BitFieldOperatorsEnabled<BitFieldTy>::value, BitFieldTy>& \
    operator op##=(BitFieldTy & lhs, const BitFieldTy & rhs)                                            \
    {                                                                                                   \
        using BaseTy = std::underlying_type_t<BitFieldTy>;                                              \
        lhs = static_cast<BitFieldTy>(                                                                  \
            static_cast<BaseTy>(lhs) op static_cast<BaseTy>(rhs));                                      \
        return lhs;                                                                                     \
    }

MAKE_BINARY_ASSIGN_BITFIELD_OPERATOR(&)
MAKE_BINARY_ASSIGN_BITFIELD_OPERATOR(|)
MAKE_BINARY_ASSIGN_BITFIELD_OPERATOR(^)

#undef MAKE_BINARY_ASSIGN_BITFIELD_OPERATOR

/*
        Operators taking only one parameter and returning the same type
*/
#define MAKE_UNARY_BITFIELD_OPERATOR(op)                                                               \
    template<typename BitFieldTy>                                                                      \
    inline constexpr std::enable_if_t<detail::BitFieldOperatorsEnabled<BitFieldTy>::value, BitFieldTy> \
    operator op(const BitFieldTy & lhs)                                                                \
    {                                                                                                  \
        using BaseTy = std::underlying_type_t<BitFieldTy>;                                             \
        return static_cast<BitFieldTy>(                                                                \
            op(static_cast<BaseTy>(lhs)));                                                             \
    }

MAKE_UNARY_BITFIELD_OPERATOR(~)

#undef MAKE_UNARY_BITFIELD_OPERATOR

/*
        Convenience for testing flags
*/
template<typename BitFieldTy>
inline constexpr std::enable_if_t<detail::BitFieldOperatorsEnabled<BitFieldTy>::value, bool>
IsSet(const BitFieldTy& lhs, const BitFieldTy& rhs)
{
    return (lhs & rhs) == rhs;
}
template<typename BitFieldTy>
inline constexpr std::enable_if_t<detail::BitFieldOperatorsEnabled<BitFieldTy>::value, bool>
IsAnySet(const BitFieldTy& lhs, const BitFieldTy& rhs)
{
    using BaseTy = std::underlying_type_t<BitFieldTy>;
    return static_cast<BaseTy>(lhs & rhs) != BaseTy{};
}

template<class T>
consteval T Bit(T ith)
{
    return static_cast<T>(1 << (ith + 1));
}

// NOLINTEND
