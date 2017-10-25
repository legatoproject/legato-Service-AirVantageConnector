#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

set(TEST_EXEC packageDownloadHost)

if(TEST_COVERAGE EQUAL 1)
    set(CFLAGS "--cflags=\"--coverage\"")
    set(LFLAGS "--ldflags=\"--coverage\"")
endif()

mkexe(${TEST_EXEC}
    .
    packageDownloadComp
    -i ${LEGATO_ROOT}/framework/liblegato
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/include
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/include/lwm2mcore
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/include/platform-specific/linux
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/packageDownloader
    -C "-fvisibility=default"
    -C "-Wno-error"
    ${CFLAGS}
    ${LFLAGS}
)

file(COPY packageDownloadComp/test.dwl DESTINATION ${DATA_OUTPUT_PATH})

add_test(${TEST_EXEC} ${EXECUTABLE_OUTPUT_PATH}/${TEST_EXEC})

add_dependencies(avc_tests_c ${TEST_EXEC})
