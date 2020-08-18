
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "tag_invoke.hpp"
#include "type_traits.hpp"
#include <type_traits>

namespace asio_ext
{
    namespace detail
    {
        void set_error();
        struct set_error_impl
        {
            template <class Receiver, class Error>
            using member_detector = decltype(std::declval<Receiver>().set_error(std::declval<Error>()));

            template <class Receiver, class Error>
            using free_fn_detector = decltype(set_error(std::declval<Receiver>(), std::declval<Error>()));

            template <class Receiver, class Error>
            using use_member = asio_ext::is_detected<member_detector, Receiver, Error>;

            template <class Receiver, class Error>
            using use_free_function =
                std::conjunction<std::negation<use_member<Receiver, Error>>,
                asio_ext::is_detected<free_fn_detector, Receiver, Error>>;

            template <class Receiver, class Error>
            std::enable_if_t<use_member<Receiver, Error>::value> operator()(Receiver&& rx,
                Error&& err) const noexcept {
                std::forward<Receiver>(rx).set_error(std::forward<Error>(err));
            }

            template <class Receiver, class Error>
            std::enable_if_t<use_free_function<Receiver, Error>::value> operator()(Receiver&& rx,
                Error&& err) const noexcept {
                set_error(std::forward<Receiver>(rx), std::forward<Error>(err));
            }
        };
    } // namespace detail
    constexpr detail::set_error_impl set_error;

    namespace tag
    {
        template <class Receiver, class Error>
        std::enable_if_t<std::is_invocable_v<::asio_ext::detail::set_error_impl, Receiver, Error>>
            tag_invoke(Receiver&& rx, ::asio_ext::tag::set_error_t, Error&& err) {
            ::asio_ext::set_error(std::forward<Receiver>(rx), std::forward<Error>(err));
        }
    } // namespace tag

} // namespace asio_ext