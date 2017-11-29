#include "legato.h"
#include "interfaces.h"

#define RESOURCE_A         "/test/resourceA"
#define RESOURCE_B         "/test/resourceB"
#define RESOURCE_C         "/test/resourceC"
#define RESOURCE_D         "/test/resourceD"

#define LOCAL_RESOURCE_A_INT_VAL            1
#define LOCAL_RESOURCE_B_INT_VAL            2
#define LOCAL_RESOURCE_C_INT_VAL            3
#define LOCAL_RESOURCE_D_INT_VAL            4

#define GLOBAL_RESOURCE_A_INT_VAL           11
#define GLOBAL_RESOURCE_B_INT_VAL           22
#define GLOBAL_RESOURCE_C_INT_VAL           33
#define GLOBAL_RESOURCE_D_INT_VAL           44

void NamespaceTest_01()
{
    int intVal;

    // Create resources (should be namespaced in app)
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_B, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_C, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_D, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);

    LE_ASSERT(le_avdata_SetInt(RESOURCE_A, LOCAL_RESOURCE_A_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_B, LOCAL_RESOURCE_B_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_C, LOCAL_RESOURCE_C_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_D, LOCAL_RESOURCE_D_INT_VAL) == LE_OK);

    // Switch to global namespace
    LE_ASSERT(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_GLOBAL) == LE_OK);

    // Should not exists
    LE_ASSERT(le_avdata_GetInt(RESOURCE_A, &intVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_A, &intVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_A, &intVal) == LE_NOT_FOUND);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_A, &intVal) == LE_NOT_FOUND);

    // Create these resources again but globally
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_B, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_C, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_D, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);

    LE_ASSERT(le_avdata_SetInt(RESOURCE_A, GLOBAL_RESOURCE_A_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_B, GLOBAL_RESOURCE_B_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_C, GLOBAL_RESOURCE_C_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_D, GLOBAL_RESOURCE_D_INT_VAL) == LE_OK);

    // Check global resource values
    LE_ASSERT(le_avdata_GetInt(RESOURCE_A, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_A, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_A_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_B, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_B, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_B_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_C, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_C, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_C_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_D, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_D, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_D_INT_VAL);

    // Switch back to application namespace and check our resources
    LE_ASSERT(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_APPLICATION) == LE_OK);

    LE_ASSERT(le_avdata_GetInt(RESOURCE_A, &intVal) == LE_OK);
    LE_INFO("Application namespace %s: %d", RESOURCE_A, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_A_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_B, &intVal) == LE_OK);
    LE_INFO("Application namespace %s: %d", RESOURCE_B, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_B_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_C, &intVal) == LE_OK);
    LE_INFO("Application namespace %s: %d", RESOURCE_C, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_C_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_D, &intVal) == LE_OK);
    LE_INFO("Application namespace %s: %d", RESOURCE_D, intVal);
    LE_ASSERT(intVal == LOCAL_RESOURCE_D_INT_VAL);

    // Read from server on /nsAssetDataTest/test/resourceA => 1
    // Read from server on /nsAssetDataTest/test/resourceB => 2
    // Read from server on /nsAssetDataTest/test/resourceC => 3
    // Read from server on /nsAssetDataTest/test/resourceD => 4

    // Read from server on /test/resourceA => 11
    // Read from server on /test/resourceB => 22
    // Read from server on /test/resourceC => 33
    // Read from server on /test/resourceD => 44

    LE_INFO("Pass");
}


void NamespaceTest_02()
{
    int intVal;

    // Switch to global namespace
    LE_ASSERT(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_GLOBAL) == LE_OK);

    // Create these resources again but globally
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_B, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_C, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_D, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);

    LE_ASSERT(le_avdata_SetInt(RESOURCE_A, GLOBAL_RESOURCE_A_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_B, GLOBAL_RESOURCE_B_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_C, GLOBAL_RESOURCE_C_INT_VAL) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_D, GLOBAL_RESOURCE_D_INT_VAL) == LE_OK);

        // Check global resource values
    LE_ASSERT(le_avdata_GetInt(RESOURCE_A, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_A, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_A_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_B, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_B, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_B_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_C, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_C, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_C_INT_VAL);
    LE_ASSERT(le_avdata_GetInt(RESOURCE_D, &intVal) == LE_OK);
    LE_INFO("Global namespace %s: %d", RESOURCE_D, intVal);
    LE_ASSERT(intVal == GLOBAL_RESOURCE_D_INT_VAL);
}


static void PushCallbackHandler
(
    le_avdata_PushStatus_t status,
    void* contextPtr
)
{
    int value = (int)contextPtr;
    if (status == LE_AVDATA_PUSH_SUCCESS)
    {
        LE_INFO("Push successful: %d", value);
    }
    else
    {
        LE_INFO("Push failed: %d", value);
    }
}


void NamespaceTest_03()
{
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_A, LOCAL_RESOURCE_A_INT_VAL) == LE_OK);

    // Switch to global namespace
    LE_ASSERT(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_GLOBAL) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_A, GLOBAL_RESOURCE_A_INT_VAL) == LE_OK);

    // Should push the global namespaced resource A
    LE_ASSERT(le_avdata_Push(RESOURCE_A, PushCallbackHandler, (void *)1) == LE_OK);
}


void NamespaceTest_04()
{
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_A, LOCAL_RESOURCE_A_INT_VAL) == LE_OK);

    // Switch to global namespace
    LE_ASSERT(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_GLOBAL) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    LE_ASSERT(le_avdata_SetInt(RESOURCE_A, GLOBAL_RESOURCE_A_INT_VAL) == LE_OK);

    // Should push the application namespaced resource A
    LE_ASSERT(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_APPLICATION) == LE_OK);
    LE_ASSERT(le_avdata_Push(RESOURCE_A, PushCallbackHandler, (void *)1) == LE_OK);
}

// A read on /nsAssetDataTest/test/resourceA should trigger this
static void ApplicationNsHandler
(
    const char* path,
    le_avdata_AccessType_t accessType,
    le_avdata_ArgumentListRef_t argumentList,
    void* contextPtr
)
{
    LE_INFO("Application namespaced handler: %s", path);
}

// A read on /test/resourceA should trigger this
static void GlobalNsHandler
(
    const char* path,
    le_avdata_AccessType_t accessType,
    le_avdata_ArgumentListRef_t argumentList,
    void* contextPtr
)
{
    LE_INFO("Global namespaced handler: %s", path);
}


void NamespaceTest_05()
{
    // Create resources (should be namespaced in app)
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    le_avdata_AddResourceEventHandler(RESOURCE_A, ApplicationNsHandler, NULL);

    // Switch to global namespace
    LE_ASSERT(le_avdata_SetNamespace(LE_AVDATA_NAMESPACE_GLOBAL) == LE_OK);
    LE_ASSERT(le_avdata_CreateResource(RESOURCE_A, LE_AVDATA_ACCESS_VARIABLE) == LE_OK);
    le_avdata_AddResourceEventHandler(RESOURCE_A, GlobalNsHandler, NULL);
}


COMPONENT_INIT
{
    LE_INFO("Start nsAssetDataTest");

    int testCase = 0;

    if (le_arg_NumArgs() >= 1)
    {
        const char* arg1 = le_arg_GetArg(0);
        testCase = atoi(arg1);
    }

    switch (testCase)
    {
        case 1:
            NamespaceTest_01();
            break;
        case 2:
            NamespaceTest_02();
            break;
        case 3:
            NamespaceTest_03();
            break;
        case 4:
            NamespaceTest_04();
            break;
        case 5:
            NamespaceTest_05();
            break;
        default:
            LE_INFO("Invalid test case");
            break;
    }
}