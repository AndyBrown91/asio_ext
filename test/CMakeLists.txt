cmake_minimum_required (VERSION 3.10)
find_package(doctest CONFIG REQUIRED)
add_executable(test test.cpp just.cpp)
target_link_libraries(test 
PRIVATE
	asio_ext
	doctest::doctest
)
