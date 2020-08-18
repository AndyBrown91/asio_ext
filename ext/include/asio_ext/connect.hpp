
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <asio_ext/type_traits.hpp>
#include <type_traits>

namespace asio_ext
{
    namespace detail
    {
        void connect();

        struct connect_cpo
        {
            template<class Sender, class Receiver>
            using member_detector = decltype(std::declval<Sender>().connect(std::declval<Receiver>()));

            template<class Sender, class Receiver>
            using free_function_detector = decltype(connect(std::declval<Sender>(), std::declval<Receiver>()));

            template<class Sender, class Receiver>
            using use_member = asio_ext::is_detected<member_detector, Sender, Receiver>;

            template<class Sender, class Receiver>
            using use_free_function = std::conjunction<
                std::negation<use_member<Sender, Receiver>>,
                asio_ext::is_detected<free_function_detector, Sender, Receiver>
            >;

            template<class Sender, class Receiver, std::enable_if_t<use_member<Sender, Receiver>::value>* = nullptr>
            auto operator()(Sender&& sender, Receiver&& receiver) const {
                return sender.connect(std::forward<Receiver>(receiver));
            }

            template<class Sender, class Receiver, std::enable_if_t<use_free_function<Sender, Receiver>::value>* = nullptr>
            auto operator()(Sender&& sender, Receiver&& receiver) const {
                return connect(std::forward<Sender>(sender), std::forward<Receiver>(receiver));
            }
        };
    }
    constexpr detail::connect_cpo connect;

    template <class Sender, class Receiver>
    using operation_type =
        decltype(asio_ext::connect(std::declval<Sender>(), std::declval<Receiver>()));
}