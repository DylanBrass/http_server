cmake_minimum_required(VERSION 4.1)
project(http_server C)

set(CMAKE_C_STANDARD 23)

add_library(httpserver src/server.c src/router.c
        include/httpserver.h
        src/utils/str_functions.c
        src/utils/arr_functions.c)

target_include_directories(httpserver PUBLIC include)

add_executable(dashboard_server example/main.c)

target_link_libraries(dashboard_server httpserver)