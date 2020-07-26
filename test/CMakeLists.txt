cmake_minimum_required (VERSION 3.10)
add_executable(test test.cpp)
target_link_libraries(test 
PRIVATE
	asio_ext
)
