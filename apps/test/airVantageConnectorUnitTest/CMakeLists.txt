#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

set(TEST_EXEC airVantageConnectorUnitTest)

set(LEGATO_AVC "${LEGATO_ROOT}/apps/platformServices/airVantageConnector/")

if(TEST_COVERAGE EQUAL 1)
    set(CFLAGS "--cflags=\"--coverage\"")
    set(LFLAGS "--ldflags=\"--coverage\"")
endif()

mkexe(${TEST_EXEC}
    airVantageConnectorComp
    .
    -i airVantageConnectorComp
    -i ${LEGATO_AVC}/apps/test/airVantageConnectorUnitTest/
    -i ${LEGATO_AVC}/avcClient/
    -i ${LEGATO_AVC}/avcDaemon/
    -i ${LEGATO_AVC}/avcAppUpdate/
    -i ${LEGATO_AVC}/packageDownloader/
    -i ${LEGATO_ROOT}/framework/liblegato
    -i ${LEGATO_ROOT}/components/watchdogChain/
    -i ${LEGATO_ROOT}/framework/liblegato/linux/
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/include/
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/include/platform-specific/linux/
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/packageDownloader/
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/3rdParty/wakaama/core/
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/include/lwm2mcore/
    -i ${LEGATO_ROOT}/3rdParty/Lwm2mCore/sessionManager/
    -i ${LEGATO_ROOT}/interfaces/airVantage/
    -i ${LEGATO_ROOT}/interfaces/modemServices/
    -i ${LEGATO_ROOT}/interfaces/
    -i ${PA_DIR}/simu/components/le_pa_avc
    -i ${LEGATO_ROOT}/components/airVantage/platformAdaptor/inc/
    -s ${LEGATO_ROOT}/3rdParty/Lwm2mCore/include/lwm2mcore/
    ${CFLAGS}
    ${LFLAGS}
    -C "-fvisibility=default"
)

add_test(${TEST_EXEC} ${EXECUTABLE_OUTPUT_PATH}/${TEST_EXEC})

# This is a C test
add_dependencies(tests_c ${TEST_EXEC})
