
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "type_traits.hpp"
#include <tuple>

namespace asio_ext
{
    namespace detail
    {
        template <class T>
        using value_type_detector = typename T::value_type;

        template <class T, bool HasValueType = asio_ext::is_detected_v<value_type_detector, T>>
        struct sender_value_type_impl
        {
            using type = void;
        };

        template <class T>
        struct sender_value_type_impl<T, true>
        {
            using type = typename T::value_type;
        };
    } // namespace detail

    template <class T>
    struct sender_value_type : detail::sender_value_type_impl<T>
    {};

    namespace detail
    {
        template<class T>
        struct mapped_sender_type
        {
            using type = asio_ext::remove_cvref_t<T>;
        };

        template<std::size_t N>
        struct mapped_sender_type<const char[N]>
        {
            using type = const char*;
        };
    }

    template<class T>
    struct mapped_sender_type
    {
        using type = typename detail::mapped_sender_type<std::decay_t<T>>::type;
    };

    template<class T>
    using mapped_sender_type_t = typename mapped_sender_type<T>::type;

    template<class...T>
    struct sender_value_type_for
    {
        using type = std::tuple<mapped_sender_type_t<T>...>;
    };

    template<>
    struct sender_value_type_for<>
    {
        using type = void;
    };

    template<class T>
    struct sender_value_type_for<T>
    {
        using type = std::decay_t<T>;
    };
} // namespace asio_ext