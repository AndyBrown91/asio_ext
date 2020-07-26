cmake_minimum_required (VERSION 3.10)
project (asio)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ../third_party/asio/asio/include)
