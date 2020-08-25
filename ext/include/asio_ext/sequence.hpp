
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <exception>
#include <tuple>
#include <utility>
#include <variant>

#include <asio/execution/connect.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>
#include <asio/execution/start.hpp>
#include <asio/execution/submit.hpp>

#include <asio_ext/sender_traits.hpp>

namespace asio_ext
{
    namespace sequence
    {
        namespace detail
        {
            template <class S1, class S2, class Receiver>
            struct operation_state;

            template <class S1, class S2, class Receiver>
            struct basic_receiver {
                operation_state<S1, S2, Receiver>* state_;

                basic_receiver(operation_state<S1, S2, Receiver>* state) : state_(state) {}

                void set_done() {
                    asio::execution::set_done(std::move(state_->receiver_));
                }

                template <class E>
                void set_error(E&& error) {
                    asio::execution::set_error(std::move(state_->receiver_), std::forward<E>(error));
                }
            };

            template <class S1, class S2, class Receiver>
            struct second_receiver : public basic_receiver<S1, S2, Receiver>
            {
                using basic_receiver::basic_receiver;
                template <class... Values>
                void set_value(Values &&... values) {
                    asio::execution::set_value(std::move(state_->receiver_), std::forward<Values>(values)...);
                }
            };

            template <class S1, class S2, class Receiver>
            struct first_receiver : public basic_receiver<S1, S2, Receiver>
            {
                using basic_receiver::basic_receiver;

                template <class... Values>
                inline void set_value(Values &&...);
            };

            template <class S1, class S2, class Receiver>
            struct operation_state
            {
                operation_state(S1&& first, S2&& second, Receiver receiver)
                    : first_sender_(std::move(first)), second_sender_(std::move(second)),
                    receiver_(std::move(receiver)), state_(std::in_place_index<2>) {
                }

                void start() ASIO_NOEXCEPT {
                    auto& ref = state_.template emplace<0>(
                        asio::execution::connect(
                            std::move(first_sender_), 
                            first_receiver<S1, S2, Receiver>(this)));
                    asio::execution::start(ref);
                }

                using first_connect_type = asio_ext::remove_cvref_t<decltype(
                    asio::execution::connect(
                        std::declval<S1&&>(), 
                        std::declval<first_receiver<S1, S2, Receiver>&&>()))>;
                using second_connect_type = asio_ext::remove_cvref_t<decltype(
                    asio::execution::connect(
                        std::declval<S2&>(), 
                        std::declval<second_receiver<S1, S2, Receiver>&&>()))>;
                asio_ext::remove_cvref_t<S1> first_sender_;
                asio_ext::remove_cvref_t<S2> second_sender_;
                Receiver receiver_;
                std::variant<first_connect_type, second_connect_type, std::monostate> state_;
            };

            template <class S1, class S2, class Receiver>
            template <class... Values>
            void first_receiver<S1, S2, Receiver>::set_value(Values &&...) {
                // this will be destroyed below! Only use local variables!!!
                auto* state = state_;
                try {
                    auto& ref = state->state_.template emplace<1>(
                        asio::execution::connect(state_->second_sender_, second_receiver(state)));
                    auto index = state->state_.index();
                    auto valueless = state->state_.valueless_by_exception();
                    asio::execution::start(ref);
                }
                catch (...) {
                    asio::execution::set_error(std::move(state->receiver_), std::current_exception());
                }
            }

            template <class... Senders>
            struct sequence_sender;

            template <class S1, class S2>
            struct sequence_sender<S1, S2>
            {
                template <template <class...> class Tuple, template <class...> class Variant>
                using value_types = typename asio::execution::sender_traits<S2>::template value_types<Tuple, Variant>;

                template <template <class...> class Variant>
                using error_types = asio_ext::append_error_types<Variant, S2, std::exception_ptr>;

                static constexpr bool sends_done = asio::execution::sender_traits<S2>::sends_done;

                asio_ext::remove_cvref_t<S1> first_;
                asio_ext::remove_cvref_t<S2> second_;

                template <class T1, class T2>
                sequence_sender(T1&& t1, T2&& t2)
                    : first_(std::move_if_noexcept(t1)), second_(std::move_if_noexcept(t2)) {
                }

                template <class Receiver>
                auto connect(Receiver&& receiver) {
                    return operation_state<S1, S2, asio_ext::remove_cvref_t<Receiver>>(
                        std::move(first_), std::move(second_), std::move(receiver));
                }
            };
        } // namespace detail

        struct cpo
        {
            template <class Sender>
            std::decay_t<Sender> operator()(Sender&& sender) const {
                return std::forward<Sender>(sender);
            }

            template <class S1, class... Senders>
            auto operator()(S1&& s1, Senders &&... senders) const {
                using next_type = decltype(cpo{}(std::forward<Senders>(senders)...));
                return detail::sequence_sender<asio_ext::remove_cvref_t<S1>,
                    asio_ext::remove_cvref_t<next_type>>(
                        std::forward<S1>(s1), next_type(std::forward<Senders>(senders)...));
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
static ASIO_CONSTEXPR const asio_ext::sequence::cpo&
      sequence = asio_ext::sequence::static_instance<>::instance;
} // namespace execution
} // namespace asio

#if !defined(ASIO_HAS_DEDUCED_START_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class S1, class S2, class Receiver>
struct start_member<asio_ext::sequence::detail::operation_state<S1, S2, Receiver>>
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

template <class S1, class S2, typename R>
struct connect_member<asio_ext::sequence::detail::sequence_sender<S1, S2>, R>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef typename 
      asio_ext::sequence::detail::operation_state<S1, S2, asio_ext::remove_cvref_t<R>> result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_CONNECT_MEMBER_TRAIT)

#if !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

namespace asio {
namespace traits {

template <class S1, class S2, class Receiver, class... Values>
struct set_value_member<asio_ext::sequence::detail::first_receiver<S1, S2, Receiver>, void(Values...)>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

template <class S1, class S2, class Receiver, class... Values>
struct set_value_member<asio_ext::sequence::detail::second_receiver<S1, S2, Receiver>, void(Values...)>
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

template <class S1, class S2, class Receiver, class E>
struct set_error_member<asio_ext::sequence::detail::first_receiver<S1, S2, Receiver>, E>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

template <class S1, class S2, class Receiver, class E>
struct set_error_member<asio_ext::sequence::detail::second_receiver<S1, S2, Receiver>, E>
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

template <class S1, class S2, class Receiver>
struct set_done_member<asio_ext::sequence::detail::first_receiver<S1, S2, Receiver>>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

template <class S1, class S2, class Receiver>
struct set_done_member<asio_ext::sequence::detail::second_receiver<S1, S2, Receiver>>
{
    ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
    ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
    typedef void result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)