
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <utility>

#include <asio/execution/connect.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/start.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>

#include <asio_ext/sender_traits.hpp>
#include <asio_ext/type_traits.hpp>
#include <asio_ext/detail/optional.hpp>

namespace asio_ext
{
    namespace when_all
    {
        namespace detail
        {
            template <typename Receiver, class... Senders>
            struct shared_state;

            template <typename Receiver, class... Senders>
            struct op_receiver
            {
                std::shared_ptr<shared_state<Receiver, Senders...>> state_;
                template <class... Values>
                void set_value(Values &&... values) {
                    if (state_->next_) {
                        state_->waiting_for_--;
                        if (state_->waiting_for_ == 0) {
                            try {
                                asio::execution::set_value(*state_->next_, std::forward<Values>(values)...);
                                state_->next_.reset();
                            }
                            catch (...) {
                                asio::execution::set_error(*state_->next_, std::current_exception());
                            }
                        }
                    }
                    state_.reset();
                }

                template <class E>
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
            using operation_storage_t = std::tuple<
                asio::execution::connect_result_t<
                Senders,
                op_receiver<Receiver, Senders...>>...>;

            template <typename Receiver, class... Senders>
            struct shared_state
            {
                asio_ext::optional<Receiver> next_;
                int waiting_for_;
                asio_ext::optional<operation_storage_t<Receiver, Senders...>> op_storage_;

                shared_state(Receiver&& recv, int waiting_for)
                    : next_(std::move(recv)), waiting_for_(waiting_for) {
                }
            };

            template <class Receiver, class... Senders>
            op_receiver<Receiver, Senders...>
                make_op_receiver(std::shared_ptr<shared_state<Receiver, Senders...>> state_)
            {
                return op_receiver<Receiver, Senders...>{std::move(state_)};
            }

            template <class Receiver, class... Senders>
            struct operation_state
            {
                using senders_storage = std::tuple<Senders...>;

                using shared_state_t = shared_state<Receiver, Senders...>;

                senders_storage senders_;

                std::shared_ptr<shared_state_t> state_;

                template <class Rx>
                operation_state(senders_storage&& senders, Rx&& receiver)
                    : senders_(std::move(senders)),
                    state_(std::make_unique<shared_state_t>(std::forward<Rx>(receiver),
                        std::tuple_size_v<senders_storage>)) {
                }

                void start() ASIO_NOEXCEPT {
                    auto sender_to_op = [this](auto &&... senders) {
                        return operation_storage_t<Receiver, Senders...>{
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

            template <class... Senders>
            struct when_all_op
            {
                using senders_storage = std::tuple<Senders...>;

                template <template <class...> class Tuple, template <class...> class Variant>
                using value_types = boost::mp11::mp_unique<boost::mp11::mp_append<
                    typename asio::execution::sender_traits<Senders>::template value_types<Tuple, Variant>...>>;

                template <template <class...> class Variant>
                using error_types = boost::mp11::mp_unique<boost::mp11::mp_append<
                    typename asio::execution::sender_traits<Senders>::template error_types<Variant>...>>;

                // static constexpr bool sends_done = (asio_ext::sender_traits<Senders>::sends_done || ...);
                static constexpr bool sends_done = std::disjunction<
                    std::bool_constant<asio::execution::sender_traits<Senders>::sends_done>...>::value;

                senders_storage senders_;

                template <class... Tx>
                when_all_op(Tx &&... tx) : senders_(std::forward<Tx>(tx)...) {}

                template <class Receiver>
                auto connect(Receiver&& receiver) {
                    return operation_state<asio_ext::remove_cvref_t<Receiver>, Senders...>(
                        std::move(senders_), std::forward<Receiver>(receiver));
                }
            };
        } // namespace detail

        struct cpo
        {
            template <class S>
            auto operator()(S&& sender) const {
                return std::forward<asio_ext::remove_cvref_t<S>>(sender);
            }

            template <class First, class... Rest>
            auto operator()(First&& first, Rest&&... rest) const {
                return detail::when_all_op<
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
static ASIO_CONSTEXPR const asio_ext::when_all::cpo&
      when_all = asio_ext::when_all::static_instance<>::instance;
} // namespace execution
} // namespace asio

#if !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class Receiver, class... Senders>
struct start_member<asio_ext::when_all::detail::operation_state<Receiver, Senders...>>
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

template <class... Senders, typename R>
struct connect_member<asio_ext::when_all::detail::when_all_op<Senders...>, R>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef typename 
      asio_ext::when_all::detail::operation_state<asio_ext::remove_cvref_t<R>, Senders...>
      result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

#if !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <typename Receiver, class... Senders, class... Values>
struct set_value_member<asio_ext::when_all::detail::op_receiver<Receiver, Senders...>, void(Values...)>
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

template <typename Receiver, class... Senders, typename E>
struct set_error_member<asio_ext::when_all::detail::op_receiver<Receiver, Senders...>, E>
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

template <typename Receiver, class... Senders>
struct set_done_member<asio_ext::when_all::detail::op_receiver<Receiver, Senders...>>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)