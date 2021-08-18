cmake_minimum_required(VERSION 3.20)
project(mython)

set(CMAKE_CXX_STANDARD 17)

add_executable(mython main.cpp lexer.h lexer.cpp test_runner_p.h lexer_test_open.cpp runtime.h runtime.cpp runtime_test.cpp)
