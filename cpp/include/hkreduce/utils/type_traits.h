#pragma once

#include <type_traits>


template<class T>
struct does_support_copy_semantic
{
    static constexpr bool value = is_copy_assignable<T>::value && is_copy_constructible<T>::value;
};


template<class T>
struct does_support_move_semantic
{
    static constexpr bool value = is_move_assignable<T>::value && is_move_constructible<T>::value;
};