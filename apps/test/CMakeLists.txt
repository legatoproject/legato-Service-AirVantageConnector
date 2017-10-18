#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

# Tests
enable_testing()

# Output
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests/lib)
set(APP_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests/apps)
set(DATA_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests/data)

file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})
file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_PATH})
file(MAKE_DIRECTORY ${APP_OUTPUT_PATH})

# Custom targets for different language tests
add_custom_target(avc_tests_c
  COMMENT "Generated C tests in ${EXECUTABLE_OUTPUT_PATH}"
)

# AVC Integration test (avcTool)
add_subdirectory(avcIntegrationTest)

# Package Downloader UnitTest
if((LEGATO_TARGET MATCHES "ar7") OR (LEGATO_TARGET MATCHES "ar86"))
    # Targets using 'legacy' AVC should not compile the package downloader unit test.
    # TODO: A better method to do this is to use the variable 'LEGATO_APP_AVC_CHOICE'.
    # So, find a way to get this variable value from this file scope.
else()
    add_subdirectory(packageDownloadHost)
endif()

# AirVantageConnector unitary test
add_subdirectory(airVantageConnectorUnitTest)
