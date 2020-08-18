
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <asio_ext/type_traits.hpp>
#include <asio_ext/tag.hpp>

namespace asio_ext
{
    namespace detail
    {
        struct tag_invoke_impl
        {
            template<class Class, class Tag, class...Args>
            std::enable_if_t<asio_ext::has_tag_invoke_member_function<Class, Tag, Args...>::value>
                operator()(Class&& target, Tag&& t, Args&&...args) const
            {
                target.tag_invoke(std::forward<Tag>(t), std::forward<Args>(args)...);
            }

            template<class Class, class Tag, class...Args>
            std::enable_if_t<asio_ext::use_tag_invoke_free_function<Class, Tag, Args...>::value>
                operator()(Class&& target, Tag&& t, Args&&...args) const
            {
                tag_invoke(std::forward<Class>(target), std::forward<Tag>(t), std::forward<Args>(args)...);
            }
        };
    }
    constexpr detail::tag_invoke_impl tag_invoke;
}