
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <exception>
#include <type_traits>
#include <tuple>

#include <asio/detail/type_traits.hpp>

namespace asio_ext
{
    template <typename... Values>
    using sender_storage_t = typename std::tuple<std::decay_t<Values>...>;

    template<class T>
    std::decay_t<T> decay_copy(T&& t) {
        return std::forward<T>(t);
    }

    template <class T>
    using remove_cvref_t = typename asio::remove_cvref<T>::type;

    template <class T>
    struct is_nothrow_move_or_copy_constructible
        : std::disjunction<std::is_nothrow_move_constructible<T>, std::is_copy_constructible<T>>
    {};

    namespace detail
    {
        template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
        struct detector
        {
            using value_t = std::false_type;
            using type = Default;
        };

        template <class Default, template <class...> class Op, class... Args>
        struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
        {
            using value_t = std::true_type;
            using type = Op<Args...>;
        };
        struct nonesuch
        {
            ~nonesuch() = delete;
            nonesuch(nonesuch const&) = delete;
            void operator=(nonesuch const&) = delete;
        };
    } // namespace detail

    template <template <class...> class Op, class... Args>
    using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

    template <template <class...> class Op, class... Args>
    constexpr bool is_detected_v = is_detected<Op, Args...>::value;

    template <template <class...> class Op, class... Args>
    using detected_t = typename detail::detector<detail::nonesuch, void, Op, Args...>::type;

    template <class Default, template <class...> class Op, class... Args>
    using detected_or = detail::detector<Default, void, Op, Args...>;

} // namespace asio_ext