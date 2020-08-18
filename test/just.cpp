#include <doctest/doctest.h>
#include <asio_ext/connect.hpp>
#include <asio_ext/just.hpp>
#include <asio_ext/make_receiver.hpp>
#include <asio_ext/start.hpp>

TEST_CASE("just: single value connect")
{
    bool called = false;
    auto op = asio_ext::connect(asio_ext::just(), asio_ext::value_channel([&]() {
        called = true;
        }));
    REQUIRE_FALSE(called);
    asio_ext::start(op);
    REQUIRE(called);
}