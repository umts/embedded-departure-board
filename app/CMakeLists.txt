cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/sign.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(embedded-departure-board)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
FILE(GLOB_RECURSE app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_compile_definitions(app PRIVATE _POSIX_C_SOURCE=200809L)

SET_SOURCE_FILES_PROPERTIES( ${app_sources} PROPERTIES COMPILE_FLAGS -fanalyzer )
