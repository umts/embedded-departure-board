# SB_CONFIG_MERGED_HEX_FILES merges per board target, and MCUboot builds for
# the secure board variant while the app builds for /ns, so upstream emits two
# partial-chip files. Merge them into one full-chip merged.hex for programming
# outside of west (nrfutil/J-Link, recovery, release artifacts).
if(SB_CONFIG_MERGED_HEX_FILES)
  # Names follow sysbuild_merged_hex.cmake (CONFIG_BOARD_TARGET, / -> _);
  # hardcoded because image Kconfig isn't available yet when this runs.
  set(normalized_board "circuitdojo_feather_nrf9160_ns")
  set(normalized_board_secure "circuitdojo_feather_nrf9160")

  set(merged_inputs
    ${CMAKE_BINARY_DIR}/merged_${normalized_board_secure}.hex
    ${CMAKE_BINARY_DIR}/merged_${normalized_board}.hex
  )

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/merged.hex
    COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/scripts/build/mergehex.py
            -o ${CMAKE_BINARY_DIR}/merged.hex --overlap replace
            ${merged_inputs}
    DEPENDS ${merged_inputs}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  add_custom_target(merged_full_hex ALL DEPENDS ${CMAKE_BINARY_DIR}/merged.hex)
endif()
