#include "legato.h"
#include "interfaces.h"

#define RESOURCE_UNAVAILABLE    "/test/unAvailable"

#define RESOURCE_INT            "/test/resourceInt"
#define RESOURCE_STRING         "/test/resourceString"
#define RESOURCE_FLOAT          "/test/resourceFloat"
#define RESOURCE_BOOL           "/test/resourceBool"

#define SETTING_INT             "/test/settingInt"
#define SETTING_STRING          "/test/settingString"
#define SETTING_FLOAT           "/test/settingFloat"
#define SETTING_BOOL            "/test/settingBool"

#define TEST_INT_VAL            1234
#define TEST_STRING_VAL         "test_string"
#define TEST_FLOAT_VAL          123.4567
#define TEST_BOOL_VAL           true


COMPONENT_INIT
{
    LE_INFO("Start assetDataTest");

    int intVal;
    double floatVal;
    bool boolVal;
    char stringVal[256];

    // Check if uncreated resources return LE_NOT_FOUND
    LE_ASSERT(le_avdata_GetInt(RESOURCE_UNAVAILABLE, &intVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetString(RESOURCE_UNAVAILABLE, (char*)&stringVal,
                                  sizeof(stringVal)) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetFloat(RESOURCE_UNAVAILABLE, &floatVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetBool(RESOURCE_UNAVAILABLE, &boolVal) == LE_NOT_FOUND);

    // Check if we can create variable resources
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_INT, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_INT, TEST_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_INT, &intVal) == LE_OK);
    LE_ASSERT(intVal == TEST_INT_VAL);

    LE_ASSERT(le_avdata_CreateResource(RESOURCE_STRING, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetString(RESOURCE_STRING, TEST_STRING_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetString(RESOURCE_STRING, (char*)&stringVal, sizeof(stringVal)) == LE_OK);
    LE_ASSERT(strcmp(stringVal, TEST_STRING_VAL) == 0);

    LE_ASSERT(le_avdata_CreateResource(RESOURCE_FLOAT, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat(RESOURCE_FLOAT, TEST_FLOAT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetFloat(RESOURCE_FLOAT, &floatVal) == LE_OK);
    LE_ASSERT(floatVal == TEST_FLOAT_VAL);

    LE_ASSERT(le_avdata_CreateResource(RESOURCE_BOOL, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetBool(RESOURCE_BOOL, TEST_BOOL_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetBool(RESOURCE_BOOL, &boolVal) == LE_OK);
    LE_ASSERT(boolVal == TEST_BOOL_VAL);

    // try to change all the variable to setting and make sure it errors
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_INT, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_STRING, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_FLOAT, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_BOOL, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);

    // check if we can create settings, but cannot set them from client side
    LE_ASSERT(le_avdata_CreateResource(SETTING_INT, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(SETTING_INT, TEST_INT_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetInt(SETTING_INT, &intVal) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(SETTING_STRING, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetString(SETTING_STRING, TEST_STRING_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetString(SETTING_STRING, (char*)&stringVal, sizeof(stringVal)) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(SETTING_FLOAT, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat(SETTING_FLOAT, TEST_FLOAT_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetFloat(SETTING_FLOAT, &floatVal) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(SETTING_BOOL, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetBool(SETTING_BOOL, TEST_BOOL_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetBool(SETTING_BOOL, &boolVal) == LE_UNAVAILABLE);

    LE_INFO("assetDataTest successful");
}
