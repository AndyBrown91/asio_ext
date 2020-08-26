
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>
#include <optional>
#include <tuple>

#include <asio/execution/connect.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/start.hpp>

#include <boost/mp11/tuple.hpp>
#include <boost/mp11/algorithm.hpp>

#include <asio_ext/type_traits.hpp>
#include <asio_ext/sender_traits.hpp>

namespace asio_ext
{
    namespace when_any
    {
        namespace detail
        {
            template <typename Receiver, class... Senders>
            struct shared_state;

            template <typename Receiver, class... Senders>
            struct op_receiver
            {
                std::shared_ptr<shared_state<Receiver, Senders...>> state_;
                template<class...Values>
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

                template<class E>
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

            template <typename Receiver, class... Senders>
            struct shared_state
            {
                using operation_storage =
                    std::tuple<asio::execution::connect_result_t<Senders, 
                    op_receiver<Receiver, Senders...>>...>;

                std::optional<Receiver> next_;
                std::optional<operation_storage> op_storage_;

                shared_state(Receiver&& recv) : next_(std::move(recv)) {}
            };

            template<class Receiver, class... Senders>
            struct operation_state
            {
                using senders_storage = std::tuple<Senders...>;
                using shared_state_t = shared_state<Receiver, Senders...>;
                using operation_storage =
                    std::tuple<asio::execution::connect_result_t<Senders,
                    op_receiver<Receiver, Senders...>>...>;

                senders_storage senders_;

                std::shared_ptr<shared_state_t> state_;

                template<class Rx>
                operation_state(senders_storage&& senders, Rx&& receiver) : senders_(std::move(senders)),
                    state_(std::make_unique<shared_state_t>(std::forward<Rx>(receiver))) {}

                void start() ASIO_NOEXCEPT {
                    auto sender_to_op = [this](auto&&...senders) {
                        return operation_storage{
                            asio::execution::connect(
                                std::forward<decltype(senders)>(senders),
                                op_receiver<Receiver, Senders...>{state_})...
                        };
                    };

                    state_->op_storage_.emplace(std::apply(sender_to_op, std::move(senders_)));
                    boost::mp11::tuple_for_each(*state_->op_storage_, [](auto& op) {
                        asio::execution::start(op);
                    });
                }
            };

            template<class...Senders>
            struct when_any_op
            {
                using senders_storage = std::tuple<Senders...>;

                template<template<class...> class Tuple, template<class...> class Variant>
                using value_types = boost::mp11::mp_unique<
                    boost::mp11::mp_append<
                    typename asio::execution::sender_traits<Senders>::template value_types<Tuple, Variant>...
                    >
                >;

                template<template<class...> class Variant>
                using error_types = boost::mp11::mp_unique<
                    boost::mp11::mp_append<
                    typename asio::execution::sender_traits<Senders>::template error_types<Variant>...
                    >
                >;

                static constexpr bool sends_done = std::disjunction<
                    std::bool_constant<asio::execution::sender_traits<Senders>::sends_done>...
                >::value;

                senders_storage senders_;

                template<class...Tx, std::enable_if_t<std::is_constructible_v<senders_storage, Tx...>>* = nullptr>
                explicit when_any_op(Tx &&...tx) : senders_(std::forward<Tx>(tx)...) {}

                template<class Receiver>
                auto connect(Receiver&& receiver)
                {
                    return operation_state<asio_ext::remove_cvref_t<Receiver>, Senders...>
                        (std::move(senders_), std::forward<Receiver>(receiver));
                }
            };
        }

        struct cpo
        {
            template<class Sender>
            auto operator()(Sender&& sender) const {
                return std::forward<asio_ext::remove_cvref_t<Sender>>(sender);
            }

            template<class First, class... Rest>
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