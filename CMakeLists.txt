cmake_minimum_required(VERSION 3.10)
project(qylang)

file(GLOB SRC "src/regex/*.c")

add_executable(aaa ${SRC})
