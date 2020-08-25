#include <doctest/doctest.h>
#include <asio_ext/just.hpp>
#include <asio_ext/sequence.hpp>
#include <asio_ext/sync_wait.hpp>
#include <asio_ext/transform.hpp>

using namespace asio::execution;

template <typename F>
auto lazy(F&& f) {
    return transform(just(), (F&&)f);
}

TEST_CASE("basic sequence test")
{
    std::string result;
    sync_wait(sequence(
        lazy([&] { result = "Done"; }
    )));
    REQUIRE(result == "Done");
}

TEST_CASE("More sequence senders test")
{
    std::string result;
    sync_wait(sequence(
        lazy([&] { result += "1"; }),
        lazy([&] { result += "2"; })
    ));
    REQUIRE(result == "12");
}