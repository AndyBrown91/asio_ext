
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
    namespace detail
    {
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

            template <class Receiver>
            struct operation_state
            {
                struct second_receiver
                {
                    operation_state* state_;

                    second_receiver(operation_state* state) : state_(state) {
                    }

                    template <class... Values>
                    void set_value(Values &&... values) {
                        asio::execution::set_value(std::move(state_->receiver_), std::forward<Values>(values)...);
                    }

                    void set_done() {
                        asio::execution::set_done(std::move(state_->receiver_));
                    }

                    template <class E>
                    void set_error(E&& error) {
                        asio::execution::set_error(std::move(state_->receiver_), std::forward<E>(error));
                    }
                };
                struct first_receiver
                {
                    operation_state* state_;

                    first_receiver(operation_state* state) : state_(state) {
                    }

                    template <class... Values>
                    void set_value(Values &&...) {
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

                    void set_done() {
                        asio::execution::set_done(std::move(state_->receiver_));
                    }

                    template <class E>
                    void set_error(E&& error) {
                        asio::execution::set_error(std::move(state_->receiver_), std::forward<E>(error));
                    }
                };

                operation_state(S1&& first, S2&& second, Receiver receiver)
                    : first_sender_(std::move(first)), second_sender_(std::move(second)),
                    receiver_(std::move(receiver)), state_(std::in_place_index<2>) {
                }

                void start() {
                    auto& ref = state_.template emplace<0>(
                        asio::execution::connect(std::move(first_sender_), first_receiver(this)));
                    asio::execution::start(ref);
                }

                using first_connect_type = asio_ext::remove_cvref_t<decltype(
                    asio::execution::connect(std::declval<S1&&>(), std::declval<first_receiver&&>()))>;
                using second_connect_type = asio_ext::remove_cvref_t<decltype(
                    asio::execution::connect(std::declval<S2&>(), std::declval<second_receiver&&>()))>;
                asio_ext::remove_cvref_t<S1> first_sender_;
                asio_ext::remove_cvref_t<S2> second_sender_;
                Receiver receiver_;
                std::variant<first_connect_type, second_connect_type, std::monostate> state_;
            };

            asio_ext::remove_cvref_t<S1> first_;
            asio_ext::remove_cvref_t<S2> second_;

            template <class T1, class T2>
            sequence_sender(T1&& t1, T2&& t2)
                : first_(std::move_if_noexcept(t1)), second_(std::move_if_noexcept(t2)) {
            }

            template <class Receiver>
            auto connect(Receiver&& receiver) {
                return operation_state<asio_ext::remove_cvref_t<Receiver>>(
                    std::move(first_), std::move(second_), std::move(receiver));
            }
        };
    } // namespace detail

    template <class Sender>
    std::decay_t<Sender> sequence(Sender&& sender) {
        return std::forward<Sender>(sender);
    }

    template <class S1, class... Senders>
    auto sequence(S1&& s1, Senders &&... senders) {
        using next_type = decltype(::asio_ext::sequence(std::forward<Senders>(senders)...));
        ;
        return detail::sequence_sender<asio_ext::remove_cvref_t<S1>,
            asio_ext::remove_cvref_t<next_type>>(
                std::forward<S1>(s1), ::asio_ext::sequence(std::forward<Senders>(senders)...));
    }
} // namespace asio_ext