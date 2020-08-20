#include <doctest/doctest.h>
#include <asio/execution/start.hpp>
#include <asio_ext/just.hpp>
#include <asio_ext/make_receiver.hpp>

TEST_CASE("just: zero value connect")
{
    bool called = false;
    auto op = asio::execution::connect(asio_ext::just(), asio_ext::value_channel([&]() {
        called = true;
    }));
    REQUIRE_FALSE(called);
    asio::execution::start(op);
    REQUIRE(called);
}

TEST_CASE("just: single value connect")
{
    bool called = false;
    auto op = asio::execution::connect(asio_ext::just(10), asio_ext::value_channel([&](int val) {
        REQUIRE(val == 10);
        called = true;
    }));
    REQUIRE_FALSE(called);
    asio::execution::start(op);
    REQUIRE(called);
}

TEST_CASE("just: multiple values connect")
{
    bool called = false;
    auto op = asio::execution::connect(asio_ext::just(10, 20, 30), asio_ext::value_channel([&](int x, int y, int z) {
        REQUIRE(x == 10);
        REQUIRE(y == 20);
        REQUIRE(z == 30);
        called = true;
    }));
    REQUIRE_FALSE(called);
    asio::execution::start(op);
    REQUIRE(called);
}