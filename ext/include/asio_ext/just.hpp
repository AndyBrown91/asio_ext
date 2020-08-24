
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <tuple>

#include <asio/execution/connect.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/start.hpp>

namespace asio_ext
{
    namespace just
    {
        namespace detail
        {
            template <class storage_type, class Receiver>
            struct operation
            {
                Receiver receiver_;
                storage_type values_;

                void start() ASIO_NOEXCEPT {
                    try {
                        auto caller = [this](auto &&... values) {
                            asio::execution::set_value(std::move(receiver_), std::forward<decltype(values)>(values)...);
                        };
                        std::apply(caller, std::move(values_));
                    }
                    catch (...) {
                        asio::execution::set_error((Receiver&&)receiver_, std::current_exception());
                    }
                }
            };

            template <class... Values>
            struct sender
            {
                template<template<class...> class Tuple, template<class...> class Variant>
                using value_types = Variant<Tuple<std::decay_t<Values>...>>;
                template<template<class...> class Variant>
                using error_types = Variant<std::exception_ptr>;
                static constexpr bool sends_done = false;

                using storage_type = typename std::tuple<std::decay_t<Values>...>;
                storage_type val_;

                template <class... Vs, std::enable_if_t<std::is_constructible_v<storage_type, Vs...>>* = nullptr>
                sender(Vs &&... v) : val_(std::forward<Vs>(v)...) {
                }

                template <class Receiver>
                auto connect(Receiver&& recv) {
                    return operation<
                        storage_type, std::decay_t<Receiver>>{std::forward<Receiver>(recv), std::move(val_)};
                }
            };
        } // namespace detail

        struct cpo 
        {
            template <typename... Values>
            detail::sender<Values...> operator()(Values&&... values) const
            {
                return detail::sender<Values...>(std::forward<Values>(values)...);
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
static ASIO_CONSTEXPR const asio_ext::just::cpo&
      just = asio_ext::just::static_instance<>::instance;
} // namespace execution
} // namespace asio

#if !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class storage_type, class Receiver>
struct start_member<asio_ext::just::detail::operation<storage_type, Receiver>>
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

template <class... Values, typename R>
struct connect_member<asio_ext::just::detail::sender<Values...>, R>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef typename asio_ext::just::detail::operation<
      typename asio_ext::just::detail::sender<Values...>::storage_type, R> result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)