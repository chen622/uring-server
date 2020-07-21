cmake_minimum_required(VERSION 3.16)
project(uring_server C)

set(CMAKE_C_STANDARD 11)

add_executable(uring_server webserver_epoll.c webserver_liburing.c websever.c websever.h)
target_link_libraries(uring_server uring)