
//          Copyright Andreas Wass 2004 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace asio_ext
{
	namespace tag
	{
		struct execute_t
		{
		};
		constexpr execute_t execute;

		struct submit_t
		{
		};
		constexpr submit_t submit;
		struct set_value_t
		{
		};
		constexpr set_value_t set_value;

		struct set_done_t
		{
		};
		constexpr set_done_t set_done;

		struct set_error_t
		{
		};
		constexpr set_error_t set_error;

		struct schedule_t
		{
		};
		constexpr schedule_t schedule;
	} // namespace tag
} // namespace asio_ext