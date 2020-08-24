#include <doctest/doctest.h>
#include <asio_ext/sync_wait.hpp>
#include <asio_ext/just.hpp>

using namespace asio::execution;
TEST_CASE("sync_wait: compile-test just()")
{
    sync_wait(just());
}

TEST_CASE("sync_wait: just(5) == 5")
{
    int test = sync_wait(just(5));
    REQUIRE(test == 5);
}