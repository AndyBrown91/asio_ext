#pragma once

#include <memory>
#include <tuple>

#include <asio/execution/set_value.hpp>
#include <asio_ext/type_traits.hpp>

struct test_receiver
{
    bool* shared_submitted = nullptr;
    void set_done() {}

    template<class T>
    void set_error(T) {}

    void set_value() {
        if (shared_submitted) {
            *shared_submitted = true;
        }
    }
};

struct counting_receiver
{
    struct counters_type
    {
        int set_value = 0;
        int set_done = 0;
        int set_error = 0;
    };

    std::shared_ptr<counters_type> counters{ std::make_shared<counters_type>() };

    template<class...Values>
    void set_value(Values&&...values) {
        counters->set_value++;
    }

    void set_done() {
        counters->set_done++;
    }

    template<class E>
    void set_error(E&& e) {
        counters->set_error++;
    }
};

struct test_sender
{
    template<template<class...> class Tuple, template<class...> class Variant>
    using value_types = Variant<Tuple<>>;

    template<template<class...> class Variant>
    using error_types = Variant<>;

    static constexpr bool sends_done = false;

    template<class Receiver>
    void submit(Receiver&& rcv)
    {
        asio::execution::set_value(rcv);
    }

    template<class Receiver>
    struct operation_state
    {
        Receiver receiver_;
        void start() {
            asio::execution::set_value(std::move(receiver_));
        }
    };

    template<class Receiver>
    auto connect(Receiver&& receiver) {
        return operation_state<asio_ext::remove_cvref_t<Receiver>>{std::forward<Receiver>(receiver)};
    }
};

struct conditional_sender
{
    bool should_set_value = false;
    explicit conditional_sender(bool b) : should_set_value(b) {}

    template<class Receiver>
    struct operation
    {
        bool should_set_value_;
        Receiver next_;
        void start() {
            if (should_set_value_) {
                asio::execution::set_value(std::move(next_));
            }
        }
    };

    template<class Receiver>
    auto connect(Receiver&& recv) {
        return operation<asio_ext::remove_cvref_t<Receiver>>{
            should_set_value,
                std::forward<Receiver>(recv)
        };
    }
};

template<class...Values>
struct value_sender
{
    using storage_type = std::tuple<asio_ext::remove_cvref_t<Values>...>;
    storage_type val_;

    template<template<class...> class Tuple, template<class...> class Variant>
    using value_types = Variant<Tuple<std::decay_t<Values>...>>;

    template<template<class...> class Variant>
    using error_types = Variant<>;

    static constexpr bool sends_done = false;

    template<class...Vs>
    value_sender(Vs&&... v) : val_(std::forward<Vs>(v)...) {}

    template<class Receiver>
    struct operation
    {
        storage_type val_;
        Receiver receiver_;
        void start() {
            std::apply([&, this](auto&&...values) {
                asio::execution::set_value(std::move(receiver_), std::forward<decltype(values)>(values)...);
            }, std::move(val_));
        }
    };

    template<class Receiver>
    auto connect(Receiver&& rx) {
        return operation<asio_ext::remove_cvref_t<Receiver>> {
            std::move(val_),
                std::forward<Receiver>(rx)
        };
    }
};