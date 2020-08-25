cmake_minimum_required (VERSION 3.10)
find_package(doctest CONFIG REQUIRED)
add_executable(test 
    just.cpp
    let.cpp
    sequence.cpp
    sync_wait.cpp
    test.cpp
    transform.cpp
    when_any.cpp
    when_all.cpp
)

target_link_libraries(test 
PRIVATE
	asio_ext
	doctest::doctest
)
