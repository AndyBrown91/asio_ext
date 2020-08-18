
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
        void set_done();
        struct set_done_impl
        {
            template<class Receiver>
            using member_detector = decltype(std::declval<Receiver>().set_done());

            template<class Receiver>
            using free_fn_detector = decltype(set_done(std::declval<Receiver>()));

            template<class Receiver>
            using use_member = asio_ext::is_detected<member_detector, Receiver>;

            template<class Receiver>
            using use_free_function = std::conjunction<
                std::negation<use_member<Receiver>>,
                asio_ext::is_detected<free_fn_detector, Receiver>
            >;

            template<class Receiver>
            std::enable_if_t<use_member<Receiver>::value> operator()(Receiver&& rx) const noexcept {
                std::forward<Receiver>(rx).set_done();
            }

            template<class Receiver>
            std::enable_if_t<use_free_function<Receiver>::value> operator()(Receiver&& rx) const noexcept {
                set_done(rx);
            }
        };
    }
    constexpr detail::set_done_impl set_done;

    namespace tag
    {
        template<class Receiver>
        std::enable_if_t<std::is_invocable_v<::asio_ext::detail::set_done_impl, Receiver>>
            tag_invoke(Receiver&& rx, ::asio_ext::tag::set_done_t)
        {
            ::asio_ext::set_done(std::forward<Receiver>(rx));
        }
    }

}