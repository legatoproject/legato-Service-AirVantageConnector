//--------------------------------------------------------------------------------------------------
/**
 * Simple test app that records timeseries data and pushes the data to the server.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"


// push ack callback
static void PushCallbackHandler
(
    le_avdata_PushStatus_t status,
    void* contextPtr
)
{
    switch (status)
    {
        case LE_AVDATA_PUSH_SUCCESS:
            LE_INFO("LE_AVDATA_PUSH_SUCCESS");
            break;
        case LE_AVDATA_PUSH_FAILED:
            LE_INFO("LE_AVDATA_PUSH_FAILED");
            break;
    }
}


// Record a value of different type on a resource already set as another
void RecordInvalidValue()
{
    LE_INFO("Running record invalid value");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    uint64_t timestamp = 1412320402000;

    // Start record integer value on resource "intValue", try to record value of different type on "intValue"
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 6161, timestamp) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "intValue", 0.08, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordBool(recRef, "intValue", false, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordString(recRef, "intValue", "Hello World", timestamp) == LE_FAULT);

    // Start record float value on resource "floatValue", try to record value of different type on "floatValue"
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.08, timestamp) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "floatValue", 1234, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordBool(recRef, "floatValue", true, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordString(recRef, "floatValue", "Hello World", timestamp) == LE_FAULT);

    // Start record boolean value on resource "boolValue", try to record value of different type on "boolValue"
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, timestamp) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "boolValue", 1234, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "boolValue", 0.08, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordString(recRef, "boolValue", "Hello World", timestamp) == LE_FAULT);

    // Start record float value on resource "strValue", try to record value of different type on "strValue"
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "Hello World", timestamp) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "strValue", 897349, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "strValue", 0.08, timestamp) == LE_FAULT);
    LE_ASSERT(le_avdata_RecordBool(recRef, "strValue", false, timestamp) == LE_FAULT);

    LE_INFO("Pass");
}


// Pushing a single integer resource to the server
void PushInt_01()
{
    LE_INFO("Running single integer push");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 6161, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple integer values accumulated over ONE resource
void PushInt_02()
{
    LE_INFO("Running multiple integer push over ONE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 14, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 17, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 22, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 33, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 50, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 53, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 70, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 99, 1412320409000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple integer values accumulated over MULTIPLE resource
/* Constructing the following:

  | "ts"              | "intValue"              | "intValue2"
  | 1412320402000     | 14                      | 10000
  | 1412320403000     | 17                      | 10001
  | 1412320404000     | 22                      | 10011
  | 1412320405000     | 33                      | 10111
  | 1412320406000     | 50                      | 11111
  | 1412320407000     | 53                      |     1
  | 1412320408000     | 70                      |    11
  | 1412320409000     | 99                      |   111

  e.g. How to interpret data. intValue at ts 142320402000 is 14
*/
void PushInt_03()
{
    LE_INFO("Running multiple integer push over MULTIPLE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 14, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 17, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 22, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 33, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 50, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 53, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 70, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 99, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 10000, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 10001, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 10011, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 10111, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 11111, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 1, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 11, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 111, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);
    LE_INFO("Pass");
}


// Pushing multiple integer values accumulated over MULTIPLE resource (default)
void PushInt_04()
{
    LE_INFO("Running multiple integer push over MULTIPLE resource (default)");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    // int1,2,3,6,7 only have one value
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 0, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue2", 1, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue3", 1, 1412320404000) == LE_OK);
    // int4 & 5 multiple values
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue4", 2, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue4", 4, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue4", 8, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue5", 3, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue5", 9, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue6", 5, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue7", 8, 1412320408000) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);
    LE_INFO("Pass");
}


// Pushing multiple integer values accumulated over ONE resource until the buffer is overflow and send
void PushInt_05()
{
    LE_INFO("Running multiple integer push over ONE resource (overflow)");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    le_result_t result = LE_OK;
    int value = 0;
    uint64_t timestamp = 1412320402000;

    while (result != LE_NO_MEMORY)
    {
        LE_INFO("Sampling  value: %d timestamp: %" PRIu64, value, timestamp);
        result = le_avdata_RecordInt(recRef, "intOverflow", value, timestamp);
        value++;
        timestamp += 100;
    }

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);
    LE_INFO("Pass");
}


// Pushing a float integer resource to the server
void PushFloat_01()
{
    LE_INFO("Running single float push");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.08, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple float values accumulated over ONE resource
void PushFloat_02()
{
    LE_INFO("Running multiple float push over ONE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.8292100722, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.4292728335, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.0165476592, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.7936539892, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.6718297351, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.2347403661, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.0987814032, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.9667730980, 1412320409000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple float values accumulated over MULTIPLE resource
void PushFloat_03()
{
    LE_INFO("Running multiple float push over MULTIPLE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.8292100722, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.4292728335, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.0165476592, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.7936539892, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.6718297351, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.2347403661, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.0987814032, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.9667730980, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.7555294798, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.6172080662, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.5672352094, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.9774335244, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.2496382523, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.0926582738, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.1159668317, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue2", 0.6971518122, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


double rand_float(double low, double high)
{
    return ((double)rand() * (high - low)) / (double)RAND_MAX + low;
}


// Pushing multiple float values accumulated over ONE resource until the buffer is overflow and send
void PushFloat_05()
{
    LE_INFO("Running multiple float push over ONE resource (overflow)");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    le_result_t result = LE_OK;
    uint64_t timestamp = 1412320402000;

    while (result != LE_NO_MEMORY)
    {
        double value = rand_float(0,1);
        LE_INFO("Sampling  value: %f timestamp: %" PRIu64, value, timestamp);
        result = le_avdata_RecordFloat(recRef, "floatOverflow", value, timestamp);
        timestamp += 100;
    }

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);
    LE_INFO("Pass");
}


// Pushing a boolean integer resource to the server
void PushBoolean_01()
{
    LE_INFO("Running single boolean push");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple boolean values accumulated over ONE resource
void PushBoolean_02()
{
    LE_INFO("Running multiple boolean push over ONE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320409000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple boolean values accumulated over MULTIPLE resource
void PushBoolean_03()
{
    LE_INFO("Running multiple boolean push over ONE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", true, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", false, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", false, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", true, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", true, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", false, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", false, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue2", false, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}



// Pushing multiple boolean values accumulated over ONE resource until the buffer is overflow and send
void PushBoolean_05()
{
    LE_INFO("Running multiple boolean push over ONE resource (overflow)");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    le_result_t result = LE_OK;
    uint64_t timestamp = 1412320402000;

    while (result != LE_NO_MEMORY)
    {
        bool value = rand() % 2;
        LE_INFO("Sampling  value: %d timestamp: %" PRIu64, value, timestamp);
        result = le_avdata_RecordBool(recRef, "boolOverflow", value, timestamp);
        timestamp += 100;
    }

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);
    LE_INFO("Pass");
}


// Pushing a float integer resource to the server
void PushString_01()
{
    LE_INFO("Running single string push");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "Hello World", 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple string values accumulated over ONE resource
void PushString_02()
{
    LE_INFO("Running multiple string push over ONE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "hello", 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "there", 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "thank", 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "you", 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "for", 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "reading", 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "this", 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "LOL", 1412320409000) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple string values accumulated over MULTIPLE resource
void PushString_03()
{
    LE_INFO("Running multiple string push over ONE resource");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "hello", 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "there", 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "thank", 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "you", 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "for", 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "reading", 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "this", 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "LOL", 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "a", 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "b", 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "c", 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "d", 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "e", 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "f", 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "g", 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue2", "f", 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Generate random string of length len
void gen_random(char *s, const int len) {
    static const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789~!@#$%^&*()_";

    int i;
    for (i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}


// Pushing multiple string values accumulated over ONE resource until the buffer is overflow and send
void PushString_05()
{
    LE_INFO("Running multiple string push over ONE resource (overflow)");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    le_result_t result = LE_OK;
    uint64_t timestamp = 1412320402000;

    while (result != LE_NO_MEMORY)
    {
        int randSize = rand() % 10; // this number can be adjusted to fit more/less data
        char buff[255];
        gen_random(buff, randSize);
        LE_INFO("Sampling  value: %s timestamp: %" PRIu64, buff, timestamp);
        result = le_avdata_RecordString(recRef, "strOverflow", buff, timestamp);
        timestamp += 100;
    }

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);
    LE_INFO("Pass");
}


// Pushing values with unordered timestamps
// Data should be represented on the server in order
void UnorderedTimestamp()
{
    LE_INFO("Running unordered timestamp");

    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 50, 1412320406000) == LE_OK); // [6000]
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 14, 1412320402000) == LE_OK); // [2000,6000]
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 22, 1412320404000) == LE_OK); // [2000,4000,6000]
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 53, 1412320407000) == LE_OK); // [2000,4000,6000,7000]
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 17, 1412320403000) == LE_OK); // [2000,3000,4000,6000,7000]
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 70, 1412320408000) == LE_OK); // [2000,3000,4000,6000,7000,8000]
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 33, 1412320405000) == LE_OK); // [2000,3000,4000,5000,6000,7000,8000]
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 99, 1412320409000) == LE_OK); // [2000,3000,4000,5000,6000,7000,8000,9000]
                                                                                    // => [14,17,22,33,50,53,70,99]
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Creating multiple records and push them to the server
void PushMultipleRecords()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 14, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 17, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 22, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 33, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 50, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 53, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 70, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 99, 1412320409000) == LE_OK);

    le_avdata_RecordRef_t recRef2 = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.8292100722, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.4292728335, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.0165476592, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.7936539892, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.6718297351, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.2347403661, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.0987814032, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef2, "floatValue", 0.9667730980, 1412320409000) == LE_OK);

    le_avdata_RecordRef_t recRef3 = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", true, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", false, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", true, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", false, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", true, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", false, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", true, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef3, "boolValue", false, 1412320409000) == LE_OK);

    le_avdata_RecordRef_t recRef4 = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "hello", 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "there", 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "thank", 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "you", 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "for", 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "reading", 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "this", 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef4, "strValue", "LOL", 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef2, PushCallbackHandler, NULL) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef3, PushCallbackHandler, NULL) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef4, PushCallbackHandler, NULL) == LE_OK);

    le_avdata_DeleteRecord(recRef);
    le_avdata_DeleteRecord(recRef2);
    le_avdata_DeleteRecord(recRef3);
    le_avdata_DeleteRecord(recRef4);

    LE_INFO("Pass");
}


// Pushing multiple values of different type over multiple resources
void PushMix_01()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "x", 0, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "y", 2, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "z", 0, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "lat", 49.455177, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "long", 0.537743, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "nbat", 6, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "speed", 0.08, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "y", 3, 1412320402100) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple values of different type over multiple resources (more)
void PushMix_02()
{
    // Similar to test case PushMultipleRecords but combining into a single record
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 14, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 17, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 22, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 33, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 50, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 53, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 70, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "intValue", 99, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.8292100722, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.4292728335, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.0165476592, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.7936539892, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.6718297351, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.2347403661, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.0987814032, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "floatValue", 0.9667730980, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", true, 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "boolValue", false, 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "hello", 1412320402000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "there", 1412320403000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "thank", 1412320404000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "you", 1412320405000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "for", 1412320406000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "reading", 1412320407000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "this", 1412320408000) == LE_OK);
    LE_ASSERT(le_avdata_RecordString(recRef, "strValue", "LOL", 1412320409000) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);

    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


// Pushing multiple values of different type over multiple resources with current timestamp
void PushMix_03()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();
    struct timeval tv;
    uint64_t utcMilliSec;

    // get current time
    gettimeofday(&tv, NULL);
    utcMilliSec = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
    LE_ASSERT(le_avdata_RecordInt(recRef, "x", 0, utcMilliSec) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "y", 2, utcMilliSec) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "z", 0, utcMilliSec) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "lat", 49.455177, utcMilliSec) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "long", 0.537743, utcMilliSec) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "nbat", 6, utcMilliSec) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "speed", 0.08, utcMilliSec) == LE_OK);

    // change timestamp
    gettimeofday(&tv, NULL);
    utcMilliSec = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
    LE_ASSERT(le_avdata_RecordInt(recRef, "y", 3, utcMilliSec) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
    le_avdata_DeleteRecord(recRef);

    LE_INFO("Pass");
}


void PushMix_04()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

/*
RecordString TsVarString = 20170410-153946 @ 1485134809
RecordInt TsVarInt = 15140 @ 1485134812
RecordBool TsVarBool = 1 @ 1485134815
RecordFloat TsVarFloat = 1.075843 @ 1485134818
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-153946", 1485134809) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 15140, 1485134812) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", true, 1485134815) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 1.075843, 1485134818) == LE_OK);

/*
RecordString TsVarString = 20170410-153951 @ 1485139808
RecordInt TsVarInt = 15587 @ 1485139810
RecordBool TsVarBool = 0 @ 1485139811
RecordFloat TsVarFloat = 0.494045 @ 1485139813
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-153951", 1485139808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 15587, 1485139810) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", false, 1485139811) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 0.494045, 1485139813) == LE_OK);

/*
RecordString TsVarString = 20170410-153956 @ 1485144808
RecordInt TsVarInt = 2086 @ 1485144810
RecordBool TsVarBool = 1 @ 1485144812
RecordFloat TsVarFloat = 1.483310 @ 1485144813
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-153956", 1485144808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 2086, 1485144810) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", true, 1485144812) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 1.483310, 1485144813) == LE_OK);

/*
RecordString TsVarString = 20170410-154001 @ 1485149808
RecordInt TsVarInt = 8967 @ 1485149810
RecordBool TsVarBool = 0 @ 1485149812
RecordFloat TsVarFloat = 13.533650 @ 1485149814
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-154001", 1485149808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 8967, 1485149810) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", false, 1485149812) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 13.533650, 1485149814) == LE_OK);

/*
RecordString TsVarString = 20170410-154006 @ 1485154808
RecordInt TsVarInt = 8752 @ 1485154810
RecordBool TsVarBool = 1 @ 1485154813
RecordFloat TsVarFloat = 1.001101 @ 1485154814
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-154006", 1485154808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 8752, 1485154810) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", true, 1485154813) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 1.001101, 1485154814) == LE_OK);

/*
RecordString TsVarString = 20170410-154011 @ 1485159808
RecordInt TsVarInt = 14717 @ 1485159811
RecordBool TsVarBool = 0 @ 1485159813
RecordFloat TsVarFloat = 0.384943 @ 1485159815
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-154011", 1485159808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 14717, 1485159811) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", false, 1485159813) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat",  0.384943, 1485159815) == LE_OK);


/*
RecordString TsVarString = 20170410-154016 @ 1485164808
RecordInt TsVarInt = 3013 @ 1485164811
RecordBool TsVarBool = 1 @ 1485164813
RecordFloat TsVarFloat = 0.514709 @ 1485164816
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-154016", 1485164808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 3013, 1485164811) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", true, 1485164813) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 0.514709, 1485164816) == LE_OK);


/*
RecordString TsVarString = 20170410-154021 @ 1485169808
RecordInt TsVarInt = 3903 @ 1485169811
RecordBool TsVarBool = 0 @ 1485169814
RecordFloat TsVarFloat = 0.500076 @ 1485169816
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-154021", 1485169808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 3903, 1485169811) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", false, 1485169814) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 0.500076, 1485169816) == LE_OK);

/*
RecordString TsVarString = 20170410-154026 @ 1485174808
RecordInt TsVarInt = 6758 @ 1485174811
RecordBool TsVarBool = 1 @ 1485174814
RecordFloat TsVarFloat = 1.442721 @ 1485174817
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-154026", 1485174808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 6758, 1485174811) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", true, 1485174814) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 1.442721, 1485174817) == LE_OK);

/*
RecordString TsVarString = 20170410-154031 @ 1485179808
RecordInt TsVarInt = 15771 @ 1485179811
RecordBool TsVarBool = 0 @ 1485179814
RecordFloat TsVarFloat = 0.510231 @ 1485179817
*/
    LE_ASSERT(le_avdata_RecordString(recRef, "TsVarString", "20170410-154031", 1485179808) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 15771, 1485179811) == LE_OK);
    LE_ASSERT(le_avdata_RecordBool(recRef, "TsVarBool", false, 1485179814) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 0.510231, 1485179817) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
}


void PushMix_05()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();

    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 15140, 1485134812) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 15150, 1485134814) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 1.075843, 1485134818) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 2.075843, 1485134820) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 15160, 1485134816) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 15170, 1485134818) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 3.075843, 1485134822) == LE_OK);
    LE_ASSERT(le_avdata_RecordFloat(recRef, "TsVarFloat", 4.075843, 1485134824) == LE_OK);

    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackHandler, NULL) == LE_OK);
}


static void PushCallbackRecord1
(
    le_avdata_PushStatus_t status,
    void* contextPtr
)
{
    int value = (int)contextPtr;
    LE_INFO("PushCallbackRecord1: %d, value: %d", status, value);
}

static void PushCallbackRecord2
(
    le_avdata_PushStatus_t status,
    void* contextPtr
)
{
    int value = (int)contextPtr;
    LE_INFO("PushCallbackRecord2: %d, value: %d", status, value);
}


// Pushing different records through different callbacks.
// Verify if corresponding callback is returned with the correct contextPtr.
void PushMix_06()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();
    le_avdata_RecordRef_t recRef2 = le_avdata_CreateRecord();
    LE_ASSERT(le_avdata_RecordInt(recRef, "TsVarInt", 123, 1485134812) == LE_OK);
    LE_ASSERT(le_avdata_RecordInt(recRef2, "TsVarInt", 456, 1485134812) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef, PushCallbackRecord1, (void *)123) == LE_OK);
    LE_ASSERT(le_avdata_PushRecord(recRef2, PushCallbackRecord2, (void *)456) == LE_OK);
}


// Push one big and small record
void PushMix_07()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();
    le_avdata_RecordRef_t recRef2 = le_avdata_CreateRecord();

    le_result_t result = LE_OK;
    uint64_t timestamp = 1412320402000;

    // fill record 1
    while (result != LE_NO_MEMORY)
    {
        int randSize = rand() % 10; // this number can be adjusted to fit more/less data
        char buff[255];
        gen_random(buff, randSize);
        result = le_avdata_RecordString(recRef, "strOverflow", buff, timestamp);
        timestamp += 100;
    }

    result = LE_OK;

    int value = 0;
    // fill record 2 (this one is relatively small once it's compressed)
    while (result != LE_NO_MEMORY)
    {
        result = le_avdata_RecordInt(recRef2, "intOverflow", value, timestamp);
        value++;
        timestamp += 100;
    }

    // push both records
    result = le_avdata_PushRecord(recRef, PushCallbackRecord1, (void *)1);
    LE_INFO("Pushing first records: %s", LE_RESULT_TXT(result));

    result = le_avdata_PushRecord(recRef2, PushCallbackRecord2, (void *)2);
    LE_INFO("Pushing second records: %s", LE_RESULT_TXT(result));
}


// Push 2 big records
void PushMix_08()
{
    le_avdata_RecordRef_t recRef = le_avdata_CreateRecord();
    le_avdata_RecordRef_t recRef2 = le_avdata_CreateRecord();

    le_result_t result = LE_OK;
    uint64_t timestamp = 1412320402000;

    // fill record 1
    while (result != LE_NO_MEMORY)
    {
        int randSize = rand() % 10; // this number can be adjusted to fit more/less data
        char buff[255];
        gen_random(buff, randSize);
        result = le_avdata_RecordString(recRef, "strOverflow", buff, timestamp);
        timestamp += 100;
    }

    result = LE_OK;

    while (result != LE_NO_MEMORY)
    {
        double value = rand_float(0,1);
        result = le_avdata_RecordFloat(recRef2, "floatOverflow", value, timestamp);
        timestamp += 100;
    }

    // push both records
    result = le_avdata_PushRecord(recRef, PushCallbackRecord1, (void *)1);
    LE_INFO("Pushing first records: %s", LE_RESULT_TXT(result));

    result = le_avdata_PushRecord(recRef2, PushCallbackRecord2, (void *)2);
    LE_INFO("Pushing second records: %s", LE_RESULT_TXT(result));
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
        // integer test cases
        case 1:
            PushInt_01();
            break;
        case 2:
            PushInt_02();
            break;
        case 3:
            PushInt_03();
            break;
        case 4:
            PushInt_04();
            break;
        case 5:
            PushInt_05();
            break;
        case 6:
            PushFloat_01();
            break;
        case 7:
            PushFloat_02();
            break;
        case 8:
            PushFloat_03();
            break;
        case 9:
            //PushFloat_04();
            break;
        case 10:
            PushFloat_05();
            break;
        case 11:
            PushBoolean_01();
            break;
        case 12:
            PushBoolean_02();
            break;
        case 13:
            PushBoolean_03();
            break;
        case 14:
            //PushBoolean_04();
            break;
        case 15:
            PushBoolean_05();
            break;
        case 16:
            PushString_01();
            break;
        case 17:
            PushString_02();
            break;
        case 18:
            PushString_03();
            break;
        case 19:
            //PushString_04();
            break;
        case 20:
            PushString_05();
            break;
        case 21:
            RecordInvalidValue();
            break;
        case 22:
            UnorderedTimestamp();
            break;
        case 23:
            PushMultipleRecords();
            break;
        case 24:
            PushMix_01();
            break;
        case 25:
            PushMix_02();
            break;
        case 26:
            PushMix_03();
            break;
        case 27:
            PushMix_04();
            break;
        case 28:
            PushMix_05();
            break;
        case 29:
            PushMix_06();
            break;
        case 30:
            PushMix_07();
            break;
        case 31:
            PushMix_08();
            break;
        default:
            LE_INFO("Invalid test case");
            break;
    }
}
