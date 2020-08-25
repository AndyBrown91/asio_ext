
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <exception>
#include <optional>
#include <thread>
#include <variant>
#include <tuple>

#include <asio/execution/connect.hpp>
#include <asio/execution/start.hpp>
#include <asio/execution/set_done.hpp>
#include <asio/execution/set_error.hpp>
#include <asio/execution/set_value.hpp>

#include <asio_ext/sender_traits.hpp>
#include <iostream>

namespace asio_ext
{
    namespace sync_wait
    {
        namespace detail
        {
            template <typename T>
            struct shared_state;

            template <typename T>
            class reference
            {
            public:
                reference(shared_state<T>& state) : state_(&state) {}
                void set_value() { this->set_done(); }
                template <class T2, class... Rest>
                std::enable_if_t<std::is_convertible_v<std::decay_t<T2>, std::decay_t<T>>>
                    set_value(T2&& v, Rest &&...);
                void set_done();
                void set_error(std::exception_ptr ex);
            private:
                shared_state<T>* state_;
            };

            template <>
            class reference<void>
            {
            public:
                reference(shared_state<void>& state) : state_(&state) {}
                template <class... Values>
                void set_value(Values &&... v);
                inline void set_done();
                inline void set_error(std::exception_ptr ex);
            private:
                shared_state<void>* state_;
            };

            template <class T>
            struct shared_state
            {
                friend class reference<T>;
                T get() const {
                    while (!has_been_set_.load()) {
                        std::this_thread::yield();
                    }
                    if (exception_) {
                        std::rethrow_exception(exception_);
                    }
                    return *value;
                }

                reference<T> ref() {
                    return reference<T>(*this);
                }

            private:
                std::exception_ptr exception_;
                std::atomic_bool has_been_set_{ false };
                std::optional<T> value;
            };

            template <typename T>
            template <class T2, class... Rest>
            std::enable_if_t<std::is_convertible_v<std::decay_t<T2>, std::decay_t<T>>>
                reference<T>::set_value(T2&& v, Rest &&...) {
                state_->value = std::forward<T2>(v);
                state_->has_been_set_.store(true);
            }

            template <typename T>
            void reference<T>::set_done() {
                state_->value = std::nullopt;
                state_->has_been_set_.store(true);
            }

            template <typename T>
            void reference<T>::set_error(std::exception_ptr ex) {
                state_->exception_ = ex;
                state_->has_been_set_.store(true);
            }

            template <>
            struct shared_state<void>
            {
                friend class reference<void>;
                reference<void> ref() {
                    return reference<void>(*this);
                }

                void get() const {
                    while (!has_been_set_.load()) {
                        std::this_thread::yield();
                    }
                    if (exception_) {
                        std::rethrow_exception(exception_);
                    }
                }

            private:
                std::exception_ptr exception_;
                std::atomic_bool has_been_set_{ false };
            };

            template <class... Values>
            void reference<void>::set_value(Values &&... v) {
                state_->has_been_set_.store(true);
            }

            void reference<void>::set_done() {
                state_->has_been_set_.store(true);
            }

            void reference<void>::set_error(std::exception_ptr ex) {
                state_->exception_ = ex;
                state_->has_been_set_.store(true);
            }

            template<class T>
            struct valued_sync_wait_impl
            {
                template<class Sender>
                static auto run(Sender&& sender)
                {
                    using decayed = std::decay_t<Sender>;
                    using value_types = typename asio::execution::sender_traits<decayed>::template value_types<std::tuple, std::variant>;
                    shared_state<value_types> state;
                    auto op = asio::execution::connect(std::forward<Sender>(sender), state.ref());
                    asio::execution::start(op);
                    return state.get();
                }
            };

            template<class T>
            struct valued_sync_wait_impl<std::variant<std::tuple<T>>>
            {
                template<class Sender>
                static auto run(Sender&& sender)
                {
                    shared_state<T> state;
                    auto op = asio::execution::connect(std::forward<Sender>(sender), state.ref());
                    asio::execution::start(op);
                    return state.get();
                }
            };

            template<>
            struct valued_sync_wait_impl<std::variant<std::tuple<>>>
            {
                template<class Sender>
                static auto run(Sender&& sender)
                {
                    shared_state<void> state;
                    auto op = asio::execution::connect(std::forward<Sender>(sender), state.ref());
                    asio::execution::start(op);
                    state.get();
                }
            };

        } // namespace detail

        struct cpo
        {
            template <class Sender>
            constexpr auto operator()(Sender&& sender) const {
                using decayed = std::decay_t<Sender>;
                using value_types = typename asio::execution::sender_traits<decayed>::template value_types<std::tuple, std::variant>;
                return asio_ext::sync_wait::detail::valued_sync_wait_impl<value_types>::run(std::forward<Sender>(sender));
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
    static ASIO_CONSTEXPR const asio_ext::sync_wait::cpo&
        sync_wait = asio_ext::sync_wait::static_instance<>::instance;
} // namespace execution
} // namespace asio

#if !defined(ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)
namespace asio {
namespace traits {

template <class T, class... Values>
struct set_value_member<asio_ext::sync_wait::detail::reference<T>, void(Values...)>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = false);
  typedef void result_type;
};

template <class... Values>
struct set_value_member<asio_ext::sync_wait::detail::reference<void>, void(Values...)>
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

template <typename T, typename E>
struct set_error_member<asio_ext::sync_wait::detail::reference<T>, E>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

template <typename E>
struct set_error_member<asio_ext::sync_wait::detail::reference<void>, E>
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

template <typename T>
struct set_done_member<asio_ext::sync_wait::detail::reference<T>>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

template <>
struct set_done_member<asio_ext::sync_wait::detail::reference<void>>
{
  ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

} // namespace traits
} // namespace asio

#endif // !defined(ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)
