cmake_minimum_required(VERSION 3.16)
project(dash_host_test C)

set(CMAKE_C_STANDARD 11)
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../main/core)

add_executable(test_core test_core.c ${CORE}/leaf_can.c ${CORE}/dash_calc.c)
target_include_directories(test_core PRIVATE ${CORE})
target_compile_options(test_core PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_core PRIVATE m)

enable_testing()
add_test(NAME test_core COMMAND test_core)
