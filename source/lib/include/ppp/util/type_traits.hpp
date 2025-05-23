#pragma once

#include <type_traits>

// NOLINTBEGIN

/*
        Always evaluate to true/false
*/
template<class...>
struct always_true_t : std::true_type
{
};
template<class... T>
inline constexpr bool always_true_v = always_true_t<T...>::value;
template<class...>
struct always_false_t : std::false_type
{
};
template<class... T>
inline constexpr bool always_false_v = always_false_t<T...>::value;

/*
        Is always void
*/
template<class...>
using void_t = void;

/*
        Same type as passed in
*/
template<typename T>
struct identity
{
    using type = T;
};
template<typename T>
using identity_t = typename identity<T>::type;

/*
        Tests if type is an array of a given type
*/
template<class array_t, class U>
struct is_array_of : std::false_type
{
};
template<class T>
struct is_array_of<T[], T> : std::true_type
{
};
template<class T, std::size_t N>
struct is_array_of<T[N], T> : std::true_type
{
};
template<class array_t, class U>
inline constexpr bool is_array_of_v = is_array_of<array_t, U>::value;

/*
        Tests wether a type is in a list of types
*/
template<class T, class... list_t>
struct is_in_type_list : std::disjunction<std::is_same<T, list_t>...>
{
};
template<class T, template<class...> class tuple_t, class... U>
struct is_in_type_list<T, tuple_t<U...>> : is_in_type_list<T, U...>
{
};
template<class T, class... list_t>
inline constexpr bool is_in_type_list_v = is_in_type_list<T, list_t...>::value;

/*
        Concatenates two tuple-like types
*/
template<class tuple_lhs_t, class tuple_rhs_t>
struct concat
{
    static_assert(always_false_t<tuple_lhs_t, tuple_rhs_t>::value, "concat_t<T, U> only applies when T and U are instantiations of the same template");
};
template<template<class...> class tuple_t, class... lhs_t, class... rhs_t>
struct concat<tuple_t<lhs_t...>, tuple_t<rhs_t...>>
{
    using type = tuple_t<lhs_t..., rhs_t...>;
};
template<class tuple_lhs_t, class tuple_rhs_t>
using concat_t = typename concat<tuple_lhs_t, tuple_rhs_t>::type;

// NOLINTEND
