
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
    namespace transform
    {
        namespace detail
        {
            template <class sender_type, class receiver_type>
            struct operation_state
            {
                using next_operation_state = decltype(asio::execution::connect(std::declval<sender_type>(), std::declval<receiver_type>()));
                next_operation_state state_;

                operation_state(sender_type&& sender, receiver_type&& receiver) : state_(asio::execution::connect(std::move(sender), std::move(receiver))) {}

                void start() ASIO_NOEXCEPT {
                    asio::execution::start(state_);
                }
            };

            template <class function_type, class Receiver>
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

            template <class Sender, class Function>
            struct sender
            {
                using sender_type = asio_ext::remove_cvref_t<Sender>;
                using function_type = asio_ext::remove_cvref_t<Function>;

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
                    using receiver_type = receiver<function_type, Receiver>;
                    return operation_state<sender_type, receiver_type>(std::move(sender_), receiver_type(std::forward<Receiver>(recv), function_));
                }

                template <class S, class Fn>
                sender(S&& sender, Fn&& fn)
                    : sender_(std::forward<S>(sender)), function_(std::forward<Fn>(fn)) {
                }
            };
        } // namespace detail

        struct cpo
        {
            template <class Sender, class Function>
            auto operator()(Sender&& sender, Function&& fn) const {
                return detail::sender<Sender, Function>(std::forward<Sender>(sender),
                    std::forward<Function>(fn));
            }
        };

        template <typename T = cpo>
        struct static_instance
        {
            static const T instance;
        };

        template <typename T>
        const T static_instance<T>::instance = {};
    }
} // namespace asio_ext

namespace asio {
namespace execution {
    static ASIO_CONSTEXPR const asio_ext::transform::cpo&
        transform = asio_ext::transform::static_instance<>::instance;
} // namespace execution
} // namespace asio

#if !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class sender_type, class receiver_type>
struct start_member<asio_ext::transform::detail::operation_state<sender_type, receiver_type>>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio
#endif // !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

#if !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class Sender, class Function, typename R>
struct connect_member<asio_ext::transform::detail::sender<Sender, Function>, R>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef typename asio_ext::transform::detail::operation_state<
      typename asio_ext::transform::detail::sender<Sender, Function>::sender_type,
      typename asio_ext::transform::detail::receiver<
      typename asio_ext::transform::detail::sender<Sender, Function>::function_type, R>> result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)
#if !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class function_type, class Receiver, class... Values>
struct set_value_member<asio_ext::transform::detail::receiver<function_type, Receiver>, void(Values...)>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

#if !defined(ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class function_type, class Receiver, class E>
struct set_error_member<asio_ext::transform::detail::receiver<function_type, Receiver>, E>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

#if !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

namespace asio {
namespace traits {

template < class function_type, class Receiver >
struct set_done_member<asio_ext::transform::detail::receiver<function_type, Receiver>>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)
