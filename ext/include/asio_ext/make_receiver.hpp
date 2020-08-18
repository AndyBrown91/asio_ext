
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <utility>
#include <asio_ext/type_traits.hpp>

#include <boost/mp11/algorithm.hpp>
#include <type_traits>
#include <tuple>

namespace asio_ext
{
    namespace make_receiver_detail
    {
        template <unsigned N, class ValueType>
        struct make_receiver_tag_type
        {
            using value_type = std::decay_t<ValueType>;
            constexpr static unsigned tag_value = N;
            value_type value;

            template <class Fn, std::enable_if_t<std::is_constructible_v<value_type, Fn>>* = nullptr>
            make_receiver_tag_type(Fn&& fn) : value(std::forward<Fn>(fn)) {
            }

            template<class...Values>
            std::enable_if_t<std::is_invocable<value_type, Values...>::value> operator()(Values&&...values) {
                value(std::forward<Values>(values)...);
            }
        };

        constexpr unsigned done_tag_value = 0;
        constexpr unsigned error_tag_value = 1;
        constexpr unsigned value_tag_value = 2;

        template <class T>
        struct is_tag_type : std::false_type
        {};

        template <unsigned N, class T>
        struct is_tag_type<make_receiver_tag_type<N, T>> : std::true_type
        {};

        template <class T1, class T2>
        using tag_less = std::bool_constant < T1::tag_value<T2::tag_value>;

        template <class T1, class T2>
        using tag_equal = std::bool_constant<T1::tag_value == T2::tag_value>;

        template <class T>
        using tag_valid = std::bool_constant<T::tag_value <= value_tag_value>;

        template <class T, int N>
        using is_tag = std::conjunction<is_tag_type<T>, std::bool_constant<T::tag_value == N>>;

        template <class T>
        using is_done_tag = is_tag<T, done_tag_value>;

        template <class T>
        using is_error_tag = is_tag<T, error_tag_value>;

        template <class T>
        using is_value_tag = is_tag<T, value_tag_value>;

        template <class Tags>
        using has_error_tag =
            boost::mp11::mp_bool<boost::mp11::mp_count_if<Tags, is_error_tag>::value == 1>;

        template <class Tags>
        using has_done_tag = boost::mp11::mp_bool<boost::mp11::mp_count_if<Tags, is_done_tag>::value == 1>;

        template <class Tags>
        using has_value_tag =
            boost::mp11::mp_bool<boost::mp11::mp_count_if<Tags, is_value_tag>::value == 1>;

        template <class Tags>
        using valid_tags = boost::mp11::mp_and<
            boost::mp11::mp_all_of<Tags, tag_valid>,
            boost::mp11::mp_bool<boost::mp11::mp_count_if<Tags, is_done_tag>::value <= 1>,
            boost::mp11::mp_bool<boost::mp11::mp_count_if<Tags, is_error_tag>::value <= 1>,
            boost::mp11::mp_bool<boost::mp11::mp_count_if<Tags, is_value_tag>::value <= 1>>;

        template <class... Tags>
        struct receiver_impl
        {
            using tag_list = boost::mp11::mp_list<std::decay_t<Tags>...>;
            using storage_type = std::tuple<std::decay_t<Tags>...>;
            storage_type impls_;

            template<class...Ts, std::enable_if_t<std::is_constructible_v<storage_type, Ts...>>* = nullptr>
            receiver_impl(Ts&&... ts) : impls_(std::forward<Ts>(ts)...) {
                static_assert(valid_tags<tag_list>::value, "Not all tags are valid");
            }

            template <class... Values>
            void set_value(Values &&... values) {
                this->tagged_value(has_value_tag<tag_list>{}, std::forward<Values>(values)...);
            }

            void set_done() {
                this->tagged_done<boost::mp11::mp_find_if<tag_list, is_done_tag>::value>(
                    has_done_tag<tag_list>{});
            }

            template <class E>
            void set_error(E&& err) {
                this->tagged_error(has_error_tag<tag_list>{}, std::forward<E>(err));
            }

        private:
            template <class... Values>
            void tagged_value(std::false_type, Values &&... values) {
            }

            template <class... Values>
            void tagged_value(std::true_type, Values &&... values) {
                std::get<boost::mp11::mp_find_if<tag_list, is_value_tag>::value>(impls_)(
                    std::forward<Values>(values)...);
            }

            template <class E>
            void tagged_error(std::false_type, E&& err) {
                std::terminate();
            }

            template <class E>
            void tagged_error(std::true_type, E&& err) {
                std::get<boost::mp11::mp_find_if<tag_list, is_error_tag>::value>(impls_)(
                    std::forward<E>(err));
            }

            template <std::size_t N>
            void tagged_done(std::false_type) {
            }

            template <std::size_t N>
            void tagged_done(std::true_type) {
                std::get<N>(impls_)();
            }
        };

        struct build_receiver_impl_fn
        {
            template<class...Ts>
            auto operator()(Ts&&...ts) const
            {
                return receiver_impl<std::decay_t<Ts>...>(std::forward<Ts>(ts)...);
            }
        };

        constexpr build_receiver_impl_fn build_receiver_impl;

        template<class...Tags, unsigned N, class ChannelValueType>
        auto operator+(const receiver_impl<Tags...>& lhs, const make_receiver_tag_type<N, ChannelValueType>& rhs)
        {
            return std::apply(build_receiver_impl, std::tuple_cat(lhs.impls_, std::make_tuple(rhs)));
        }

        template<class...Tags, class...Tags2>
        auto operator+(const receiver_impl<Tags...>& lhs, const receiver_impl<Tags2...>& rhs)
        {
            return std::apply(build_receiver_impl, std::tuple_cat(lhs.impls_, rhs.impls_));
        }

        struct make_receiver_fn
        {
            template <class... Tags>
            auto operator()(Tags &&... tags) const {
                using tag_list = boost::mp11::mp_list<std::decay_t<Tags>...>;
                static_assert(valid_tags<tag_list>::value, "Invalid tag detected");
                return receiver_impl<Tags...>(std::forward<Tags>(tags)...);
            }
        };

        template<unsigned N0, class Type0, unsigned N1, class Type1>
        auto operator+(const make_receiver_tag_type<N0, Type0>& lhs, const make_receiver_tag_type<N1, Type1>& rhs)
        {
            return make_receiver_fn{}(lhs, rhs);
        }

        struct done_fn
        {
            template <class Fn, std::enable_if_t<std::is_invocable_v<Fn>>* = nullptr>
            auto operator()(Fn&& fn) const {
                return build_receiver_impl(make_receiver_tag_type<done_tag_value, std::decay_t<Fn>>(std::forward<Fn>(fn)));
            }
        };

        struct error_fn
        {
            template <class Fn>
            auto operator()(Fn&& fn) const {
                return build_receiver_impl(make_receiver_tag_type<error_tag_value, std::decay_t<Fn>>(std::forward<Fn>(fn)));
            }
        };

        struct value_fn
        {
            template <class Fn>
            auto operator()(Fn&& fn) const {
                return build_receiver_impl(make_receiver_tag_type<value_tag_value, std::decay_t<Fn>>(std::forward<Fn>(fn)));
            }
        };
    } // namespace make_receiver_detail

    constexpr make_receiver_detail::done_fn done_channel;
    constexpr make_receiver_detail::error_fn error_channel;
    constexpr make_receiver_detail::value_fn value_channel;
    constexpr make_receiver_detail::make_receiver_fn make_receiver;
} // namespace asio_ext