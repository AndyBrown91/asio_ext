#include <doctest/doctest.h>
#include <asio_ext/just.hpp>
#include <asio_ext/sync_wait.hpp>
#include <asio_ext/transform.hpp>
#include <asio_ext/when_any.hpp>

using namespace asio::execution;

template <typename F>
auto lazy(F&& f) {
    return transform(just(), (F&&)f);
}

TEST_CASE("single event when_all test")
{
    bool done1 = false;
    sync_wait(when_any(
        lazy([&] { done1 = true; })
    ));
    REQUIRE(done1 == true);
}

TEST_CASE("mutiple eveents when_all test")
{
    bool done1 = false, done2 = false;
    sync_wait(when_any(
        lazy([&] { done1 = true; }),
        lazy([&] { done2 = true; })
    ));
    REQUIRE((done1 | done2) == true);
}