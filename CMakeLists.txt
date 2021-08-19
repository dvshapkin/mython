cmake_minimum_required(VERSION 3.20)
project(mython)

set(CMAKE_CXX_STANDARD 17)

add_executable(mython main.cpp
        lexer.h lexer.cpp
        runtime.h runtime.cpp
        tests/test_runner_p.h
        tests/lexer_test_open.cpp
        tests/runtime_test.cpp)
