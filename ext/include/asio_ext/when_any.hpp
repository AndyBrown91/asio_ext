
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>
#include <tuple>

#include <asio/execution/connect.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/start.hpp>

#include <boost/mp11/tuple.hpp>
#include <boost/mp11/algorithm.hpp>

#include <asio_ext/detail/optional.hpp>
#include <asio_ext/type_traits.hpp>
#include <asio_ext/sender_traits.hpp>

namespace asio_ext
{
    namespace when_any
    {
        namespace detail
        {
            template <typename Receiver, typename... Senders>
            struct shared_state;

            template <typename Receiver, typename... Senders>
            struct op_receiver
            {
                std::shared_ptr<shared_state<Receiver, Senders...>> state_;
                template <typename...Values>
                void set_value(Values&&...values)
                {
                    if (state_->next_) {
                        try {
                            asio::execution::set_value(*state_->next_, std::forward<Values>(values)...);
                            state_->next_.reset();
                        }
                        catch (...) {
                            asio::execution::set_error(*state_->next_, std::current_exception());
                        }
                    }
                    state_.reset();
                }

                template <typename E>
                void set_error(E&& e) {
                    if (state_->next_) {
                        asio::execution::set_error(*state_->next_, std::forward<E>(e));
                        state_->next_.reset();
                    }
                    state_.reset();
                }

                void set_done() noexcept {
                    if (state_->next_) {
                        asio::execution::set_done(*state_->next_);
                        state_->next_.reset();
                    }
                    state_.reset();
                }
            };

            template <typename Receiver, typename... Senders>
            struct shared_state
            {
                using operation_storage =
                    std::tuple<asio::execution::connect_result_t<Senders, 
                    op_receiver<Receiver, Senders...>>...>;

                asio_ext::optional<std::decay_t<Receiver>> next_;
                asio_ext::optional<operation_storage> op_storage_;

                shared_state(Receiver&& recv) : next_(std::move(recv)) {}
            };

            template <typename Receiver, typename... Senders>
            op_receiver<Receiver, Senders...>
                make_op_receiver(std::shared_ptr<shared_state<Receiver, Senders...>> state_)
            {
                return op_receiver<Receiver, Senders...>{std::move(state_)};
            }

            template <typename Receiver, typename... Senders>
            struct operation_state
            {
                using shared_state_t = shared_state<Receiver, Senders...>;
                using operation_storage =
                    std::tuple<asio::execution::connect_result_t<Senders,
                    op_receiver<Receiver, Senders...>>...>;

                sender_storage_t<Senders...> senders_;

                std::shared_ptr<shared_state_t> state_;

                template <typename Rx>
                operation_state(sender_storage_t<Senders...>&& senders, Rx&& receiver) : senders_(std::move(senders)),
                    state_(std::make_unique<shared_state_t>(std::forward<Rx>(receiver))) {}

                void start() ASIO_NOEXCEPT {
                    auto sender_to_op = [this](auto&&...senders) {
                        return operation_storage{
                            asio::execution::connect(
                                std::forward<decltype(senders)>(senders),
                                make_op_receiver(state_))... };
                    };

                    state_->op_storage_.emplace(std::apply(sender_to_op, std::move(senders_)));
                    boost::mp11::tuple_for_each(*state_->op_storage_, [](auto& op) {
                        asio::execution::start(op);
                    });
                }
            };

            template<typename...Senders>
            struct when_any_op
            {
                template<template<typename...> class Tuple, template<typename...> class Variant>
                using value_types = boost::mp11::mp_unique<
                    boost::mp11::mp_append<
                    typename asio::execution::sender_traits<Senders>::template value_types<Tuple, Variant>...
                    >
                >;

                template<template<typename...> class Variant>
                using error_types = boost::mp11::mp_unique<
                    boost::mp11::mp_append<
                    typename asio::execution::sender_traits<Senders>::template error_types<Variant>...
                    >
                >;

                static constexpr bool sends_done = std::disjunction<
                    std::bool_constant<asio::execution::sender_traits<Senders>::sends_done>...
                >::value;

                sender_storage_t<Senders...> senders_;

                template <typename...Tx,
                    std::enable_if_t<std::is_constructible_v<sender_storage_t<Senders...>, Tx...>>* = nullptr>
                explicit when_any_op(Tx &&...tx) : senders_(std::forward<Tx>(tx)...) {}

                template <typename Receiver>
                auto connect(Receiver&& receiver)
                {
                    return operation_state<Receiver, Senders...>
                        (std::move(senders_), std::forward<Receiver>(receiver));
                }
            };
        }

        struct cpo
        {
            template <typename Sender>
            auto operator()(Sender&& sender) const {
                return std::forward<asio_ext::remove_cvref_t<Sender>>(sender);
            }

            template <typename First, typename... Rest>
            auto operator()(First&& first, Rest&&... rest) const {
                return detail::when_any_op<
                    asio_ext::remove_cvref_t<First>,
                    asio_ext::remove_cvref_t<Rest>...>(
                    std::forward<First>(first),
                    std::forward<Rest>(rest)...);
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
static ASIO_CONSTEXPR const asio_ext::when_any::cpo&
      when_any = asio_ext::when_any::static_instance<>::instance;
} // namespace execution
} // namespace asio

#if !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <typename Receiver, typename... Senders>
struct start_member<asio_ext::when_any::detail::operation_state<Receiver, Senders...>>
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

template <typename... Senders, typename Receiver>
struct connect_member<asio_ext::when_any::detail::when_any_op<Senders...>, Receiver>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef typename 
      asio_ext::when_any::detail::operation_state<Receiver, Senders...>
      result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

#if !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <typename Receiver, typename... Senders, typename... Values>
struct set_value_member<asio_ext::when_any::detail::op_receiver<Receiver, Senders...>, void(Values...)>
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

template <typename Receiver, typename... Senders, typename E>
struct set_error_member<asio_ext::when_any::detail::op_receiver<Receiver, Senders...>, E>
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

template <typename Receiver, typename... Senders>
struct set_done_member<asio_ext::when_any::detail::op_receiver<Receiver, Senders...>>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)