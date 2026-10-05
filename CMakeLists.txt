cmake_minimum_required(VERSION 4.1)
project(http_server C)

set(CMAKE_C_STANDARD 23)
enable_testing()

add_compile_options(-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Werror)
add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
add_link_options(-fsanitize=address,undefined)

add_library(httpserver src/server.c src/router.c
        include/httpserver.h
        src/utils/str_functions.c
        src/utils/buffer.c
        src/utils/buffer.h
        src/utils/queue.h
        src/utils/queue.c)

target_include_directories(httpserver PUBLIC include)

add_executable(dashboard_server example/main.c)
target_link_libraries(dashboard_server httpserver)

add_executable(test_str_functions tests/test_str_functions.c)
target_link_libraries(test_str_functions PRIVATE httpserver)
add_test(NAME str_functions COMMAND test_str_functions)