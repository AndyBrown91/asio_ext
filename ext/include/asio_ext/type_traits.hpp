
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <exception>
#include <type_traits>

#include <asio_ext/tag.hpp>

namespace asio_ext
{

    template<class T>
    std::decay_t<T> decay_copy(T&& t) {
        return std::forward<T>(t);
    }

    template <class T>
    struct remove_cvref
    {
        typedef std::remove_cv_t<std::remove_reference_t<T>> type;
    };

    template <class T>
    using remove_cvref_t = typename remove_cvref<T>::type;

    template <class T>
    struct is_nothrow_move_or_copy_constructible
        : std::disjunction<std::is_nothrow_move_constructible<T>, std::is_copy_constructible<T>>
    {};

    namespace detail
    {
        template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
        struct detector
        {
            using value_t = std::false_type;
            using type = Default;
        };

        template <class Default, template <class...> class Op, class... Args>
        struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
        {
            using value_t = std::true_type;
            using type = Op<Args...>;
        };
        struct nonesuch
        {
            ~nonesuch() = delete;
            nonesuch(nonesuch const&) = delete;
            void operator=(nonesuch const&) = delete;
        };
    } // namespace detail

    template <template <class...> class Op, class... Args>
    using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

    template <template <class...> class Op, class... Args>
    constexpr bool is_detected_v = is_detected<Op, Args...>::value;

    template <template <class...> class Op, class... Args>
    using detected_t = typename detail::detector<detail::nonesuch, void, Op, Args...>::type;

    template <class Default, template <class...> class Op, class... Args>
    using detected_or = detail::detector<Default, void, Op, Args...>;

    namespace detail
    {
        template <class Class, class... Args>
        using tag_invoke_member_detector =
            decltype(std::declval<Class>().tag_invoke(std::declval<Args>()...));
    }

    template <class Class, class Tag, class... Args>
    struct has_tag_invoke_member_function
        : ::asio_ext::is_detected<detail::tag_invoke_member_detector, Class, Tag, Args...>
    {};

    namespace detail
    {
        void tag_invoke();

        template <class Class, class Tag, class... Args>
        using tag_invoke_free_function_detector =
            decltype(tag_invoke(std::declval<Class>(), std::declval<Tag>(), std::declval<Args>()...));
    } // namespace detail

    template <class Class, class Tag, class... Args>
    struct has_tag_invoke_free_function
        : ::asio_ext::is_detected<detail::tag_invoke_free_function_detector, Class, Tag, Args...>
    {};

    template <class Class, class Tag, class... Args>
    struct use_tag_invoke_free_function
        : std::conjunction<std::negation<has_tag_invoke_member_function<Class, Tag, Args...>>,
        has_tag_invoke_free_function<Class, Tag, Args...>>
    {};

    template <class Class, class Tag, class... Args>
    struct has_tag_invoke : std::disjunction<has_tag_invoke_member_function<Class, Tag, Args...>,
        has_tag_invoke_free_function<Class, Tag, Args...>>
    {};

    namespace detail
    {
        template <class Executor, class Function>
        struct is_executor_of_impl
            : std::conjunction<std::is_invocable<Function>, std::is_nothrow_copy_constructible<Executor>,
            std::is_nothrow_destructible<Executor>,
            asio_ext::has_tag_invoke<Executor, asio_ext::tag::execute_t, Function>>
        {};

        struct archetype_invocable
        {
            void operator()();
            archetype_invocable() = delete;
        };
    } // namespace detail

    template <class Executor>
    struct is_executor : asio_ext::detail::is_executor_of_impl<Executor, detail::archetype_invocable>
    {};

    template <class Executor, class Function>
    struct is_executor_of : asio_ext::detail::is_executor_of_impl<Executor, Function>
    {};

    template <class T, class E = std::exception_ptr>
    struct is_receiver : std::conjunction<std::is_move_constructible<remove_cvref_t<T>>,
        is_nothrow_move_or_copy_constructible<remove_cvref_t<T>>,
        has_tag_invoke<T&&, ::asio_ext::tag::set_done_t>,
        has_tag_invoke<T&&, ::asio_ext::tag::set_error_t, E&&>>
    {};

    template <class T, class... Vs>
    struct is_receiver_of
        : std::conjunction<is_receiver<T>, has_tag_invoke<T, ::asio_ext::tag::set_value_t, Vs...>>
    {};

} // namespace asio_ext