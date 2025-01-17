cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/sign.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(embedded-departure-board)

# Generate hex files from pem files
set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
zephyr_include_directories(${gen_dir})

generate_inc_file_for_target(
  app
  keys/public/jes-contact-root-r4.pem
  ${gen_dir}/jes-contact-root-r4.pem.hex
)

generate_inc_file_for_target(
  app
  keys/public/bustracker-pvta-com-root-x1.pem
  ${gen_dir}/bustracker-pvta-com-root-x1.pem.hex
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
FILE(GLOB_RECURSE app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

target_compile_definitions(app PRIVATE _POSIX_C_SOURCE=200809L)
SET_SOURCE_FILES_PROPERTIES( ${app_sources} APPEND PROPERTIES COMPILE_FLAGS -fanalyzer )
