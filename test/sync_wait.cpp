#include <doctest/doctest.h>
#include <asio_ext/sync_wait.hpp>
#include <asio_ext/just.hpp>

TEST_CASE("sync_wait: compile-test just()")
{
    asio_ext::sync_wait(asio_ext::just());
}

TEST_CASE("sync_wait: just(5) == 5")
{
    int test = asio_ext::sync_wait(asio_ext::just(5));
    REQUIRE(test == 5);
}