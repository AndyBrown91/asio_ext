
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <exception>
#include <utility>

#include <asio/execution/connect.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/start.hpp>

#include <asio_ext/sender_traits.hpp>
#include <asio_ext/type_traits.hpp>

#include <asio_ext/make_receiver.hpp>

namespace asio_ext
{
    namespace detail
    {
        template <class Sender, class Function>
        struct transform_sender
        {
            using sender_type = asio_ext::remove_cvref_t<Sender>;
            using function_type = asio_ext::remove_cvref_t<Function>;

            template <class Receiver>
            struct receiver
            {
                using receiver_type = asio_ext::remove_cvref_t<Receiver>;
                receiver_type next_;
                function_type fn_;

                template <class Rx, class Fn>
                receiver(Rx&& rx, Fn&& fn) : next_(std::forward<Rx>(rx)), fn_(std::forward<Fn>(fn)) {
                }

                template <class... Values>
                void set_value(Values &&... values) {
                    if constexpr (std::is_void<std::invoke_result_t<function_type, Values...>>::value) {
                        try {
                            fn_(std::forward<Values>(values)...);
                            asio::execution::set_value((receiver_type&&)next_);
                        }
                        catch (...) {
                            asio::execution::set_error((Receiver&&)next_, std::current_exception());
                        }
                    }
                    else {
                        try {
                            asio::execution::set_value((Receiver&&)next_,
                                fn_(std::forward<Values>(values)...));
                        }
                        catch (...) {
                            asio::execution::set_error(next_, std::current_exception());
                        }
                    }
                }

                void set_done() {
                    asio::execution::set_done((Receiver&&)next_);
                }

                template <class E>
                void set_error(E&& e) {
                    asio::execution::set_error((Receiver&&)next_, (E&&)e);
                }
            };

            template<class Receiver>
            struct operation_state
            {
                using next_operation_state = decltype(asio::execution::connect(std::declval<sender_type>(), std::declval<receiver<Receiver>>()));
                next_operation_state state_;

                operation_state(sender_type&& sender, receiver<Receiver>&& receiver) : state_(asio::execution::connect(std::move(sender), std::move(receiver))) {}

                void start() {
                    asio::execution::start(state_);
                }
            };

            sender_type sender_;
            function_type function_;

            template <template <class...> class Tuple>
            struct add_tuple
            {
                template <class T>
                using type = std::conditional_t<std::is_void_v<T>, Tuple<>, Tuple<T>>;
            };

            template <template <class...> class Tuple, template <class...> class Variant>
            using value_types = boost::mp11::mp_unique<boost::mp11::mp_transform<
                add_tuple<Tuple>::template type,
                asio_ext::function_result_types<Variant, function_type, sender_type>>>;

            template<template<class...> class Variant>
            using error_types = asio_ext::append_error_types<Variant, sender_type, std::exception_ptr>;

            static constexpr bool sends_done = asio_ext::sender_traits<sender_type>::sends_done;

            template<class Receiver>
            auto connect(Receiver&& recv) {
                return operation_state<Receiver>(std::move(sender_), receiver<Receiver>(std::forward<Receiver>(recv), function_));
            }

            template <class S, class Fn>
            transform_sender(S&& sender, Fn&& fn)
                : sender_(std::forward<S>(sender)), function_(std::forward<Fn>(fn)) {
            }
        };

        struct transform_fn
        {
            template <class Sender, class Function>
            auto operator()(Sender&& sender, Function&& fn) const {
                return ::asio_ext::detail::transform_sender<Sender, Function>(std::forward<Sender>(sender),
                    std::forward<Function>(fn));
            }
        };
    } // namespace detail

    constexpr ::asio_ext::detail::transform_fn transform{};
} // namespace asio_ext
