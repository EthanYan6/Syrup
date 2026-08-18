# Copy the packaged firmware into docs/firmware for the GitHub Pages flasher.
# Deletes previous .bin files so "远程获取" always serves this build.
#
# Required -D variables:
#   DOCS_FIRMWARE_DIR  - destination directory (repo docs/firmware)
#   FIRMWARE_BIN       - source .bin from the build tree

if(NOT DEFINED DOCS_FIRMWARE_DIR)
    message(FATAL_ERROR "DOCS_FIRMWARE_DIR is required")
endif()
if(NOT DEFINED FIRMWARE_BIN)
    message(FATAL_ERROR "FIRMWARE_BIN is required")
endif()

if(NOT EXISTS "${FIRMWARE_BIN}")
    message(FATAL_ERROR "Firmware image not found: ${FIRMWARE_BIN}")
endif()

file(MAKE_DIRECTORY "${DOCS_FIRMWARE_DIR}")

file(GLOB _old_bins "${DOCS_FIRMWARE_DIR}/*.bin")
if(_old_bins)
    file(REMOVE ${_old_bins})
endif()

file(COPY_FILE "${FIRMWARE_BIN}" "${DOCS_FIRMWARE_DIR}/syrup.bin")

get_filename_component(_packaged_name "${FIRMWARE_BIN}" NAME)
message(STATUS "docs/firmware updated: syrup.bin (from ${_packaged_name})")
