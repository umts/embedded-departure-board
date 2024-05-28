cmake_minimum_required(VERSION 3.20.0)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/keys/private/boot-ecdsa-p256.pem")
  SET(mcuboot_CONFIG_BOOT_SIGNATURE_KEY_FILE "\"${CMAKE_CURRENT_SOURCE_DIR}/keys/private/boot-ecdsa-p256.pem\"")
  SET(mcuboot_CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256 "y")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(embedded-departure-board)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

SET_SOURCE_FILES_PROPERTIES( ${app_sources} PROPERTIES COMPILE_FLAGS -fanalyzer )
