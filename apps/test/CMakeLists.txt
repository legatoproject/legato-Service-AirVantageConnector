#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

# Tests
enable_testing()

# Output
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests/lib)
set(APP_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests/apps)

file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})
file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_PATH})
file(MAKE_DIRECTORY ${APP_OUTPUT_PATH})

# Custom targets for different language tests
add_custom_target(avc_tests_c
  COMMENT "Generated C tests in ${EXECUTABLE_OUTPUT_PATH}"
)

# AVC Integration test (avcTool)
add_subdirectory(avcIntegrationTest)
