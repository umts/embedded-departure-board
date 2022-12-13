cmake_minimum_required(VERSION 3.20.0)

# Define configuration files.
list(APPEND CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf)

# MCUboot related

# Adding custom overlay to add settings that aren't included in the SDK (yet) for the nRF9160 Feather
# if(${BOARD} STREQUAL circuitdojo_feather_nrf9160_ns)
#   message(STATUS "Adding .overlay for mcuboot.")
#   list(APPEND mcuboot_OVERLAY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot/circuitdojo_feather_nrf9160_ns.conf")
#   list(APPEND mcuboot_DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/mcuboot/circuitdojo_feather_nrf9160_ns.overlay")
# endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
find_package(Zephyr 3.2)
project(cell_departure_board)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_sources(app PRIVATE src/main.c src/http_client.c src/rtc.c)# src/parse_json.c)
