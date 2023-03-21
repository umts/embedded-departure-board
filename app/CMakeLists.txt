cmake_minimum_required(VERSION 3.20.0)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf")
  list(APPEND mcuboot_OVERLAY_CONFIG
    "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.conf"
  )
endif()

# SET(mcuboot_CONFIG_BOOT_SIGNATURE_KEY_FILE "\"${CMAKE_CURRENT_SOURCE_DIR}/keys/private/boot-ecdsa-p256.pem\"")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(embedded-departure-board)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_sources(app PRIVATE src/main.c src/custom_http_client.c src/jsmn_parse.c src/rtc.c)
