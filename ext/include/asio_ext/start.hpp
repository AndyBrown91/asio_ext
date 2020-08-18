
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
        void start();

        struct start_cpo
        {
            template<class Operation>
            using member_detector = decltype(std::declval<Operation>().start());

            template<class Operation>
            using free_function_detector = decltype(start(std::declval<Operation>()));

            template<class Operation>
            using use_member = asio_ext::is_detected<member_detector, Operation>;

            template<class Operation>
            using use_free_function = std::conjunction<
                std::negation<use_member<Operation>>,
                asio_ext::is_detected<free_function_detector, Operation>
            >;

            template<class Operation, std::enable_if_t<use_member<Operation>::value>* = nullptr>
            auto operator()(Operation& op) const {
                return op.start();
            }

            template<class Operation, std::enable_if_t<use_free_function<Operation>::value>* = nullptr>
            auto operator()(Operation& op) const {
                return start(std::forward<Operation>(op));
            }
        };
    }
    constexpr detail::start_cpo start;
}