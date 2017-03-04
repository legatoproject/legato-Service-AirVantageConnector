/**
 * @file avData.c
 *
 * This implements the avdata API.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */


#include "tinycbor/cbor.h"
#include "legato.h"
#include "interfaces.h"
#include "lwm2mcoreCoapHandlers.h"
#include "timeseriesData.h"


//--------------------------------------------------------------------------------------------------
/**
 * Maximum expected number of asset data.  From AtlasCopco use cases.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_EXPECTED_ASSETDATA 256


//--------------------------------------------------------------------------------------------------
/**
 * Map containing asset data.
 */
//--------------------------------------------------------------------------------------------------
static le_hashmap_Ref_t AssetDataMap;


//--------------------------------------------------------------------------------------------------
/**
 * Map containing safe refs of resource event handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t ResourceEventHandlerMap;


//--------------------------------------------------------------------------------------------------
/**
 * Map containing safe refs of argument lists (for resource event handlers).
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t ArgListRefMap;


//--------------------------------------------------------------------------------------------------
/**
 * Asset path memory pool.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t AssetPathPool;


//--------------------------------------------------------------------------------------------------
/**
 * Asset data memory pool.
 */

//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t AssetDataPool;


//--------------------------------------------------------------------------------------------------
/**
 * String memory pool.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t StringPool;


//--------------------------------------------------------------------------------------------------
/**
 * Argument memory pool.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t ArgumentPool;


//--------------------------------------------------------------------------------------------------
/**
 * List of taboo first level path names, to avoid path names resembling standard lwm2m paths.
 */
//--------------------------------------------------------------------------------------------------
static const char* InvalidFirstLevelPathNames[] =
{
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "10241",
    "10242",
    "10243"
};


//--------------------------------------------------------------------------------------------------
/**
 * Structure representing an asset value - a union of the possible types.
 */
//--------------------------------------------------------------------------------------------------
typedef union
{
    int intValue;
    double floatValue;
    bool boolValue;
    char* strValuePtr;
}
AssetValue_t;


//--------------------------------------------------------------------------------------------------
/**
 * Structure representing an asset data.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avdata_AccessType_t access;  ///< Permitted access to this asset data.
    le_avdata_DataType_t dataType;  ///< Data type of the Asset Value.
    AssetValue_t value;             ///< Asset Value.
    le_avdata_ResourceHandlerFunc_t handlerPtr; ///< Registered handler when asset data is accessed.
    void* contextPtr;               // Client context for the handler.
    le_dls_List_t arguments;        // Argument list for the handler.
}
AssetData_t;


//--------------------------------------------------------------------------------------------------
/**
 * Structure representing an argument in an Argument List.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char* argumentName;
    le_avdata_DataType_t argValType;
    AssetValue_t argValue;
    le_dls_Link_t link;
}
Argument_t;


//--------------------------------------------------------------------------------------------------
/**
 * Data associated with an record reference. This is used for keeping track of which client
 * is using the record ref, so that everything can be cleaned up when the client dies.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avdata_RecordRef_t recRef;               ///< Record ref
    le_msg_SessionRef_t clientSessionRef;       ///< Client using this record ref
}
RecordRefData_t;


//--------------------------------------------------------------------------------------------------
/**
 * Record reference data memory pool. Used for keeping track of the client that is using a
 * specific record ref. Initialized in avData_Init().
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t RecordRefDataPoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
 * Safe Reference Map for record references. Initialized in avData_Init().
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t RecordRefMap;


////////////////////////////////////////////////////////////////////////////////////////////////////
/* Helper funcitons                                                                               */
////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------
/**
 * Handler for client session closes
 */
//--------------------------------------------------------------------------------------------------
static void ClientCloseSessionHandler
(
    le_msg_SessionRef_t sessionRef,
    void*               contextPtr
)
{
    // Search for the record references used by the closed client, and clean up any data.
    le_ref_IterRef_t iterRef = le_ref_GetIterator(RecordRefMap);
    RecordRefData_t* recRefDataPtr;

    while ( le_ref_NextNode(iterRef) == LE_OK )
    {
        recRefDataPtr = le_ref_GetValue(iterRef);

        if ( recRefDataPtr->clientSessionRef == sessionRef )
        {
            // Delete instance data, and also delete asset data, if last instance is deleted
            timeSeries_Delete(recRefDataPtr->recRef);

            // Delete safe reference and associated data
            le_mem_Release((void*)recRefDataPtr);
            le_ref_DeleteRef( RecordRefMap, (void*)le_ref_GetSafeRef(iterRef) );
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Translates an asset data type to a string.
 */
//--------------------------------------------------------------------------------------------------
static const char* GetDataTypeStr
(
    le_avdata_DataType_t dataType ///< [IN] Asset data type.
)
{
    switch (dataType)
    {
        case LE_AVDATA_DATA_TYPE_NONE:
            return "none";
            break;

        case LE_AVDATA_DATA_TYPE_INT:
            return "int";
            break;

        case LE_AVDATA_DATA_TYPE_FLOAT:
            return "float";
            break;

        case LE_AVDATA_DATA_TYPE_BOOL:
            return "bool";
            break;

        case LE_AVDATA_DATA_TYPE_STRING:
            return "string";
            break;

        default:
            return "invalid";
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Converts asset data access mode to bit mask of access types.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ConvertAccessModeToMask
(
    le_avdata_AccessMode_t accessMode,       ///< [IN] access mode.
    le_avdata_AccessType_t* accessBitMaskPtr ///< [OUT] bitmask of access types.
)
{
    le_avdata_AccessType_t mask = 0;

    switch (accessMode)
    {
        case LE_AVDATA_ACCESS_VARIABLE:
            mask = LE_AVDATA_ACCESS_READ;
            break;

        case LE_AVDATA_ACCESS_SETTING:
            mask = LE_AVDATA_ACCESS_READ | LE_AVDATA_ACCESS_WRITE;
            break;

        case LE_AVDATA_ACCESS_COMMAND:
            mask = LE_AVDATA_ACCESS_EXEC;
            break;

        default:
            return LE_FAULT;
    }

    *accessBitMaskPtr = mask;
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Check if the asset data path is legal. The path cannot resemble a lwm2m object.
 */
//--------------------------------------------------------------------------------------------------
static bool IsAssetDataPathValid
(
    const char* path ///< [IN] Asset data path
)
{
    char* pathDup = strdup(path);
    char* firstLevelPath = strtok(pathDup, "/");

    int i;
    for (i = 0; i < NUM_ARRAY_MEMBERS(InvalidFirstLevelPathNames); i++)
    {
        if (strcmp(firstLevelPath, InvalidFirstLevelPathNames[i]) == 0)
        {
            free(pathDup);
            return false;
        }
    }

    free(pathDup);
    return true;
}


//--------------------------------------------------------------------------------------------------
/**
 * Return true if the provided path is a parent to any of the asset data paths in the hashmap.
 */
//--------------------------------------------------------------------------------------------------
static bool IsPathParent
(
    const char* path ///< [IN] Asset data path
)
{
    le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(AssetDataMap);

    while (le_hashmap_NextNode(iter) == LE_OK)
    {
        if (le_path_IsSubpath(path, le_hashmap_GetKey(iter), "/"))
        {
            return true;
        }
    }

    return false;
}


//--------------------------------------------------------------------------------------------------
/**
 * Looks up an argument in the argument list with the argument name.
 *
 * @return:
 *      - argument ref if found
 *      - NULL if not found
 */
//--------------------------------------------------------------------------------------------------
static Argument_t* GetArg
(
    le_avdata_ArgumentListRef_t argumentListRef, ///< [IN] Argument list.
    const char* argName                          ///< [IN] Argument name.
)
{
    Argument_t* argPtr = NULL;
    le_dls_List_t* argListPtr = le_ref_Lookup(ArgListRefMap, argumentListRef);
    le_dls_Link_t* argLinkPtr = le_dls_Peek(argListPtr);

    while (argLinkPtr != NULL)
    {
        argPtr = CONTAINER_OF(argLinkPtr, Argument_t, link);

        if (strcmp(argPtr->argumentName, argName) == 0)
        {
            return argPtr;
        }

        argLinkPtr = le_dls_PeekNext(argListPtr, argLinkPtr);
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Looks up the asset data in the AssetDataMap with the provided path.
 *
 * @return:
 *      - asset data ref if found
 *      - NULL if not found
 */
//--------------------------------------------------------------------------------------------------
static AssetData_t* GetAssetData
(
    const char* path  ///< [IN] Asset data path
)
{
    AssetData_t* assetDataPtr = NULL;

    le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(AssetDataMap);

    while (le_hashmap_NextNode(iter) == LE_OK)
    {
        if (strcmp(path, le_hashmap_GetKey(iter)) == 0)
        {
            assetDataPtr = le_hashmap_GetValue(iter);
            break;
        }
    }

    return assetDataPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the asset value associated with the provided asset data path.
 *
 * @return:
 *      - LE_NOT_FOUND - if the path is invalid and does not point to an asset data
 *      - LE_NOT_PERMITTED - asset data being accessed does not have the right permission
 *      - LE_OK - access successful.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetVal
(
    const char* path,                  ///< [IN] Asset data path
    AssetValue_t* valuePtr,            ///< [OUT] Asset value
    le_avdata_DataType_t* dataTypePtr, ///< [OUT] Asset value data type
    bool isClient                      ///< [IN] Is it client or server access
)
{
    AssetData_t* assetDataPtr = GetAssetData(path);

    if (assetDataPtr == NULL)
    {
        return LE_NOT_FOUND;
    }

    // Check access permission
    if (!isClient && ((assetDataPtr->access & LE_AVDATA_ACCESS_READ) != LE_AVDATA_ACCESS_READ))
    {
        LE_ERROR("Asset (%s) does not have read permission.", path);
        return LE_NOT_PERMITTED;
    }

    // Call registered handler, which must be done before reading the value, so the handler
    // function has a chance to get the updated value from hardware.
    if ((!isClient) && (assetDataPtr->handlerPtr != NULL))
    {
        le_avdata_ArgumentListRef_t argListRef
             = le_ref_CreateRef(ArgListRefMap, &assetDataPtr->arguments);

        le_result_t commandResult = LE_OK;

        assetDataPtr->handlerPtr(path, LE_AVDATA_ACCESS_READ,
                                 argListRef, &commandResult, assetDataPtr->contextPtr);

        le_ref_DeleteRef(ArgListRefMap, argListRef);
    }

    // Must be done after handler is called.
    *valuePtr = assetDataPtr->value;
    *dataTypePtr = assetDataPtr->dataType;

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Sets the asset value associated with the provided asset data path.
 *
 * @return:
 *      - LE_NOT_FOUND - if the path is invalid and does not point to an asset data
 *      - LE_NOT_PERMITTED - asset data being accessed does not have the right permission
 *      - LE_OK - access successful.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetVal
(
    const char* path,              ///< [IN] Asset data path
    AssetValue_t value,            ///< [IN] Asset value
    le_avdata_DataType_t dataType, ///< [IN] Asset value data type
    bool isClient                  ///< [IN] Is it client or server access
)
{
    AssetData_t* assetDataPtr = GetAssetData(path);

    if (assetDataPtr == NULL)
    {
        return LE_NOT_FOUND;
    }

    // Check access permission
    if (!isClient && ((assetDataPtr->access & LE_AVDATA_ACCESS_WRITE) != LE_AVDATA_ACCESS_WRITE))
    {
        LE_ERROR("Asset (%s) does not have write permission.", path);
        return LE_NOT_PERMITTED;
    }

    // If the current data type is string, we need to free the memory for the string before
    // assigning asset value to the new one.
    if (assetDataPtr->dataType == LE_AVDATA_DATA_TYPE_STRING)
    {
        le_mem_Release(assetDataPtr->value.strValuePtr);
    }

    // Must be done before handler is called.
    assetDataPtr->value = value;
    assetDataPtr->dataType = dataType;

    // Call registered handler, which must be done after writing the value, so the handler
    // function can update the hardware with the latest value.
    if ((!isClient) && (assetDataPtr->handlerPtr != NULL))
    {

        le_avdata_ArgumentListRef_t argListRef
             = le_ref_CreateRef(ArgListRefMap, &assetDataPtr->arguments);

        le_result_t commandResult = LE_OK;

        assetDataPtr->handlerPtr(path, LE_AVDATA_ACCESS_WRITE,
                                 argListRef, &commandResult, assetDataPtr->contextPtr);

        le_ref_DeleteRef(ArgListRefMap, argListRef);

    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * String compare function for qsort.
 */
//--------------------------------------------------------------------------------------------------
static int CompareStrings
(
    const void *a,
    const void *b
)
{
    return strcmp(*(char **)a, *(char **)b);
}


//--------------------------------------------------------------------------------------------------
/**
 * Given a list of asset data paths, look up the associated asset value, and encode them in CBOR
 * format with the provided CBOR encoder.
 *
 * Notes:
 *
 * 1. The list of paths is assumed to be grouped at each level. They don't need to be sorted, but a
 *    sorted list achieves the same goal.
 *
 * 2. At each level, a recursive call is made on a range of paths of the same node name at that
 *    level. A CBOR "map" is created for such range of paths.
 *
 * 3. Recursion ends when the non-existing child node of a leaf node is reached. It can't end at a
 *    leaf node because it might still have peer branch nodes to be processed. A leaf node certainly
 *    does not have any peer nodes of the same name, so the next rescursive call will certainly have
 *    a range of 1, where we can safely end the recursion
 */
//--------------------------------------------------------------------------------------------------
static void EncodeMultiData
(
    char* list[], ///< [IN] List of asset data paths
    CborEncoder* parentCborEncoder, ///< [OUT] Parent CBOR encoder
    int minIndex, ///< [IN] Min index of the list to start with in the current recursion
    int maxIndex, ///< [IN] Max index of the list to end with in the current recursion
    int level     ///< [IN] Path depth for the current recursion
)
{
    char* currToken = "";
    char* savedToken = "";
    char* peekToken = "";

    int minCurrRange = minIndex;
    int maxCurrRange = minIndex;

    int i, j;
    for (i = minIndex; i <= maxIndex; i++)
    {
        char* currStrCopy = strdup(list[i]);

        currToken = strtok(currStrCopy, "/");
        for (j = 1; j < level; j++)
        {
            currToken = strtok(NULL, "/");
        }

        // Ending condition
        if (currToken == NULL)
        {
            return;
        }

        peekToken = strtok(NULL, "/");

        if (peekToken == NULL)
        {
            // When a leaf node is encountered, we need to make recursive calls on the previous
            // range of branch nodes.
            if (strcmp(savedToken, "") != 0)
            {
                maxCurrRange = i - 1;

                CborEncoder someEncoder1;
                cbor_encode_text_stringz(parentCborEncoder, savedToken);
                cbor_encoder_create_map(parentCborEncoder, &someEncoder1, CborIndefiniteLength);

                // recursive call
                EncodeMultiData(list, &someEncoder1, minCurrRange, maxCurrRange, level+1);

                cbor_encoder_close_container(parentCborEncoder, &someEncoder1);
            }

            // CBOR encoding for the leaf node itself.
            cbor_encode_text_stringz(parentCborEncoder, currToken);

            // Use the path to look up its asset data, and do the corresponding encoding.
            AssetValue_t assetValue;
            le_avdata_DataType_t type;

            le_result_t result = GetVal(list[i], &assetValue, &type, false);
            LE_ASSERT(result == LE_OK);

            switch (type)
            {
                case LE_AVDATA_DATA_TYPE_NONE:

                    // TODO: TBD.
                    // data type is none if the asset data has not been accessed.
                    // so it contains no value anyway.
                    // Should we return some sort of default like 0, or reply with error?
                    // If the former, then when asset data is created, the type needs to be known
                    // already.

                    // My preference is to return nothing (instead of a default value of 0). Since
                    // 0 could mean something. However at this point we are already half way into
                    // encoding... unless we exclude paths with data type of none.

                    break;

                case LE_AVDATA_DATA_TYPE_INT:
                    cbor_encode_int(parentCborEncoder, assetValue.intValue);
                    break;

                case LE_AVDATA_DATA_TYPE_FLOAT:
                    cbor_encode_double(parentCborEncoder, assetValue.floatValue);
                    break;

                case LE_AVDATA_DATA_TYPE_BOOL:
                    cbor_encode_boolean(parentCborEncoder, assetValue.boolValue);
                    break;

                case LE_AVDATA_DATA_TYPE_STRING:
                    // TODO: maybe stringz is also ok?
                    cbor_encode_text_string(parentCborEncoder,
                                            assetValue.strValuePtr, strlen(assetValue.strValuePtr));
                    break;

                default:
                    // TODO: TBD
                    LE_ERROR("unexpected data type");
            }

            // recursive call on a phantom child in order to end the recursion
            CborEncoder someEncoder;
            EncodeMultiData(list, &someEncoder, i, i, level+1);

            //TODO: necessary?
            if (strcmp(savedToken, "") != 0)
            {
                free(savedToken);
            }
            savedToken = "";
        }
        else if (strcmp(currToken, savedToken) != 0)
        {
            // we have encountered a "new" branch node, so make recursive call on the saved range.
            if (strcmp(savedToken, "") != 0)
            {
                maxCurrRange = i - 1;

                CborEncoder someEncoder;
                cbor_encode_text_stringz(parentCborEncoder, savedToken);
                cbor_encoder_create_map(parentCborEncoder, &someEncoder, CborIndefiniteLength);

                // recursive call
                EncodeMultiData(list, &someEncoder, minCurrRange, maxCurrRange, level+1);

                cbor_encoder_close_container(parentCborEncoder, &someEncoder);
            }

            minCurrRange = i;
            maxCurrRange = i;

            //TODO: necessary?
            if (strcmp(savedToken, "") != 0)
            {
                free(savedToken);
            }
            savedToken = strdup(currToken);
        }

        free(currStrCopy);
    }

    if (peekToken != NULL)
    {
        maxCurrRange = i - 1;

        CborEncoder someEncoder;
        cbor_encode_text_stringz(parentCborEncoder, savedToken);
        cbor_encoder_create_map(parentCborEncoder, &someEncoder, CborIndefiniteLength);

        // recursive call
        EncodeMultiData(list, &someEncoder, minCurrRange, maxCurrRange, level+1);

        cbor_encoder_close_container(parentCborEncoder, &someEncoder);
    }
}


// TODO: error handling - return LE status code? and log CBOR error codes?
//       also add similar error detection in EncodeMultiData?
//--------------------------------------------------------------------------------------------------
/**
 * Decode the CBOR data and with the provided path as the base path, set the asset data values for
 * asset data paths.
 */
//--------------------------------------------------------------------------------------------------
static CborError DecodeMultiData
(
    CborValue* it,
    char* path
)
{
    size_t endingPathSegLen = 0;
    bool labelProcessed = false;

    while (!cbor_value_at_end(it))
    {
        CborError err;
        CborType type = cbor_value_get_type(it);

        if (type != CborTextStringType)
        {
            labelProcessed = false;
        }

        switch (type)
        {
            case CborMapType:
            {
                // recursive type
                CborValue recursed;
                assert(cbor_value_is_container(it));

                err = cbor_value_enter_container(it, &recursed);
                if (err)
                    return err;       // parse error

                char pathDup[512] = {0};
                strncpy(pathDup, path, 512);
                err = DecodeMultiData(&recursed, pathDup);
                if (err)
                    return err;       // parse error

                path[strlen(path) - (endingPathSegLen + 1)] = '\0';

                err = cbor_value_leave_container(it, &recursed);
                if (err)
                    return err;       // parse error

                continue;
            }

            case CborTextStringType:
            {
                char *buf;
                size_t n;
                err = cbor_value_dup_text_string(it, &buf, &n, it);
                if (err)
                    return err;     // parse error

                if (!labelProcessed)
                {
                    endingPathSegLen = strlen(buf);

                    strcat(path, "/");
                    strcat(path, buf);

                    labelProcessed = true;
                }
                else
                {
                    AssetValue_t assetValue;
                    assetValue.strValuePtr = le_mem_ForceAlloc(StringPool);
                    strncpy(assetValue.strValuePtr, buf, LE_AVDATA_STRING_VALUE_LEN);
                    le_result_t result = SetVal(path, assetValue,
                                                LE_AVDATA_DATA_TYPE_STRING, false);
                    // TODO: return error
                    LE_ASSERT(result == LE_OK);

                    path[strlen(path) - endingPathSegLen] = '\0';

                    labelProcessed = false;
                }

                free(buf);

                continue;
            }

            case CborIntegerType:
            {
                int val;
                cbor_value_get_int(it, &val);     // can't fail

                AssetValue_t assetValue;
                assetValue.intValue = val;
                le_result_t result = SetVal(path, assetValue, LE_AVDATA_DATA_TYPE_INT, false);
                // TODO: return error
                LE_ASSERT(result == LE_OK);

                path[strlen(path) - endingPathSegLen] = '\0';

                break;
            }

            case CborBooleanType:
            {
                bool val;
                cbor_value_get_boolean(it, &val);       // can't fail

                AssetValue_t assetValue;
                assetValue.boolValue = val;
                le_result_t result = SetVal(path, assetValue, LE_AVDATA_DATA_TYPE_BOOL, false);
                // TODO: return error
                LE_ASSERT(result == LE_OK);

                path[strlen(path) - endingPathSegLen] = '\0';

                break;
            }

            case CborDoubleType:
            {
                double val;
                cbor_value_get_double(it, &val);

                AssetValue_t assetValue;
                assetValue.floatValue = val;
                le_result_t result = SetVal(path, assetValue, LE_AVDATA_DATA_TYPE_FLOAT, false);
                // TODO: return error
                LE_ASSERT(result == LE_OK);

                path[strlen(path) - endingPathSegLen] = '\0';

                break;
            }

            default:
                LE_ERROR("Server payload contains unexpected CBOR type: %d", type);
        }

        err = cbor_value_advance_fixed(it);
        if (err)
            return err;
    }
    return CborNoError;
}


// DEBUG purposes only
static void DumpArgList
(
    le_dls_List_t* argListPtr
)
{
    LE_INFO("#### DUMPING  ARUGMENT LIST ########################################################");

    Argument_t* argPtr = NULL;
    le_dls_Link_t* argLinkPtr = le_dls_Peek(argListPtr);

    while (argLinkPtr != NULL)
    {
        argPtr = CONTAINER_OF(argLinkPtr, Argument_t, link);

        LE_INFO("arg name: %s", argPtr->argumentName);

        switch (argPtr->argValType)
        {
            case LE_AVDATA_DATA_TYPE_NONE:
                LE_INFO("none");
                break;

            case LE_AVDATA_DATA_TYPE_INT:
                LE_INFO("int arg val:    [%d]", argPtr->argValue.intValue);
                break;

            case LE_AVDATA_DATA_TYPE_FLOAT:
                LE_INFO("float arg val:  [%g]", argPtr->argValue.floatValue);
                break;

            case LE_AVDATA_DATA_TYPE_BOOL:
                LE_INFO("bool arg val:   [%d]", argPtr->argValue.boolValue);
                break;

            case LE_AVDATA_DATA_TYPE_STRING:
                LE_INFO("string arg val: [%s]", argPtr->argValue.strValuePtr);
                break;

            default:
                LE_INFO("invalid");
        }

        argLinkPtr = le_dls_PeekNext(argListPtr, argLinkPtr);
    }
    LE_INFO("#### END OF DUMPING  ARUGMENT LIST #################################################");

}


// TODO: REVIEWERS - this function is messy and needs refactoring.  Also debug messages are needed
//       for now to troubleshoot some issues.
//--------------------------------------------------------------------------------------------------
/**
 * Handles requests from an AV server to read, write, or execute on an asset data.
 */
//--------------------------------------------------------------------------------------------------
static void AvServerRequestHandler
(
    lwm2mcore_coapRequestRef_t serverReqRef
)
{
    int sessionContext = avcClient_GetContext();
    LE_ASSERT(sessionContext != 0);


    const char* path;
    uint8_t* payload;
    size_t payloadLen;
    coap_method_t method;
    uint8_t* token;
    size_t tokenLength;
    unsigned int contentType;

    path = lwm2mcore_GetRequestUri(serverReqRef);
    method = lwm2mcore_GetRequestMethod(serverReqRef);

    payload = lwm2mcore_GetRequestPayload(serverReqRef);
    payloadLen = lwm2mcore_GetRequestPayloadLength(serverReqRef);

    token = lwm2mcore_GetToken(serverReqRef);
    tokenLength = lwm2mcore_GetTokenLength(serverReqRef);
    os_debug_data_dump("COAP tokenPtr3 token", token, tokenLength);

    contentType = lwm2mcore_GetContentType(serverReqRef);

    lwm2mcore_coapResponse_t response;


    memcpy(response.token, token, tokenLength);
    response.tokenLength = tokenLength;
    response.contentType = LWM2M_CONTENT_CBOR;


    LE_INFO(">>>>> Request Uri is: [%s]", path);


    switch (method)
    {
        case COAP_GET: // server reads from device
        {
            LE_INFO(">>>>> COAP_GET - Server reads from device");

            AssetValue_t assetValue;
            le_avdata_DataType_t type;

            le_result_t result = GetVal(path, &assetValue, &type, false);

            if (result == LE_OK)
            {
                LE_INFO(">>>>> Reading single data point.");

                // Encode the asset data value.
                uint8_t buf[LE_AVDATA_STRING_VALUE_LEN + 1];
                CborEncoder encoder;
                cbor_encoder_init(&encoder, (uint8_t*)&buf, sizeof(buf), 0);

                switch (type)
                {
                    case LE_AVDATA_DATA_TYPE_NONE:

                        // TODO: TBD.
                        break;

                    case LE_AVDATA_DATA_TYPE_INT:
                        cbor_encode_int(&encoder, assetValue.intValue);
                        break;

                    case LE_AVDATA_DATA_TYPE_FLOAT:
                        cbor_encode_double(&encoder, assetValue.floatValue);
                        break;

                    case LE_AVDATA_DATA_TYPE_BOOL:
                        cbor_encode_boolean(&encoder, assetValue.boolValue);
                        break;

                    case LE_AVDATA_DATA_TYPE_STRING:
                        // TODO: maybe stringz is also ok?
                        cbor_encode_text_string(&encoder,
                                                assetValue.strValuePtr,
                                                strlen(assetValue.strValuePtr));
                        break;

                    default:
                        // TODO: TBD
                        LE_ERROR("unexpected data type");
                }

                // TODO: call  lwm2mcore_SendAsyncResponse  with buf
                // 1. Reading a single data point, success [2.05 Content]
                // COAP_CONTENT_AVAILABLE

                response.code = COAP_CONTENT_AVAILABLE;
                response.payload = buf;
                // TODO: check encoder error in case of buffer overflow
                response.payloadLength = cbor_encoder_get_buffer_size(&encoder, buf);

                LE_INFO("response.payloadLength = %d", response.payloadLength);

                lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);
            }
            else if (result == LE_NOT_PERMITTED)
            {
                LE_INFO(">>>>> no permission. Replying COAP_METHOD_UNAUTHORIZED.");

                // TODO: call  lwm2mcore_SendAsyncResponse  with error
                // 2. Reading a single data point, no permission [4.01 Unauthorized]
                // COAP_METHOD_UNAUTHORIZED

                response.code = COAP_METHOD_UNAUTHORIZED;
                response.payload = NULL;
                response.payloadLength = 0;

                lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

            }
            else if (result == LE_NOT_FOUND)
            {
                // The path contain children nodes, so there might be multiple asset data under it.
                if (IsPathParent(path))
                {
                    LE_INFO(">>>>> path not found, but is parent path.");

                    AssetData_t* assetDataPtr;
                    char* pathArray[le_hashmap_Size(AssetDataMap)];
                    memset(pathArray, 0, sizeof(pathArray));
                    int pathArrayIdx = 0;

                    le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(AssetDataMap);
                    char* currentPath;

                    while (le_hashmap_NextNode(iter) == LE_OK)
                    {
                        currentPath = le_hashmap_GetKey(iter);
                        assetDataPtr = le_hashmap_GetValue(iter);

                        // TODO: do we need to check if data type is valid (not NONE)?) tbd.
                        if ((le_path_IsSubpath(path, currentPath, "/")) &&
                            ((assetDataPtr->access & LE_AVDATA_ACCESS_READ) == LE_AVDATA_ACCESS_READ))
                        {
                            // Put the currentPath in the path array.
                            pathArray[pathArrayIdx] = currentPath;
                            pathArrayIdx++;
                        }
                    }

                    // sort the path array
                    qsort(pathArray, pathArrayIdx, sizeof(*pathArray), CompareStrings);

                    // compose CBOR buffer
                    // TODO: Estimate buffer size
                    uint8_t buf[1024] = {0};
                    CborEncoder rootNode, mapNode;

                    cbor_encoder_init(&rootNode, (uint8_t*)&buf, sizeof(buf), 0);
                    cbor_encoder_create_map(&rootNode, &mapNode, CborIndefiniteLength);

                    EncodeMultiData(pathArray, &mapNode, 0, (pathArrayIdx - 1), 1);

                    cbor_encoder_close_container(&rootNode, &mapNode);

                    // TODO: call  lwm2mcore_SendAsyncResponse  with buf

                    // 4. Reading multiple data points (with multiple data loads) [2.05 Content]
                    // COAP_CONTENT_AVAILABLE

                    response.code = COAP_CONTENT_AVAILABLE;
                    response.payload = buf;
                    // TODO: check encoder error in case of buffer overflow
                    response.payloadLength = cbor_encoder_get_buffer_size(&rootNode, buf);

                    LE_INFO("response.payloadLength = %d", response.payloadLength);

                    lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                }
                // The path contains no children nodes.
                else
                {
                    LE_INFO(">>>>> path not found. Replying COAP_RESOURCE_NOT_FOUND.");

                    // TODO: call  lwm2mcore_SendAsyncResponse  with "not found"
                    // 3. Reading a single data point, asset data not found [4.04 Not Found]
                    // COAP_RESOURCE_NOT_FOUND

                    response.code = COAP_RESOURCE_NOT_FOUND;
                    response.payload = NULL;
                    response.payloadLength = 0;

                    lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                }
            }
            else
            {
                LE_FATAL("Unexpected result status: %s", LE_RESULT_TXT(result));
            }

            break;
        }
        case COAP_PUT: // server writes to device
        {
            LE_INFO(">>>>> COAP_PUT - Server writes to device");

            CborParser parser;
            CborValue value;

            cbor_parser_init(payload, payloadLen, 0, &parser, &value);

            // The payload would either contain a value for a single data point, or a map.
            if (cbor_value_is_map(&value))
            {
                LE_INFO(">>>>> AV server sent a map.");

                AssetData_t* assetDataPtr = GetAssetData(path);

                // Check if path exists. If it does, then it's impossible to have children nodes.
                // Therefore return error.
                if (assetDataPtr != NULL)
                {
                    LE_INFO(">>>>> Server writes to an existing path. Replying COAP_BAD_REQUEST.");

                    // TODO: call lwm2mcore_SendAsyncResponse with error
                    // COAP_BAD_REQUEST

                    response.code = COAP_BAD_REQUEST;
                    response.payload = NULL;
                    response.payloadLength = 0;

                    lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                }
                else
                {
                    LE_INFO(">>>>> Server writes to a non-existing path.");

                    if (IsPathParent(path))
                    {
                        LE_INFO(">>>>> path is parent. Attempting to write the multi-value.");

                        // If the path is a parent path, then call DecodeMultiData on that path.
                        // Return status of write.

                        // TODO: might need to check if path contains trailing separator

                        CborError result = DecodeMultiData(&value, path);

                        // TODO: call lwm2mcore_SendAsyncResponse depending on the decoding result
                        // COAP_RESOURCE_CHANGED if ALL paths are success
                        // COAP_BAD_REQUEST if one path is failure

                        response.payload = NULL;
                        response.payloadLength = 0;

                        response.code = (result == CborNoError) ?
                                        COAP_RESOURCE_CHANGED : COAP_BAD_REQUEST;

                        lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                    }
                    else
                    {
                        LE_INFO(">>>>> path is not parent. Replying COAP_BAD_REQUEST.");

                        // If the path doesn't exist, check if it's a parent path. If it isn't,
                        // then return error. (note that resource creation from server isn't
                        // supported)

                        // TODO: call lwm2mcore_SendAsyncResponse with error
                        // COAP_BAD_REQUEST

                        response.code = COAP_BAD_REQUEST;
                        response.payload = NULL;
                        response.payloadLength = 0;

                        lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                    }
                }
            }
            // Assume this is the case with a value for a single data point.
            else
            {
                LE_INFO(">>>>> AV server sent a single value.");

                // Decode the value and call SetVal.  Return success or failure depending on the
                // return status.
                CborType type = cbor_value_get_type(&value);

                switch (type)
                {
                    case CborTextStringType:
                    {
                        LE_INFO(">>>>> string");

                        size_t decodedValLen = LE_AVDATA_STRING_VALUE_LEN;
                        char decodedVal[decodedValLen];
                        cbor_value_copy_text_string(&value, decodedVal, &decodedValLen, NULL);

                        AssetValue_t assetValue;
                        assetValue.strValuePtr = le_mem_ForceAlloc(StringPool);
                        strncpy(assetValue.strValuePtr, decodedVal, LE_AVDATA_STRING_VALUE_LEN);

                        le_result_t result = SetVal(path, assetValue,
                                                    LE_AVDATA_DATA_TYPE_STRING, false);

                        // TODO: call lwm2mcore_SendAsyncResponse based on "result"

                        // 1. Writing a single data point, success  [2.04 Changed]
                        // COAP_RESOURCE_CHANGED

                        // 2. Writing a single data point, no permission  [4.01 Unauthorized]
                        // COAP_METHOD_UNAUTHORIZED

                        // 3. Writing a single data point, asset data not found  [4.04 Not Found]
                        // COAP_RESOURCE_NOT_FOUND

                        response.payload = NULL;
                        response.payloadLength = 0;

                        switch (result)
                        {
                            case LE_OK:
                                response.code = COAP_RESOURCE_CHANGED;
                                break;
                            case LE_NOT_PERMITTED:
                                response.code = COAP_METHOD_UNAUTHORIZED;
                                break;
                            case LE_NOT_FOUND:
                                response.code = COAP_RESOURCE_NOT_FOUND;
                                break;
                        }

                        lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                        break;
                    }
                    case CborIntegerType:
                    {
                        LE_INFO(">>>>> int");

                        int decodedVal;
                        cbor_value_get_int(&value, &decodedVal);

                        AssetValue_t assetValue;
                        assetValue.intValue = decodedVal;

                        le_result_t result = SetVal(path, assetValue,
                                                    LE_AVDATA_DATA_TYPE_INT, false);

                        // TODO: call lwm2mcore_SendAsyncResponse based on "result"
                        //   same as string

                        response.payload = NULL;
                        response.payloadLength = 0;

                        switch (result)
                        {
                            case LE_OK:
                                response.code = COAP_RESOURCE_CHANGED;
                                break;
                            case LE_NOT_PERMITTED:
                                response.code = COAP_METHOD_UNAUTHORIZED;
                                break;
                            case LE_NOT_FOUND:
                                response.code = COAP_RESOURCE_NOT_FOUND;
                                break;
                        }

                        lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                        break;
                    }
                    case CborBooleanType:
                    {
                        LE_INFO(">>>>> bool");

                        bool decodedVal;
                        cbor_value_get_boolean(&value, &decodedVal);

                        AssetValue_t assetValue;
                        assetValue.boolValue = decodedVal;

                        le_result_t result = SetVal(path, assetValue,
                                                    LE_AVDATA_DATA_TYPE_BOOL, false);

                        // TODO: call lwm2mcore_SendAsyncResponse based on "result"
                        //   same as string

                        response.payload = NULL;
                        response.payloadLength = 0;

                        switch (result)
                        {
                            case LE_OK:
                                response.code = COAP_RESOURCE_CHANGED;
                                break;
                            case LE_NOT_PERMITTED:
                                response.code = COAP_METHOD_UNAUTHORIZED;
                                break;
                            case LE_NOT_FOUND:
                                response.code = COAP_RESOURCE_NOT_FOUND;
                                break;
                        }

                        lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                        break;
                    }
                    case CborDoubleType:
                    {
                        LE_INFO(">>>>> float");

                        double decodedVal;
                        cbor_value_get_double(&value, &decodedVal);

                        AssetValue_t assetValue;
                        assetValue.floatValue = decodedVal;

                        le_result_t result = SetVal(path, assetValue,
                                                    LE_AVDATA_DATA_TYPE_FLOAT, false);

                        // TODO: call lwm2mcore_SendAsyncResponse based on "result"
                        //   same as string

                        response.payload = NULL;
                        response.payloadLength = 0;

                        switch (result)
                        {
                            case LE_OK:
                                response.code = COAP_RESOURCE_CHANGED;
                                break;
                            case LE_NOT_PERMITTED:
                                response.code = COAP_METHOD_UNAUTHORIZED;
                                break;
                            case LE_NOT_FOUND:
                                response.code = COAP_RESOURCE_NOT_FOUND;
                                break;
                        }

                        lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                        break;
                    }
                    default:
                        LE_ERROR("Server attempts to write a single data point, but payload \
                                 contains unexpected CBOR type: %d", type);

                        // TODO: call lwm2mcore_SendAsyncResponse with error
                        // COAP_BAD_REQUEST

                        response.code = COAP_BAD_REQUEST;
                        response.payload = NULL;
                        response.payloadLength = 0;

                        lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                }
            }

            break;
        }
        case COAP_POST: // server executes a command on device
        {
            LE_INFO(">>>>> COAP_POST - Server executes a command on device");

            AssetData_t* assetDataPtr = GetAssetData(path);

            if (assetDataPtr != NULL)
            {
                // Server attempts to execute a path that's not executable.
                if ((assetDataPtr->access & LE_AVDATA_ACCESS_EXEC) != LE_AVDATA_ACCESS_EXEC)
                {
                    LE_ERROR("Server attempts to execute on an asset data without execute permission.");

                    // TODO: call  lwm2mcore_SendAsyncResponse  with error
                    // 1. execute, no permission [4.01 Unauthorized]
                    // COAP_METHOD_UNAUTHORIZED

                    response.code = COAP_METHOD_UNAUTHORIZED;
                    response.payload = NULL;
                    response.payloadLength = 0;

                    lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                    return;
                }


                CborParser parser;
                CborValue value, recursed;

                cbor_parser_init(payload, payloadLen, 0, &parser, &value);

                // Decode data in payload, and construct the argument list.
                if (cbor_value_is_map(&value))
                {
                    cbor_value_enter_container(&value, &recursed);

                    bool labelProcessed = false;
                    Argument_t* argPtr = NULL;

                    while (!cbor_value_at_end(&recursed)) {

                        CborType type = cbor_value_get_type(&recursed);

                        if (type != CborTextStringType)
                        {
                            labelProcessed = false;
                        }

                        switch (type)
                        {
                            case CborTextStringType:
                            {
                                char *buf;
                                size_t n;
                                cbor_value_dup_text_string(&recursed, &buf, &n, &recursed);

                                if (!labelProcessed)
                                {
                                    // "buf" is argument name.

                                    // If the argument name doesn't exist in the list, create one.
                                    // Otherwise, save the node ref.
                                    le_dls_Link_t* argLinkPtr =
                                        le_dls_Peek(&assetDataPtr->arguments);

                                    while (argLinkPtr != NULL)
                                    {
                                        argPtr = CONTAINER_OF(argLinkPtr, Argument_t,
                                                                          link);

                                        if (strcmp(argPtr->argumentName, buf) == 0)
                                        {
                                            break;
                                        }
                                        else
                                        {
                                            argPtr = NULL;
                                        }

                                        argLinkPtr = le_dls_PeekNext(&assetDataPtr->arguments,
                                                                     argLinkPtr);
                                    }

                                    if (argPtr == NULL)
                                    {
                                        Argument_t* argumentPtr = le_mem_ForceAlloc(ArgumentPool);
                                        argumentPtr->link = LE_DLS_LINK_INIT;

                                        argumentPtr->argumentName = le_mem_ForceAlloc(StringPool);
                                        strncpy(argumentPtr->argumentName, buf,
                                                LE_AVDATA_STRING_VALUE_LEN);

                                        le_dls_Queue(&assetDataPtr->arguments,
                                                     &(argumentPtr->link));

                                        argPtr = argumentPtr;
                                    }

                                    labelProcessed = true;
                                }
                                else
                                {
                                    // "buf" is argument value.
                                    argPtr->argValType = LE_AVDATA_DATA_TYPE_STRING;
                                    argPtr->argValue.strValuePtr = le_mem_ForceAlloc(StringPool);
                                    strncpy(argPtr->argValue.strValuePtr, buf,
                                            LE_AVDATA_STRING_VALUE_LEN);
                                    argPtr = NULL;

                                    labelProcessed = false;
                                }

                                free(buf);

                                continue;
                            }

                            case CborIntegerType:
                            {
                                int val;
                                cbor_value_get_int(&recursed, &val);     // can't fail

                                argPtr->argValType = LE_AVDATA_DATA_TYPE_INT;
                                argPtr->argValue.intValue = val;
                                argPtr = NULL;

                                break;
                            }

                            case CborBooleanType:
                            {
                                bool val;
                                cbor_value_get_boolean(&recursed, &val);       // can't fail

                                argPtr->argValType = LE_AVDATA_DATA_TYPE_BOOL;
                                argPtr->argValue.boolValue = val;
                                argPtr = NULL;

                                break;
                            }

                            case CborDoubleType:
                            {
                                double val;
                                cbor_value_get_double(&recursed, &val);

                                argPtr->argValType = LE_AVDATA_DATA_TYPE_FLOAT;
                                argPtr->argValue.floatValue = val;
                                argPtr = NULL;

                                break;
                            }

                            default:
                                LE_ERROR("Server attempts to execute a command, but payload \
                                         contains unexpected CBOR type: %d", type);

                                // TODO: call lwm2mcore_SendAsyncResponse with error
                                // COAP_BAD_REQUEST

                                response.code = COAP_BAD_REQUEST;
                                response.payload = NULL;
                                response.payloadLength = 0;

                                lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                        }

                        cbor_value_advance_fixed(&recursed);
                    }

                    cbor_value_leave_container(&value, &recursed);

                    // TODO: DEBUG
                    DumpArgList(&assetDataPtr->arguments);

                    // Create a safe ref with the argument list, and pass that to the handler.
                    le_avdata_ArgumentListRef_t argListRef
                         = le_ref_CreateRef(ArgListRefMap, &assetDataPtr->arguments);

                    // Execute the command with the argument list collected earlier.
                    le_result_t commandResult = LE_OK;
                    assetDataPtr->handlerPtr(path, LE_AVDATA_ACCESS_EXEC, argListRef,
                                             &commandResult, assetDataPtr->contextPtr);

                    // We can delete the ref right away since this is the only place the argument
                    // list is used.
                    le_ref_DeleteRef(ArgListRefMap, argListRef);

                    // TODO: call  lwm2mcore_SendAsyncResponse  according to commandResult

                    // 3. execute, command success  [2.04 Changed]
                    // COAP_RESOURCE_CHANGED

                    // 4. execute, command failure [?????]
                    // COAP_BAD_REQUEST or COAP_INTERNAL_ERROR
                    // put error msg in payload

                    response.payload = NULL;  //TODO: put error msg in here in case of exe fail
                    response.payloadLength = 0;

                    response.code = (commandResult == LE_OK) ?
                                    COAP_RESOURCE_CHANGED : COAP_INTERNAL_ERROR;

                    lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                }
                else
                {
                    LE_ERROR("Server attempts to execute a command but argument list is invalid");
                    // TODO: call  lwm2mcore_SendAsyncResponse  with error
                    // 5. execute, invalid argument list  [4.00 Bad Request]
                    // COAP_BAD_REQUEST

                    response.code = COAP_BAD_REQUEST;
                    response.payload = NULL;
                    response.payloadLength = 0;

                    lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

                    return;
                }
            }
            else
            {
                LE_ERROR("Server attempts to execute a command but the asset data doesn't exist");
                // TODO: call  lwm2mcore_SendAsyncResponse  with "not found"
                // 2. execute, asset data not found  [4.04 Not Found]
                // COAP_RESOURCE_NOT_FOUND

                response.code = COAP_RESOURCE_NOT_FOUND;
                response.payload = NULL;
                response.payloadLength = 0;

                lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

            }

            break;
        }
        default:
            LE_WARN("undefined action from an AirVantage server.");
            // TODO: call  lwm2mcore_SendAsyncResponse  with error
            // COAP_BAD_REQUEST

            response.code = COAP_BAD_REQUEST;
            response.payload = NULL;
            response.payloadLength = 0;

            lwm2mcore_SendAsyncResponse(sessionContext, serverReqRef, &response);

            break;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/* Public functions                                                                               */
////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------
/**
 * Registeres a handler function to a asset data path when a resource event (read/write/execute)
 * occurs.
 *
 * @return:
 *      - resource event handler ref - needed to remove the handler
 *      - NULL - invalid asset data path is provided
 */
//--------------------------------------------------------------------------------------------------
le_avdata_ResourceEventHandlerRef_t le_avdata_AddResourceEventHandler
(
    const char* path,                           ///< [IN] Asset data path
    le_avdata_ResourceHandlerFunc_t handlerPtr, ///< [IN] Handler function for resource event
    void* contextPtr                            ///< [IN] context pointer
)
{
    AssetData_t* assetDataPtr = NULL;

    le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(AssetDataMap);

    while (le_hashmap_NextNode(iter) == LE_OK)
    {
        if (strcmp(path, le_hashmap_GetKey(iter)) == 0)
        {
            assetDataPtr = le_hashmap_GetValue(iter);
            assetDataPtr->handlerPtr = handlerPtr;
            assetDataPtr->contextPtr = contextPtr;

            return le_ref_CreateRef(ResourceEventHandlerMap, assetDataPtr);
        }
    }

    LE_WARN("Non-existing asset data path %s", path);
    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Removes a resource event handler function to a asset data path.
 */
//--------------------------------------------------------------------------------------------------
void le_avdata_RemoveResourceEventHandler
(
    le_avdata_ResourceEventHandlerRef_t addHandlerRef ///< [IN] resource event handler ref
)
{
    AssetData_t* assetDataPtr = le_ref_Lookup(ResourceEventHandlerMap, addHandlerRef);

    if (assetDataPtr != NULL)
    {
        le_ref_DeleteRef(ResourceEventHandlerMap, addHandlerRef);
        assetDataPtr->handlerPtr = NULL;
        assetDataPtr->contextPtr = NULL;
    }
}


// TODO:
//   1. duplicated entry handling
//   2. RemoveResource
//   3. Function to make an asset data with NULL value (i.e. the initial state of data type of none)
//--------------------------------------------------------------------------------------------------
/**
 * Create an asset data with the provided path. Note that asset data type and value are determined
 * upton the first call to a Set function. When an asset data is created, it contains a NULL value,
 * represented by the data type of none.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_CreateResource
(
    const char* path,                 ///< [IN] Asset data path
    le_avdata_AccessMode_t accessMode ///< [IN] Asset data access mode
)
{
    char* assetPathPtr = le_mem_ForceAlloc(AssetPathPool);
    AssetData_t* assetDataPtr = le_mem_ForceAlloc(AssetDataPool);

    // Check if the asset data path is legal.
    LE_ASSERT(IsAssetDataPathValid(path) == true);
    LE_ASSERT(le_utf8_Copy(assetPathPtr, path, LE_AVDATA_PATH_NAME_LEN, NULL) == LE_OK);

    // Initialize the asset data.
    // Note that the union field is zeroed out by the memset.
    memset(assetDataPtr, 0, sizeof(AssetData_t));
    LE_ASSERT(ConvertAccessModeToMask(accessMode, &(assetDataPtr->access)) == LE_OK);
    assetDataPtr->dataType = LE_AVDATA_DATA_TYPE_NONE;
    assetDataPtr->handlerPtr = NULL;
    assetDataPtr->contextPtr = NULL;
    assetDataPtr->arguments = LE_DLS_LIST_INIT;

    le_hashmap_Put(AssetDataMap, assetPathPtr, assetDataPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the value of an integer asset data.
 *
 * @return:
 *      - LE_BAD_PARAMETER - asset data being accessed is of the wrong data type
 *      - others per GetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetInt
(
    const char* path, ///< [IN] Asset data path
    int32_t* valuePtr ///< [OUT] Retrieved integer
)
{
    AssetValue_t assetValue;
    le_avdata_DataType_t type;

    le_result_t result = GetVal(path, &assetValue, &type, true);

    if (type != LE_AVDATA_DATA_TYPE_INT)
    {
        LE_ERROR("Accessing asset (%s) of type %s as int.", path, GetDataTypeStr(type));
        return LE_BAD_PARAMETER;
    }

    if (result != LE_OK)
    {
        return result;
    }
    else
    {
        *valuePtr = assetValue.intValue;
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Sets the value of an integer asset data.
 *
 * @return:
 *      - per SetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_SetInt
(
    const char* path, ///< [IN] Asset data path
    int32_t value     ///< [IN] integer to be set
)
{
    AssetValue_t assetValue;
    assetValue.intValue = value;

    return SetVal(path, assetValue, LE_AVDATA_DATA_TYPE_INT, true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the value of a float asset data.
 *
 * @return:
 *      - LE_BAD_PARAMETER - asset data being accessed is of the wrong data type
 *      - others per GetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetFloat
(
    const char* path, ///< [IN] Asset data path
    double* valuePtr  ///< [OUT] Retrieved float
)
{
    AssetValue_t assetValue;
    le_avdata_DataType_t type;

    le_result_t result = GetVal(path, &assetValue, &type, true);

    if (type != LE_AVDATA_DATA_TYPE_FLOAT)
    {
        LE_ERROR("Accessing asset (%s) of type %s as float.", path, GetDataTypeStr(type));
        return LE_BAD_PARAMETER;
    }

    if (result != LE_OK)
    {
        return result;
    }
    else
    {
        *valuePtr = assetValue.floatValue;
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Sets the value of a float asset data.
 *
 * @return:
 *      - per SetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_SetFloat
(
    const char* path,  ///< [IN] Asset data path
    double value       ///< [IN] float to be set
)
{
    AssetValue_t assetValue;
    assetValue.floatValue = value;

    return SetVal(path, assetValue, LE_AVDATA_DATA_TYPE_FLOAT, true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the value of a bool asset data.
 *
 * @return:
 *      - LE_BAD_PARAMETER - asset data being accessed is of the wrong data type
 *      - others per GetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetBool
(
    const char* path, ///< [IN] Asset data path
    bool* valuePtr    ///< [OUT] Retrieved bool
)
{
    AssetValue_t assetValue;
    le_avdata_DataType_t type;

    le_result_t result = GetVal(path, &assetValue, &type, true);

    if (type != LE_AVDATA_DATA_TYPE_BOOL)
    {
        LE_ERROR("Accessing asset (%s) of type %s as bool.", path, GetDataTypeStr(type));
        return LE_BAD_PARAMETER;
    }

    if (result != LE_OK)
    {
        return result;
    }
    else
    {
        *valuePtr = assetValue.boolValue;
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Sets the value of a bool asset data.
 *
 * @return:
 *      - per SetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_SetBool
(
    const char* path, ///< [IN] Asset data path
    bool value        ///< [IN] bool to be set
)
{
    AssetValue_t assetValue;
    assetValue.boolValue = value;

    return SetVal(path, assetValue, LE_AVDATA_DATA_TYPE_BOOL, true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the value of a string asset data.
 *
 * @return:
 *      - LE_BAD_PARAMETER - asset data being accessed is of the wrong data type
 *      - others per GetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetString
(
    const char* path,       ///< [IN] Asset data path
    char* value,            ///< [OUT] Retrieved string
    size_t valueNumElements ///< [IN] String buffer size
)
{
    AssetValue_t assetValue;
    le_avdata_DataType_t type;

    le_result_t result = GetVal(path, &assetValue, &type, true);

    if (type != LE_AVDATA_DATA_TYPE_STRING)
    {
        LE_ERROR("Accessing asset (%s) of type %s as string.", path, GetDataTypeStr(type));
        return LE_BAD_PARAMETER;
    }

    if (result != LE_OK)
    {
        return result;
    }
    else
    {
        strncpy(value, assetValue.strValuePtr, valueNumElements);
        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Sets the value of a string asset data.
 *
 * @return:
 *      - per SetVal
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_SetString
(
    const char* path, ///< [IN] Asset data path
    const char* value ///< [IN] string to be set
)
{
    AssetValue_t assetValue;
    assetValue.strValuePtr = le_mem_ForceAlloc(StringPool);
    strncpy(assetValue.strValuePtr, value, LE_AVDATA_STRING_VALUE_LEN);

    return SetVal(path, assetValue, LE_AVDATA_DATA_TYPE_STRING, true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the bool argument with the specified name.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if argument doesn't exist, or its data type doesn't match the API.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetBoolArg
(
    le_avdata_ArgumentListRef_t argumentListRef, ///< [IN] Argument list
    const char* argName,                         ///< [IN] Argument name as key
    bool* boolArgPtr                             ///< [OUT] Retrieved bool arg
)
{
    Argument_t* argPtr = GetArg(argumentListRef, argName);

    if (argPtr != NULL)
    {
        if (argPtr->argValType == LE_AVDATA_DATA_TYPE_BOOL)
        {
            *boolArgPtr = argPtr->argValue.boolValue;
            return LE_OK;
        }
        else
        {
            LE_ERROR("Found argument named %s, but type is %s instead of %s", argName,
                     GetDataTypeStr(argPtr->argValType), GetDataTypeStr(LE_AVDATA_DATA_TYPE_BOOL));
            return LE_NOT_FOUND;
        }
    }
    else
    {
        LE_ERROR("Cannot find argument named %s", argName);
        return LE_NOT_FOUND;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the float argument with the specified name.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if argument doesn't exist, or its data type doesn't match the API.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetFloatArg
(
    le_avdata_ArgumentListRef_t argumentListRef, ///< [IN] Argument list
    const char* argName,                         ///< [IN] Argument name as key
    double* floatArgPtr                          ///< [OUT] Retrieved float arg
)
{
    Argument_t* argPtr = GetArg(argumentListRef, argName);

    if (argPtr != NULL)
    {
        if (argPtr->argValType == LE_AVDATA_DATA_TYPE_FLOAT)
        {
            *floatArgPtr = argPtr->argValue.floatValue;
            return LE_OK;
        }
        else
        {
            LE_ERROR("Found argument named %s, but type is %s instead of %s", argName,
                     GetDataTypeStr(argPtr->argValType), GetDataTypeStr(LE_AVDATA_DATA_TYPE_FLOAT));
            return LE_NOT_FOUND;
        }
    }
    else

    {
        LE_ERROR("Cannot find argument named %s", argName);
        return LE_NOT_FOUND;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the int argument with the specified name.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if argument doesn't exist, or its data type doesn't match the API.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetIntArg
(
    le_avdata_ArgumentListRef_t argumentListRef, ///< [IN] Argument list
    const char* argName,                         ///< [IN] Argument name as key
    int32_t* intArgPtr                           ///< [OUT] Retrieved int arg
)
{
    Argument_t* argPtr = GetArg(argumentListRef, argName);

    if (argPtr != NULL)
    {
        if (argPtr->argValType == LE_AVDATA_DATA_TYPE_INT)
        {
            *intArgPtr = argPtr->argValue.intValue;
            return LE_OK;
        }
        else
        {
            LE_ERROR("Found argument named %s, but type is %s instead of %s", argName,
                     GetDataTypeStr(argPtr->argValType), GetDataTypeStr(LE_AVDATA_DATA_TYPE_INT));
            return LE_NOT_FOUND;
        }
    }
    else
    {
        LE_ERROR("Cannot find argument named %s", argName);
        return LE_NOT_FOUND;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the string argument with the specified name.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if argument doesn't exist, or its data type doesn't match the API.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetStringArg
(
    le_avdata_ArgumentListRef_t argumentListRef, ///< [IN] Argument list
    const char* argName,                         ///< [IN] Argument name as key
    char* strArg,                                ///< [OUT] Retrieved string arg
    size_t argNumElements                        ///< [IN] string arg buffer length
)
{
    Argument_t* argPtr = GetArg(argumentListRef, argName);

    if (argPtr != NULL)
    {
        if (argPtr->argValType == LE_AVDATA_DATA_TYPE_STRING)
        {
            strncpy(strArg, argPtr->argValue.strValuePtr, argNumElements);
            return LE_OK;
        }
        else
        {
            LE_ERROR("Found argument named %s, but type is %s instead of %s", argName,
                     GetDataTypeStr(argPtr->argValType),
                     GetDataTypeStr(LE_AVDATA_DATA_TYPE_STRING));
            return LE_NOT_FOUND;
        }
    }
    else
    {
        LE_ERROR("Cannot find argument named %s", argName);
        return LE_NOT_FOUND;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the length of the string argument of the specified name.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if argument doesn't exist, or its data type doesn't match the API.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_GetStringArgLength
(
    le_avdata_ArgumentListRef_t argumentListRef, ///< [IN] asdfsadf
    const char* argName,                         ///< [IN] asdfsadf
    int32_t* strArgLenPtr                        ///< [OUT] asdfsadf
)
{
    Argument_t* argPtr = GetArg(argumentListRef, argName);

    if (argPtr != NULL)
    {
        if (argPtr->argValType == LE_AVDATA_DATA_TYPE_STRING)
        {
            *strArgLenPtr = strlen(argPtr->argValue.strValuePtr);
            return LE_OK;
        }
        else
        {
            LE_ERROR("Found argument named %s, but type is %s instead of %s", argName,
                     GetDataTypeStr(argPtr->argValType),
                     GetDataTypeStr(LE_AVDATA_DATA_TYPE_STRING));
            return LE_NOT_FOUND;
        }
    }
    else
    {
        LE_ERROR("Cannot find argument named %s", argName);
        return LE_NOT_FOUND;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the real record ref from the safe ref
 */
//--------------------------------------------------------------------------------------------------
le_avdata_RecordRef_t GetRecRefFromSafeRef
(
    void* safeRef,
    const char* funcNamePtr
)
{
    RecordRefData_t* recRefDataPtr = le_ref_Lookup(RecordRefMap, safeRef);

    if (recRefDataPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference %p from %s", safeRef, funcNamePtr);
        return NULL;
    }

    return recRefDataPtr->recRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Create a timeseries record
 *
 * @return Reference to the record
 */
//--------------------------------------------------------------------------------------------------
le_avdata_RecordRef_t le_avdata_CreateRecord
(
    void
)
{
    LE_DEBUG("Creating record");
    timeSeries_RecordRef_t recRef;

    LE_ASSERT(timeSeries_Create(&recRef) == LE_OK);
    LE_ASSERT(recRef != NULL);

    // Return a safe reference for the record
    RecordRefData_t* recRefDataPtr = le_mem_ForceAlloc(RecordRefDataPoolRef);

    recRefDataPtr->clientSessionRef = le_avdata_GetClientSessionRef();
    recRefDataPtr->recRef = recRef;

    return le_ref_CreateRef(RecordRefMap, recRefDataPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a timeseries record
 */
//--------------------------------------------------------------------------------------------------
void le_avdata_DeleteRecord
(
    le_avdata_RecordRef_t recordRef
        ///< [IN]
)
{
    // Map safeRef to desired data
    recordRef = GetRecRefFromSafeRef(recordRef, __func__);

    // delete record data
    timeSeries_Delete(recordRef);

    le_ref_IterRef_t iterRef = le_ref_GetIterator(RecordRefMap);
    RecordRefData_t* recRefDataPtr;

    // remove safe ref
    while ( le_ref_NextNode(iterRef) == LE_OK )
    {
        recRefDataPtr = le_ref_GetValue(iterRef);

        if ( recRefDataPtr->recRef == recordRef )
        {
            le_mem_Release((void*)recRefDataPtr);
            le_ref_DeleteRef(RecordRefMap, (void*)le_ref_GetSafeRef(iterRef));
            break;
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Accumulate int data
 *
 * @note The client will be terminated if the recordRef is not valid, or the resource doesn't exist
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if record is full
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_RecordInt
(
    le_avdata_RecordRef_t recordRef,
        ///< [IN]

    const char* path,
        ///< [IN]

    int32_t value,
        ///< [IN]

    uint64_t timestamp
        ///< [IN]
)
{
    le_result_t result;

    // Map safeRef to desired data
    recordRef = GetRecRefFromSafeRef(recordRef, __func__);

    result = timeSeries_AddInt(recordRef, path, value, timestamp);

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Accumulate float data
 *
 * @note The client will be terminated if the recordRef is not valid, or the resource doesn't exist
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if record is full
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_RecordFloat
(
    le_avdata_RecordRef_t recordRef,
        ///< [IN]

    const char* path,
        ///< [IN]

    double value,
        ///< [IN]

    uint64_t timestamp
        ///< [IN]
)
{
    le_result_t result;

    // Map safeRef to desired data
    recordRef = GetRecRefFromSafeRef(recordRef, __func__);

    result = timeSeries_AddFloat(recordRef, path, value, timestamp);

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Accumulate boolean data
 *
 * @note The client will be terminated if the recordRef is not valid, or the resource doesn't exist
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if record is full
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_RecordBool
(
    le_avdata_RecordRef_t recordRef,
        ///< [IN]

    const char* path,
        ///< [IN]

    bool value,
        ///< [IN]

    uint64_t timestamp
        ///< [IN]
)
{
    le_result_t result;

    // Map safeRef to desired data
    recordRef = GetRecRefFromSafeRef(recordRef, __func__);

    result = timeSeries_AddBool(recordRef, path, value, timestamp);

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Accumulate string data
 *
 * @note The client will be terminated if the recordRef is not valid, or the resource doesn't exist
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if record is full
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_RecordString
(
    le_avdata_RecordRef_t recordRef,
        ///< [IN]

    const char* path,
        ///< [IN]

    const char* value,
        ///< [IN]

    uint64_t timestamp
        ///< [IN]
)
{
    le_result_t result;

    // Map safeRef to desired data
    recordRef = GetRecRefFromSafeRef(recordRef, __func__);

    result = timeSeries_AddString(recordRef, path, value, timestamp);

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Push record to the server
 *
* @return:
 *      - LE_OK on success.
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avdata_PushRecord
(
    le_avdata_RecordRef_t recordRef,
        ///< [IN]

    le_avdata_CallbackResultFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    le_result_t result;
    recordRef = GetRecRefFromSafeRef(recordRef, __func__);
    result = timeSeries_PushRecord(recordRef, handlerPtr, contextPtr);
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the avData module
 */
//--------------------------------------------------------------------------------------------------
void avData_Init
(
    void
)
{
    // Create various memory pools
    AssetPathPool = le_mem_CreatePool("Asset path pool", LE_AVDATA_PATH_NAME_LEN);
    AssetDataPool = le_mem_CreatePool("Asset data pool", sizeof(AssetData_t));
    StringPool = le_mem_CreatePool("String pool", LE_AVDATA_STRING_VALUE_LEN);
    ArgumentPool = le_mem_CreatePool("Argument pool", sizeof(Argument_t));
    RecordRefDataPoolRef = le_mem_CreatePool("Record ref data pool", sizeof(RecordRefData_t));

    // Create the hasmap to store asset data
    AssetDataMap = le_hashmap_Create("Asset Data Map", MAX_EXPECTED_ASSETDATA,
                                     le_hashmap_HashString, le_hashmap_EqualsString);

    // The argument list is used once at the command handler execution, so the map is really holding
    // one object at a time. Therefore the map size isn't expected to be big - techinically 1 is
    // enough.
    ArgListRefMap = le_ref_CreateMap("Argument List Ref Map", 5);

    // Create map to store the resource event handler.
    ResourceEventHandlerMap = le_ref_CreateMap("Resource Event Handler Map", MAX_EXPECTED_ASSETDATA);

    RecordRefMap = le_ref_CreateMap("RecRefMap", 300);

    // Set the AV server request handler
    lwm2mcore_SetCoapEventHandler(AvServerRequestHandler);

    // Add a handler for client session closes
    le_msg_AddServiceCloseHandler( le_avdata_GetServiceRef(), ClientCloseSessionHandler, NULL );
}
