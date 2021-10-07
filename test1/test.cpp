#include <assert.h>
#include <variant>
#include <asio.hpp>
#include <asio/coroutine.hpp>
#include <asio/experimental/awaitable_operators.hpp>
asio::awaitable<void>
selected(std::variant<
    asio::awaitable<void>,
    asio::awaitable<void> > v)
{
    switch (v.index())
    {
    case 0:
        return std::move(std::get<0>(v));
    case 1:
        return std::move(std::get<1>(v));
    default:
        throw std::logic_error(__func__);
    }
}

template <typename Iter>
asio::awaitable<void> 
any_one(Iter first, Iter last)
{
    using namespace asio::experimental::awaitable_operators;

    assert(first != last);

    auto next = std::next(first);
    if (next == last) {
#if 0
        co_await *first;
#endif
        co_return;
    }
    else {
#if 0
        co_return selected(
            co_await(*first) ||
            any_one(next, last));
#endif
    }
}

asio::awaitable<void> test(asio::io_context& ctx)
{
    std::vector<asio::awaitable<void>> items;
    any_one(items.begin(), items.end());
    printf("12345\n");
    //co_return any_one(items.begin(), items.end());
    co_return;
}

int main()
{
	asio::io_context ctx;
    auto guard = asio::make_work_guard(ctx);
	test(ctx);
	ctx.run();
	return 0;
}