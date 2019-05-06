cmake_minimum_required(VERSION 3.13)
project(ulog C)

set(CMAKE_C_STANDARD 99)

add_executable(ulog main.c ulog.c ulog.h)