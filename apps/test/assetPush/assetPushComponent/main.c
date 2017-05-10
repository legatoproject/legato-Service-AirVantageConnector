//--------------------------------------------------------------------------------------------------
/**
 * Simple test app that creates and pushes asset data
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"


// Push callback handler
static void PushCallbackHandler
(
    le_avdata_PushStatus_t status,
    void* contextPtr
)
{
    int value = (int)contextPtr;
    LE_INFO("PushCallbackHandler: %d, value: %d", status, value);
}


// pushing an asset that is not created
void PushNonExistentAsset()
{
    LE_ASSERT(le_avdata_Push("/asdf/zxcv", PushCallbackHandler, NULL) == LE_NOT_FOUND);
}


// pushing an asset that is not created
void PushNotValidAsset()
{
    LE_ASSERT(le_avdata_Push("/asdf////", PushCallbackHandler, NULL) == LE_FAULT);
}


// pushing single element
void PushSingle()
{
    LE_ASSERT(le_avdata_CreateResource("/assetPush/value", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt("/assetPush/value", 5) == LE_OK);
    LE_ASSERT(le_avdata_Push("/assetPush/value", PushCallbackHandler, (void *)3) == LE_OK);
}


// pushing multiple element
void PushMulti()
{
    LE_ASSERT(le_avdata_CreateResource("/asset/value1", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("/asset/value2", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("/asset/value3", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource("/asset/value4", LE_AVDATA_ACCESS_VARIABLE) == LE_OK);

    LE_ASSERT(le_avdata_SetInt("/asset/value1", 5) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat("/asset/value2", 3.14) == LE_OK);
    LE_ASSERT(le_avdata_SetString("/asset/value3", "helloWorld") == LE_OK);
    LE_ASSERT(le_avdata_SetBool("/asset/value4", false) == LE_OK);
    LE_ASSERT(le_avdata_Push("/asset", PushCallbackHandler, (void *)4) == LE_OK);
}


//--------------------------------------------------------------------------------------------------
/**
 * Component initializer.  Must return when done initializing.
 * Note: Assumes session is opened.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    int testCase = 0;

    if (le_arg_NumArgs() >= 1)
    {
        const char* arg1 = le_arg_GetArg(0);
        testCase = atoi(arg1);
    }

    switch (testCase)
    {
        case 1:
            PushNonExistentAsset();
            break;
        case 2:
            PushNotValidAsset();
            break;
        case 3:
            PushSingle();
            break;
        case 4:
            PushMulti();
            break;
        default:
            break;
    }
}