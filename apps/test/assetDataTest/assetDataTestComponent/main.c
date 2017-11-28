#include "legato.h"
#include "interfaces.h"

// Test1 : using dot as delimiter
#define TEST1_RESOURCE_UNAVAILABLE                "test1.unAvailable"
#define TEST1_RESOURCE_INT                        "test1.resourceInt"
#define TEST1_RESOURCE_STRING                     "test1.resourceString"
#define TEST1_RESOURCE_FLOAT                      "test1.resourceFloat"
#define TEST1_RESOURCE_BOOL                       "test1.resourceBool"
#define TEST1_SETTING_INT                         "test1.settingInt"
#define TEST1_SETTING_STRING                      "test1.settingString"
#define TEST1_SETTING_FLOAT                       "test1.settingFloat"
#define TEST1_SETTING_BOOL                        "test1.settingBool"

// Test2 : using slash as delimiter
#define TEST2_RESOURCE_UNAVAILABLE                "/test2/unAvailable"
#define TEST2_RESOURCE_INT                        "/test2/resourceInt"
#define TEST2_RESOURCE_STRING                     "/test2/resourceString"
#define TEST2_RESOURCE_FLOAT                      "/test2/resourceFloat"
#define TEST2_RESOURCE_BOOL                       "/test2/resourceBool"
#define TEST2_SETTING_INT                         "/test2/settingInt"
#define TEST2_SETTING_STRING                      "/test2/settingString"
#define TEST2_SETTING_FLOAT                       "/test2/settingFloat"
#define TEST2_SETTING_BOOL                        "/test2/settingBool"

// Test2 : Few crazy paths
#define TEST3_BAD_PATH1                           ".a.b...."
#define TEST3_BAD_PATH2                           "//a//b..."
#define TEST3_BAD_PATH3                           "//a/b."
#define TEST3_BAD_PATH4                           "a/b/c."
#define TEST3_BAD_PATH5                           "a/.b/c"
#define TEST3_BAD_PATH6                           "a..b.c"
#define TEST3_BAD_PATH7                           "a.b.c."

#define TEST_INT_VAL                        1234
#define TEST_STRING_VAL                     "test_string"
#define TEST_FLOAT_VAL                      123.4567
#define TEST_BOOL_VAL                       true

// test dot delimited paths
static void TestDotDelimitedPath
(
    void
)
{
    int intVal;
    double floatVal;
    bool boolVal;
    char stringVal[256];

    LE_ASSERT(le_avdata_GetInt(TEST1_RESOURCE_UNAVAILABLE, &intVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetString(TEST1_RESOURCE_UNAVAILABLE, (char*)&stringVal,
                                  sizeof(stringVal)) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetFloat(TEST1_RESOURCE_UNAVAILABLE, &floatVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetBool(TEST1_RESOURCE_UNAVAILABLE, &boolVal) == LE_NOT_FOUND);

    // Check if we can create variable resources
    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_INT, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(TEST1_RESOURCE_INT, TEST_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetInt(TEST1_RESOURCE_INT, &intVal) == LE_OK);
    LE_ASSERT(intVal == TEST_INT_VAL);

    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_STRING, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetString(TEST1_RESOURCE_STRING, TEST_STRING_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetString(TEST1_RESOURCE_STRING, (char*)&stringVal, sizeof(stringVal)) == LE_OK);
    LE_ASSERT(strcmp(stringVal, TEST_STRING_VAL) == 0);

    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_FLOAT, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat(TEST1_RESOURCE_FLOAT, TEST_FLOAT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetFloat(TEST1_RESOURCE_FLOAT, &floatVal) == LE_OK);
    LE_ASSERT(floatVal == TEST_FLOAT_VAL);

    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_BOOL, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetBool(TEST1_RESOURCE_BOOL, TEST_BOOL_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetBool(TEST1_RESOURCE_BOOL, &boolVal) == LE_OK);
    LE_ASSERT(boolVal == TEST_BOOL_VAL);

    // try to change all the variable to setting and make sure it errors
    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_INT, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_STRING, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_FLOAT, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(TEST1_RESOURCE_BOOL, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);

    // check if we can create settings, but cannot set them from client side
    LE_ASSERT(le_avdata_CreateResource(TEST1_SETTING_INT, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(TEST1_SETTING_INT, TEST_INT_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetInt(TEST1_SETTING_INT, &intVal) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(TEST1_SETTING_STRING, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetString(TEST1_SETTING_STRING, TEST_STRING_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetString(TEST1_SETTING_STRING, (char*)&stringVal, sizeof(stringVal)) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(TEST1_SETTING_FLOAT, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat(TEST1_SETTING_FLOAT, TEST_FLOAT_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetFloat(TEST1_SETTING_FLOAT, &floatVal) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(TEST1_SETTING_BOOL, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetBool(TEST1_SETTING_BOOL, TEST_BOOL_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetBool(TEST1_SETTING_BOOL, &boolVal) == LE_UNAVAILABLE);
}

// test slash delimited paths
static void TestSlashDelimitedPath
(
    void
)
{
    int intVal;
    double floatVal;
    bool boolVal;
    char stringVal[256];

    LE_ASSERT(le_avdata_GetInt(TEST2_RESOURCE_UNAVAILABLE, &intVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetString(TEST2_RESOURCE_UNAVAILABLE, (char*)&stringVal,
                                  sizeof(stringVal)) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetFloat(TEST2_RESOURCE_UNAVAILABLE, &floatVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetBool(TEST2_RESOURCE_UNAVAILABLE, &boolVal) == LE_NOT_FOUND);

    // Check if we can create variable resources
    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_INT, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(TEST2_RESOURCE_INT, TEST_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetInt(TEST2_RESOURCE_INT, &intVal) == LE_OK);
    LE_ASSERT(intVal == TEST_INT_VAL);

    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_STRING, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetString(TEST2_RESOURCE_STRING, TEST_STRING_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetString(TEST2_RESOURCE_STRING, (char*)&stringVal, sizeof(stringVal)) == LE_OK);
    LE_ASSERT(strcmp(stringVal, TEST_STRING_VAL) == 0);

    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_FLOAT, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat(TEST2_RESOURCE_FLOAT, TEST_FLOAT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetFloat(TEST2_RESOURCE_FLOAT, &floatVal) == LE_OK);
    LE_ASSERT(floatVal == TEST_FLOAT_VAL);

    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_BOOL, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetBool(TEST2_RESOURCE_BOOL, TEST_BOOL_VAL) == LE_OK);
    LE_ASSERT(le_avdata_GetBool(TEST2_RESOURCE_BOOL, &boolVal) == LE_OK);
    LE_ASSERT(boolVal == TEST_BOOL_VAL);

    // try to change all the variable to setting and make sure it errors
    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_INT, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_STRING, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_FLOAT, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);
    LE_ASSERT(le_avdata_CreateResource(TEST2_RESOURCE_BOOL, LE_AVDATA_ACCESS_SETTING) == LE_DUPLICATE);

    // check if we can create settings, but cannot set them from client side
    LE_ASSERT(le_avdata_CreateResource(TEST2_SETTING_INT, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(TEST2_SETTING_INT, TEST_INT_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetInt(TEST2_SETTING_INT, &intVal) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(TEST2_SETTING_STRING, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetString(TEST2_SETTING_STRING, TEST_STRING_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetString(TEST2_SETTING_STRING, (char*)&stringVal, sizeof(stringVal)) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(TEST2_SETTING_FLOAT, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetFloat(TEST2_SETTING_FLOAT, TEST_FLOAT_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetFloat(TEST2_SETTING_FLOAT, &floatVal) == LE_UNAVAILABLE);

    LE_ASSERT(le_avdata_CreateResource(TEST2_SETTING_BOOL, LE_AVDATA_ACCESS_SETTING) == LE_OK);
    LE_ASSERT(le_avdata_SetBool(TEST2_SETTING_BOOL, TEST_BOOL_VAL) == LE_NOT_PERMITTED);
    LE_ASSERT(le_avdata_GetBool(TEST2_SETTING_BOOL, &boolVal) == LE_UNAVAILABLE);

}

// test bad paths
static void TestBadPath()
{
    LE_ASSERT(le_avdata_CreateResource(TEST3_BAD_PATH1, LE_AVDATA_ACCESS_VARIABLE) == LE_FAULT);
    LE_ASSERT(le_avdata_CreateResource(TEST3_BAD_PATH2, LE_AVDATA_ACCESS_VARIABLE) == LE_FAULT);
    LE_ASSERT(le_avdata_CreateResource(TEST3_BAD_PATH3, LE_AVDATA_ACCESS_VARIABLE) == LE_FAULT);
    LE_ASSERT(le_avdata_CreateResource(TEST3_BAD_PATH4, LE_AVDATA_ACCESS_VARIABLE) == LE_FAULT);
    LE_ASSERT(le_avdata_CreateResource(TEST3_BAD_PATH5, LE_AVDATA_ACCESS_VARIABLE) == LE_FAULT);
    LE_ASSERT(le_avdata_CreateResource(TEST3_BAD_PATH6, LE_AVDATA_ACCESS_VARIABLE) == LE_FAULT);
    LE_ASSERT(le_avdata_CreateResource(TEST3_BAD_PATH7, LE_AVDATA_ACCESS_VARIABLE) == LE_FAULT);
}

COMPONENT_INIT
{
    LE_INFO("Start assetDataTest");

    // Test1: using dot as delimiter
    // Check if uncreated resources return LE_NOT_FOUND
    TestDotDelimitedPath();

    // Test2: using slash as delimiter
    // Check if uncreated resources return LE_NOT_FOUND
    TestSlashDelimitedPath();

    // Test3: Check if an error occurs in case of BAD paths
    TestBadPath();

    LE_INFO("assetDataTest successful");
}
