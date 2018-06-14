/**
 * This module implements the unit tests for avdata API.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 *   Using dot as delimiter to set the path where asset data will be created
 */
//--------------------------------------------------------------------------------------------------
#define TEST1_RESOURCE_UNAVAILABLE                "test1.unAvailable"
#define TEST1_RESOURCE_INT                        "test1.resourceInt"
#define TEST1_RESOURCE_STRING                     "test1.resourceString"
#define TEST1_RESOURCE_FLOAT                      "test1.resourceFloat"
#define TEST1_RESOURCE_BOOL                       "test1.resourceBool"
#define TEST1_SETTING_INT                         "test1.settingInt"
#define TEST1_SETTING_STRING                      "test1.settingString"
#define TEST1_SETTING_FLOAT                       "test1.settingFloat"
#define TEST1_SETTING_BOOL                        "test1.settingBool"

//--------------------------------------------------------------------------------------------------
/**
 *   Using slash as delimiter to set the path where asset data will be created
 */
//--------------------------------------------------------------------------------------------------
#define TEST2_RESOURCE_UNAVAILABLE                "/test2/unAvailable"
#define TEST2_RESOURCE_INT                        "/test2/resourceInt"
#define TEST2_RESOURCE_STRING                     "/test2/resourceString"
#define TEST2_RESOURCE_FLOAT                      "/test2/resourceFloat"
#define TEST2_RESOURCE_BOOL                       "/test2/resourceBool"
#define TEST2_SETTING_INT                         "/test2/settingInt"
#define TEST2_SETTING_STRING                      "/test2/settingString"
#define TEST2_SETTING_FLOAT                       "/test2/settingFloat"
#define TEST2_SETTING_BOOL                        "/test2/settingBool"
#define TEST2_RESOURCE_SETTING                    "/test2/resourceSetting"
#define TEST2_RESOURCE_READ                       "/test2/resourceread"
#define TEST2_RESOURCE_COMMAND                    "/test2/resourcecommand"

//--------------------------------------------------------------------------------------------------
/**
 *   Few crazy paths where asset data will be created
 */
//--------------------------------------------------------------------------------------------------
#define TEST3_BAD_PATH1                           ".a.b...."
#define TEST3_BAD_PATH2                           "//a//b..."
#define TEST3_BAD_PATH3                           "//a/b."
#define TEST3_BAD_PATH4                           "a/b/c."
#define TEST3_BAD_PATH5                           "a/.b/c"
#define TEST3_BAD_PATH6                           "a..b.c"
#define TEST3_BAD_PATH7                           "a.b.c."

//--------------------------------------------------------------------------------------------------
/**
 *   Default values of variables
 */
//--------------------------------------------------------------------------------------------------
#define TEST_INT_VAL                              1234
#define TEST_STRING_VAL                           "test_string"
#define TEST_FLOAT_VAL                            123.4567
#define TEST_BOOL_VAL                             true
#define NUM_DOGS_VAR_RES                          "/home1/room1/SmartCam/numDogs"
#define NUM_DOGS_VAR_RES_INVALID1                 "/home1/room1/SmartCam/numDogsInvalid"
#define NUM_DOGS_VAR_RES_INVALID2                 "//home1/room1/SmartCam/numDogsInvalid"

//--------------------------------------------------------------------------------------------------
/**
 *   Path for name space test
 */
//--------------------------------------------------------------------------------------------------
#define RESOURCE_A         "/test/resourceA"
#define RESOURCE_B         "/test/resourceB"
#define RESOURCE_C         "/test/resourceC"
#define RESOURCE_D         "/test/resourceD"

//--------------------------------------------------------------------------------------------------
/**
 *   Ressouce for name space test
 */
//--------------------------------------------------------------------------------------------------
#define LOCAL_RESOURCE_A_INT_VAL            1
#define LOCAL_RESOURCE_B_INT_VAL            2
#define LOCAL_RESOURCE_C_INT_VAL            3
#define LOCAL_RESOURCE_D_INT_VAL            4
#define GLOBAL_RESOURCE_A_INT_VAL           11
#define GLOBAL_RESOURCE_B_INT_VAL           22
#define GLOBAL_RESOURCE_C_INT_VAL           33
#define GLOBAL_RESOURCE_D_INT_VAL           44


//-------------------------------------------------------------------------------------------------
/**
 * Push ack callback handler
 * This function is called whenever push has been performed successfully in AirVantage server
 */
//-------------------------------------------------------------------------------------------------
static void PushCallbackHandler
(
    le_avdata_PushStatus_t status, ///< [IN] Status of the data push
    void* contextPtr               ///< [IN] Associated context pointer
)
{
    LE_INFO("===== PushCallbackHandler =====");

    switch (status)
    {
        case LE_AVDATA_PUSH_SUCCESS:
            LE_INFO("Legato assetdata push successfully");
            break;
        case LE_AVDATA_PUSH_FAILED:
            LE_INFO("Legato assetdata push failed");
            break;
        default:
               LE_INFO("Legato assetdata push failed");
               break;
    }
}

//-------------------------------------------------------------------------------------------------
/**
 * Setting data handler.
 * This function is returned whenever AirVantage performs a read or write on the target
 */
//-------------------------------------------------------------------------------------------------
 void AccessTest2SettingHandler
(
    const char* path,                         ///< [IN] Asset data path.
    le_avdata_AccessType_t accessType,        ///< [IN] Permitted server access to this asset data.
    le_avdata_ArgumentListRef_t argumentList, ///< [IN] Argument list.
    void* contextPtr                          ///< [IN] Associated context pointer.
)
{
    LE_INFO("===== AccessTest2SettingHandler =====");
    double varSetting = 0;
    LE_ASSERT_OK(le_avdata_GetFloat(TEST2_RESOURCE_READ, &varSetting));
 }

 //-------------------------------------------------------------------------------------------------
/**
 * Setting data handler.
 * This function is returned whenever AirVantage performs a read on the target
 */
//-------------------------------------------------------------------------------------------------
 void AccessTest2ReadHandler
(
    const char* path,                          ///< [IN] Asset data path.
    le_avdata_AccessType_t accessType,         ///< Permitted server access to this asset data.
    le_avdata_ArgumentListRef_t argumentList, ///< [IN] Argument list.
    void* contextPtr                         ///< [IN] Associated context pointer.
)
{
    LE_INFO("===== AccessTest2ReadHandler =====");
    int varInt = 0;
    LE_ASSERT_OK(le_avdata_GetInt(TEST2_RESOURCE_READ, &varInt));
 }

//-------------------------------------------------------------------------------------------------
/**
 * Setting data handler.
 * This function is returned whenever AirVantage performs an execution on the target
 */
//-------------------------------------------------------------------------------------------------
 void AccessTest2ExecuteHandler
(
    const char* path,                          ///< [IN] Asset data path.
    le_avdata_AccessType_t accessType,         ///< Permitted server access to this asset data.
    le_avdata_ArgumentListRef_t argumentList, ///< [IN] Argument list.
    void* contextPtr                         ///< [IN] Associated context pointer.
)
{
    LE_INFO("===== AccessTest2ExecuteHandler =====");

    LE_ASSERT_OK(le_avdata_SetBool(TEST2_RESOURCE_COMMAND,false));
 }

//--------------------------------------------------------------------------------------------------
/**
 * Test dot delimited paths
 */
//--------------------------------------------------------------------------------------------------
static void TestDotDelimitedPath
(
    void
)
{
    int intVal;
    double floatVal;
    bool boolVal;
    char stringVal[256];
    LE_INFO("===== Test avdata Get/Set APIs for dot delimited path =====");

    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetInt(TEST1_RESOURCE_UNAVAILABLE, &intVal));

    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetString(TEST1_RESOURCE_UNAVAILABLE, (char*)&stringVal,
                                                  sizeof(stringVal)));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetFloat(TEST1_RESOURCE_UNAVAILABLE, &floatVal));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetBool(TEST1_RESOURCE_UNAVAILABLE, &boolVal));

    LE_INFO("===== Test avdata CreateResource API for dot delimited path =====");

    // Check if we can create variable resources
    LE_ASSERT_OK(le_avdata_CreateResource(TEST1_RESOURCE_INT, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetInt(TEST1_RESOURCE_INT, TEST_INT_VAL));
    LE_ASSERT_OK(le_avdata_GetInt(TEST1_RESOURCE_INT, &intVal));
    LE_ASSERT(TEST_INT_VAL == intVal);
    LE_ASSERT_OK(le_avdata_CreateResource(TEST1_RESOURCE_STRING, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetString(TEST1_RESOURCE_STRING, TEST_STRING_VAL));
    LE_ASSERT_OK(le_avdata_GetString(TEST1_RESOURCE_STRING, (char*)&stringVal, sizeof(stringVal)));
    LE_ASSERT(0 == strcmp(stringVal, TEST_STRING_VAL));
    LE_ASSERT_OK(le_avdata_CreateResource(TEST1_RESOURCE_FLOAT, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetFloat(TEST1_RESOURCE_FLOAT, TEST_FLOAT_VAL));
    LE_ASSERT_OK(le_avdata_GetFloat(TEST1_RESOURCE_FLOAT, &floatVal));
    LE_ASSERT(TEST_FLOAT_VAL == floatVal);
    LE_ASSERT_OK(le_avdata_CreateResource(TEST1_RESOURCE_BOOL, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetBool(TEST1_RESOURCE_BOOL, TEST_BOOL_VAL));
    LE_ASSERT_OK(le_avdata_GetBool(TEST1_RESOURCE_BOOL, &boolVal));
    LE_ASSERT(TEST_BOOL_VAL == boolVal);

    // Try to change all the variable to setting and make sure it errors
    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST1_RESOURCE_INT,
                                                       LE_AVDATA_ACCESS_SETTING));

    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST1_RESOURCE_STRING,
                                                       LE_AVDATA_ACCESS_SETTING));
    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST1_RESOURCE_FLOAT,
                                                       LE_AVDATA_ACCESS_SETTING));
    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST1_RESOURCE_BOOL,
                                                       LE_AVDATA_ACCESS_SETTING));
    // Sets an asset data to contain a null value.
    LE_ASSERT_OK(le_avdata_SetNull(TEST1_RESOURCE_BOOL));

    // Updating namespace.
    LE_ASSERT_OK(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_GLOBAL));
    LE_ASSERT_OK(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_APPLICATION));

    LE_INFO("===== Test avdata for dot delimited path passed =====");
}

//--------------------------------------------------------------------------------------------------
/**
 * Test slash delimited paths
 */
//--------------------------------------------------------------------------------------------------
static void TestSlashDelimitedPath
(
    void
)
{
    int intVal;
    double floatVal;
    bool boolVal;
    char stringVal[256];
    le_avdata_RequestSessionObjRef_t sessionRequestRef;
    LE_INFO("============= Test avdata Get/Set APIs for slash delimited path ==============");

    sessionRequestRef = le_avdata_RequestSession();
    LE_ASSERT(NULL != sessionRequestRef);
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetInt(TEST2_RESOURCE_UNAVAILABLE, &intVal));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetString(TEST2_RESOURCE_UNAVAILABLE, stringVal,
                                                  sizeof(stringVal)));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetFloat(TEST2_RESOURCE_UNAVAILABLE, &floatVal));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetBool(TEST2_RESOURCE_UNAVAILABLE, &boolVal));

    LE_INFO("============= Test avdata CreateResource API for slash delimited path ==============");

    // Check if we can create variable resources
    LE_ASSERT_OK(le_avdata_CreateResource(TEST2_RESOURCE_INT, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetInt(TEST2_RESOURCE_INT, TEST_INT_VAL));
    LE_ASSERT_OK(le_avdata_GetInt(TEST2_RESOURCE_INT, &intVal));
    LE_ASSERT(TEST_INT_VAL == intVal);

    LE_ASSERT_OK(le_avdata_CreateResource(TEST2_RESOURCE_STRING, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetString(TEST2_RESOURCE_STRING, TEST_STRING_VAL));
    LE_ASSERT_OK(le_avdata_GetString(TEST2_RESOURCE_STRING, stringVal, sizeof(stringVal)));
    LE_ASSERT(0 == strncmp(stringVal, TEST_STRING_VAL, sizeof(stringVal)));

    LE_ASSERT_OK(le_avdata_CreateResource(TEST2_RESOURCE_FLOAT, LE_AVDATA_ACCESS_SETTING));
    LE_ASSERT_OK(le_avdata_SetFloat(TEST2_RESOURCE_FLOAT, TEST_FLOAT_VAL));
    LE_ASSERT_OK(le_avdata_GetFloat(TEST2_RESOURCE_FLOAT, &floatVal));
    LE_ASSERT(TEST_FLOAT_VAL == floatVal);

    LE_ASSERT_OK(le_avdata_CreateResource(TEST2_RESOURCE_BOOL, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetBool(TEST2_RESOURCE_BOOL, TEST_BOOL_VAL));
    LE_ASSERT_OK(le_avdata_GetBool(TEST2_RESOURCE_BOOL, &boolVal));
    LE_ASSERT(TEST_BOOL_VAL == boolVal);

    // Try to change all the variable to setting and make sure it errors
    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST2_RESOURCE_INT,
                                                       LE_AVDATA_ACCESS_SETTING));
    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST2_RESOURCE_STRING,
                                                       LE_AVDATA_ACCESS_SETTING));
    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST2_RESOURCE_FLOAT,
                                                       LE_AVDATA_ACCESS_SETTING));
    LE_ASSERT(LE_DUPLICATE == le_avdata_CreateResource(TEST2_RESOURCE_BOOL,
                                                       LE_AVDATA_ACCESS_SETTING));

    LE_INFO("============= Test avdata AddResourceEventHandler API for slash delimited path =====");

    // Check if we can create Setting resources and register it with a Handler
    LE_ASSERT_OK(le_avdata_CreateResource(TEST2_RESOURCE_SETTING, LE_AVDATA_ACCESS_SETTING));
    LE_ASSERT_OK(le_avdata_SetFloat(TEST2_RESOURCE_SETTING, TEST_FLOAT_VAL));
    le_avdata_AddResourceEventHandler(TEST2_RESOURCE_SETTING, AccessTest2SettingHandler, NULL);

    // Check if we can create variable resources and register it with a Handler
    LE_ASSERT_OK(le_avdata_CreateResource(TEST2_RESOURCE_READ, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetInt(TEST2_RESOURCE_READ, TEST_INT_VAL));
    le_avdata_AddResourceEventHandler(TEST2_RESOURCE_READ, AccessTest2ReadHandler, NULL);

    //Check if we can create command resources and register it with a Handler
    LE_ASSERT_OK(le_avdata_CreateResource(TEST2_RESOURCE_COMMAND, LE_AVDATA_ACCESS_COMMAND));
    le_avdata_AddResourceEventHandler(TEST2_RESOURCE_COMMAND, AccessTest2ExecuteHandler, NULL);

    // ReleaseSession
    le_avdata_ReleaseSession(sessionRequestRef);

    LE_INFO("============= Test avdata for slash delimited path passed ==============");
}

//--------------------------------------------------------------------------------------------------
/**
 * Test bad paths
 */
//--------------------------------------------------------------------------------------------------
static void TestBadPath
(
    void
)
{
    LE_INFO("============= Test avdata CreateResource API for bad path ==============");

    LE_ASSERT(LE_FAULT == le_avdata_CreateResource(TEST3_BAD_PATH1, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT(LE_FAULT == le_avdata_CreateResource(TEST3_BAD_PATH2, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT(LE_FAULT == le_avdata_CreateResource(TEST3_BAD_PATH3, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT(LE_FAULT == le_avdata_CreateResource(TEST3_BAD_PATH4, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT(LE_FAULT == le_avdata_CreateResource(TEST3_BAD_PATH5, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT(LE_FAULT == le_avdata_CreateResource(TEST3_BAD_PATH6, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT(LE_FAULT == le_avdata_CreateResource(TEST3_BAD_PATH7, LE_AVDATA_ACCESS_VARIABLE));

    LE_INFO("============= Test avdata for bad path passed ==============");
}

//-------------------------------------------------------------------------------------------------
/**
 * Test Airvantage server APIs: le_avdata_Push(), le_avdata_CreateRecord(), le_avdata_PushRecord(),
 * le_avdata_DeleteRecord().
 */
//-------------------------------------------------------------------------------------------------
static void TestAirVantageServerAPIs
(
    void
)
{
    le_avdata_RequestSessionObjRef_t sessionRequestRef;

    LE_INFO("================ Test AirVantage Server APIS =================");

    LE_ASSERT_OK(le_avdata_CreateResource(NUM_DOGS_VAR_RES, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_SetInt(NUM_DOGS_VAR_RES, 44));

    LE_INFO("================ push resource to the server =================");

    LE_ASSERT(LE_NOT_FOUND == le_avdata_Push(NUM_DOGS_VAR_RES_INVALID1, PushCallbackHandler, NULL));
    LE_ASSERT(LE_FAULT == le_avdata_Push(NUM_DOGS_VAR_RES_INVALID2, PushCallbackHandler, NULL))
    LE_ASSERT_OK(le_avdata_Push(NUM_DOGS_VAR_RES, PushCallbackHandler, NULL));

    sessionRequestRef = le_avdata_RequestSession();
    LE_ASSERT(NULL != sessionRequestRef);
    le_avdata_ReleaseSession(sessionRequestRef);

    LE_INFO("================ Test AirVantage Server APIS passed =================");

}

//-------------------------------------------------------------------------------------------------
/**
 * Test avdata Name space api
 */
//-------------------------------------------------------------------------------------------------
void NamespaceTest
(
    void
)
{
    int intVal;

LE_INFO("================ Test avdata Namespace  =================");
    // Create resources (should be namespaced in app)
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_B, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_C, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_D, LE_AVDATA_ACCESS_VARIABLE));

    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_A, LOCAL_RESOURCE_A_INT_VAL));
    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_B, LOCAL_RESOURCE_B_INT_VAL));
    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_C, LOCAL_RESOURCE_C_INT_VAL));
    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_D, LOCAL_RESOURCE_D_INT_VAL));

    // Switch to global namespace
    LE_ASSERT_OK(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_GLOBAL));

    // Should not exists
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetInt(RESOURCE_A, &intVal));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetInt(RESOURCE_A, &intVal));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetInt(RESOURCE_A, &intVal));
    LE_ASSERT(LE_NOT_FOUND == le_avdata_GetInt(RESOURCE_A, &intVal));

    // Create these resources again but globally
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_B, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_C, LE_AVDATA_ACCESS_VARIABLE));
    LE_ASSERT_OK(le_avdata_CreateResource(RESOURCE_D, LE_AVDATA_ACCESS_VARIABLE));

    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_A, GLOBAL_RESOURCE_A_INT_VAL));
    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_B, GLOBAL_RESOURCE_B_INT_VAL));
    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_C, GLOBAL_RESOURCE_C_INT_VAL));
    LE_ASSERT_OK(le_avdata_SetInt(RESOURCE_D, GLOBAL_RESOURCE_D_INT_VAL));

    // Check global resource values
    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_A, &intVal));
    LE_INFO("Global namespace %s: %d", RESOURCE_A, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_A_INT_VAL);
    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_B, &intVal));
    LE_INFO("Global namespace %s: %d", RESOURCE_B, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_B_INT_VAL);
    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_C, &intVal));
    LE_INFO("Global namespace %s: %d", RESOURCE_C, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_C_INT_VAL);
    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_D, &intVal));
    LE_INFO("Global namespace %s: %d", RESOURCE_D, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_D_INT_VAL);

    // Switch back to application namespace and check our resources
    LE_ASSERT_OK(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_APPLICATION));

    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_A, &intVal));
    LE_INFO("Application namespace %s: %d", RESOURCE_A, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_A_INT_VAL);
    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_B, &intVal));
    LE_INFO("Application namespace %s: %d", RESOURCE_B, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_B_INT_VAL);
    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_C, &intVal));
    LE_INFO("Application namespace %s: %d", RESOURCE_C, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_C_INT_VAL);
    LE_ASSERT_OK(le_avdata_GetInt(RESOURCE_D, &intVal));
    LE_INFO("Application namespace %s: %d", RESOURCE_D, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_D_INT_VAL);

    LE_INFO("================ Test avdata Namespace passed =================");
}

//-------------------------------------------------------------------------------------------------
/**
 * Test Airvantage server APIs: le_avdata_CreateRecord(), le_avdata_PushRecord(),
 * le_avdata_DeleteRecord().
 */
//-------------------------------------------------------------------------------------------------
static void TestTimeseries
(
    void
)
{
    le_avdata_RequestSessionObjRef_t sessionRequestRef;
    struct timeval tv;
    uint64_t utcMilliSec;

    LE_INFO("============= Test avdata with times series ==============");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();
    gettimeofday(&tv, NULL);
    utcMilliSec = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
    /// Pushing a single float resource to the server
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 12.35, utcMilliSec));
    LE_ASSERT_OK(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL));

    // Pushing a single integer resource to the server
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 12, utcMilliSec));
    LE_ASSERT_OK(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL));
   // Pushing a single string resource to the server
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringValue", "hello World", utcMilliSec));
    LE_ASSERT_OK(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL));

    // Start record integer value on resource "intValue", then try to record value of different type
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 123, utcMilliSec));
    LE_ASSERT(LE_FAULT == le_avdata_RecordFloat(recRef, "intValue", 1.25, utcMilliSec));
    LE_ASSERT(LE_FAULT == le_avdata_RecordBool(recRef, "intValue", false, utcMilliSec));
    LE_ASSERT(LE_FAULT == le_avdata_RecordString(recRef, "intValue", "Hello World", utcMilliSec));

    LE_INFO("====== Pushing multiple integer values accumulated over one resource ======");

    // Pushing multiple integer values accumulated over ONE resource
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 01, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 02, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 03, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 04, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 05, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 06, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 07, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordInt(recRef, "intValue", 10, utcMilliSec));
    LE_ASSERT_OK(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL));

    LE_INFO("====== Pushing multiple float values accumulated over one resource ===========");

    // Pushing multiple float values accumulated over ONE resource
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 0.1, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 0.2, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 0.3, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 0.4, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 0.5, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 0.6, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 0.7, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "floatValue", 1.0, utcMilliSec));
    LE_ASSERT_OK(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL));

    LE_INFO("====== Pushing multiple bool values accumulated over one resource ===========");

    // Pushing multiple bool values accumulated over ONE resource
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", true, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", false, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", true, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", false, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", true, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", true, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", true, utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordFloat(recRef, "boolValue", true, utcMilliSec));
    LE_ASSERT_OK(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL));

    LE_INFO("====== Pushing multiple string values accumulated over one resource ===========");

    // Pushing multiple bool values accumulated over ONE resource
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue1", utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue2", utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue3", utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue4", utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue5", utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue6", utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue7", utcMilliSec));
    LE_ASSERT_OK(le_avdata_RecordString(recRef, "stringlValue", "stringlValue8", utcMilliSec));
    LE_ASSERT_OK(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL));

    sessionRequestRef = le_avdata_RequestSession();
    LE_ASSERT(NULL != sessionRequestRef);
    le_avdata_ReleaseSession(sessionRequestRef);

    le_avdata_DeleteRecord(recRef);
    LE_INFO("============= Test avdata with times series passed==============");
}

//--------------------------------------------------------------------------------------------------
/**
 * main of the test
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("=============== Start avDataUnitTest =====================");

    // Test - using dot as delimiter
    // Check if uncreated resources return LE_NOT_FOUND
    TestDotDelimitedPath();

    // Test - using slash as delimiter
    // Check if uncreated resources return LE_NOT_FOUND
    TestSlashDelimitedPath();

    // Test - check if an error occurs in case of BAD paths
    TestBadPath();

    // Test - airvantage server APIs
    TestAirVantageServerAPIs();

    //Test namespace
    NamespaceTest();

    //Test - time series
    TestTimeseries();

    LE_INFO("=============== avDataTest successful ===================");

    exit(EXIT_SUCCESS);
}
