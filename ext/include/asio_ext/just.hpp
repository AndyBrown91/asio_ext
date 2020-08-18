
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <tuple>

#include <asio_ext/set_error.hpp>
#include <asio_ext/set_value.hpp>

#include <asio_ext/sender_value_type.hpp>

namespace asio_ext
{
    namespace detail
    {
        template <class... Values>
        struct just_sender
        {
            template<template<class...> class Tuple, template<class...> class Variant>
            using value_types = Variant<Tuple<std::decay_t<Values>...>>;
            template<template<class...> class Variant>
            using error_types = Variant<std::exception_ptr>;
            static constexpr bool sends_done = false;

            using storage_type = typename std::tuple<std::decay_t<Values>...>;
            storage_type val_;

            template <class... Vs, std::enable_if_t<std::is_constructible_v<storage_type, Vs...>>* = nullptr>
            just_sender(Vs &&... v) : val_(std::forward<Vs>(v)...) {
            }

            template <class Receiver>
            struct just_operation
            {
                Receiver receiver_;
                storage_type values_;

                void start() {
                    try {
                        auto caller = [this](auto &&... values) {
                            asio_ext::set_value(std::move(receiver_), std::forward<decltype(values)>(values)...);
                        };
                        std::apply(caller, std::move(values_));
                    }
                    catch (...) {
                        asio_ext::set_error((Receiver&&)receiver_, std::current_exception());
                    }
                }
            };

            template <class Receiver>
            auto connect(Receiver&& recv) {
                return just_operation<std::decay_t<Receiver>>{std::forward<Receiver>(recv), std::move(val_)};
            }
        };

        template <>
        struct just_sender<>
        {
            template<template<class...> class Tuple, template<class...> class Variant>
            using value_types = Variant<Tuple<>>;

            template<template<class...> class Variant>
            using error_types = Variant<std::exception_ptr>;

            static constexpr bool sends_done = false;

            template <class Receiver>
            struct just_operation
            {
                Receiver receiver_;

                void start() {
                    try {
                        asio_ext::set_value((Receiver&&)receiver_);
                    }
                    catch (...) {
                        asio_ext::set_error((Receiver&&)receiver_, std::current_exception());
                    }
                }
            };

            template <class Receiver>
            auto connect(Receiver&& recv) {
                return just_operation<asio_ext::remove_cvref_t<Receiver>>{std::forward<Receiver>(recv)};
            }
        };
    } // namespace detail

    constexpr auto just = [](auto &&... value) {
        return detail::just_sender<decltype(value)...>(std::forward<decltype(value)>(value)...);
    };
} // namespace asio_ext