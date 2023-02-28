cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cell_departure_board)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_sources(app PRIVATE src/main.c src/custom_http_client.c src/jsmn_parse.c src/rtc.c)
