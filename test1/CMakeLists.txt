cmake_minimum_required (VERSION 3.10)
add_executable(test1 test.cpp)
target_compile_features(test1 PUBLIC cxx_std_20)
target_link_libraries(test1
PRIVATE
	asio_ext
)
