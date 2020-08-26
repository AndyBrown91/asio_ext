#pragma once
#if __has_include(<optional>) || \
    (defined(__cpp_lib_optional) && __cpp_lib_optional >= 201606L)
#include <optional>
namespace asio_ext {
	using std::optional;
}
#else

#endif