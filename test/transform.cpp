#include <doctest/doctest.h>
#include <asio_ext/just.hpp>
#include <asio_ext/sync_wait.hpp>
#include <asio_ext/transform.hpp>
#include "test_receiver.hpp"
#include <iostream>

using namespace asio::execution;
TEST_CASE("transform: basic transform") {
    int count = 0;
    auto sender = transform(
        just(),
        [&] { ++count; }
    );
    sync_wait(sender);
    //REQUIRE(count == 1);
    //asio::execution::submit(std::move(submiter), recv);

    //REQUIRE(transform_called);
    //REQUIRE(submitted);
}