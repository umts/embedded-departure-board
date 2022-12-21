cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cell_departure_board)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_sources(app PRIVATE src/main.c src/rtc.c src/parse_json.c src/http_client.c)