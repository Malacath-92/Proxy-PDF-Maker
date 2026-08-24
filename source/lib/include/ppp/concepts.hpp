#pragma once

#include <concepts>

template<typename T>
concept StringLike =
    std::convertible_to<T, std::string_view> ||
    std::same_as<T, std::string>;

template<typename R>
concept RangeOfStringLike =
    std::ranges::input_range<R> &&
    StringLike<std::ranges::range_value_t<R>>;
