#include "parameter_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_PARAMETER_COUNT  3u
#define TEST_SCHEMA_VERSION   7u
#define TEST_IMAGE_CAPACITY   64u

#define PARAM_ID_GAIN         10u
#define PARAM_ID_OFFSET       20u
#define PARAM_ID_DEVICE_ID    30u

static void BuildDescriptors(
    ParameterDescriptor *descriptors,
    int32_t offset_maximum,
    float gain_default)
{
    memset(
        descriptors,
        0,
        sizeof(ParameterDescriptor) * TEST_PARAMETER_COUNT);

    descriptors[0].id = PARAM_ID_GAIN;
    descriptors[0].type = PARAMETER_TYPE_FLOAT32;
    descriptors[0].default_value.float32 = gain_default;
    descriptors[0].minimum.float32 = 0.0f;
    descriptors[0].maximum.float32 = 10.0f;
    descriptors[0].flags =
        PARAMETER_FLAG_RUNTIME_WRITABLE | PARAMETER_FLAG_PERSISTENT;

    descriptors[1].id = PARAM_ID_OFFSET;
    descriptors[1].type = PARAMETER_TYPE_INT32;
    descriptors[1].default_value.int32 = 0;
    descriptors[1].minimum.int32 = -10;
    descriptors[1].maximum.int32 = offset_maximum;
    descriptors[1].flags =
        PARAMETER_FLAG_RUNTIME_WRITABLE | PARAMETER_FLAG_PERSISTENT;

    descriptors[2].id = PARAM_ID_DEVICE_ID;
    descriptors[2].type = PARAMETER_TYPE_UINT32;
    descriptors[2].default_value.uint32 = 42u;
    descriptors[2].minimum.uint32 = 1u;
    descriptors[2].maximum.uint32 = 255u;
    descriptors[2].flags = PARAMETER_FLAG_PERSISTENT;
}

static void TestDefaultsValidationAndRevision(void)
{
    ParameterDescriptor descriptors[TEST_PARAMETER_COUNT];
    ParameterValue storage[TEST_PARAMETER_COUNT];
    ParameterStore store;
    ParameterValue value;
    ParameterType type;
    uint32_t revision;

    BuildDescriptors(descriptors, 10, 1.5f);
    assert(ParameterStore_Init(
        &store,
        descriptors,
        storage,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_STATUS_OK);

    assert(ParameterStore_IsDirty(&store) == 1u);
    assert(ParameterStore_GetRevision(&store) == 1u);
    assert(ParameterStore_Get(
        &store,
        PARAM_ID_GAIN,
        &type,
        &value) == PARAMETER_STATUS_OK);
    assert(type == PARAMETER_TYPE_FLOAT32);
    assert(value.float32 == 1.5f);

    assert(ParameterStore_MarkPersisted(
        &store,
        0u) == PARAMETER_STATUS_STALE_REVISION);
    assert(ParameterStore_MarkPersisted(
        &store,
        1u) == PARAMETER_STATUS_OK);
    assert(ParameterStore_IsDirty(&store) == 0u);

    value.float32 = 2.25f;
    assert(ParameterStore_Set(
        &store,
        PARAM_ID_GAIN,
        PARAMETER_TYPE_FLOAT32,
        value) == PARAMETER_STATUS_OK);
    revision = ParameterStore_GetRevision(&store);
    assert(revision == 2u);
    assert(ParameterStore_IsDirty(&store) == 1u);

    assert(ParameterStore_Set(
        &store,
        PARAM_ID_GAIN,
        PARAMETER_TYPE_FLOAT32,
        value) == PARAMETER_STATUS_OK);
    assert(ParameterStore_GetRevision(&store) == revision);

    value.float32 = 11.0f;
    assert(ParameterStore_Set(
        &store,
        PARAM_ID_GAIN,
        PARAMETER_TYPE_FLOAT32,
        value) == PARAMETER_STATUS_OUT_OF_RANGE);
    assert(ParameterStore_Set(
        &store,
        PARAM_ID_GAIN,
        PARAMETER_TYPE_UINT32,
        value) == PARAMETER_STATUS_TYPE_MISMATCH);

    value.uint32 = 80u;
    assert(ParameterStore_Set(
        &store,
        PARAM_ID_DEVICE_ID,
        PARAMETER_TYPE_UINT32,
        value) == PARAMETER_STATUS_READ_ONLY);
    assert(ParameterStore_Set(
        &store,
        999u,
        PARAMETER_TYPE_UINT32,
        value) == PARAMETER_STATUS_NOT_FOUND);
}

static void TestEncodeDecodeAndCrc(void)
{
    ParameterDescriptor descriptors[TEST_PARAMETER_COUNT];
    ParameterValue source_storage[TEST_PARAMETER_COUNT];
    ParameterValue target_storage[TEST_PARAMETER_COUNT];
    ParameterStore source;
    ParameterStore target;
    ParameterValue value;
    ParameterType type;
    uint8_t image[TEST_IMAGE_CAPACITY];
    uint8_t corrupted[TEST_IMAGE_CAPACITY];
    uint16_t written_size = 0u;

    BuildDescriptors(descriptors, 10, 1.5f);
    assert(ParameterStore_Init(
        &source,
        descriptors,
        source_storage,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_STATUS_OK);

    value.float32 = 3.5f;
    assert(ParameterStore_Set(
        &source,
        PARAM_ID_GAIN,
        PARAMETER_TYPE_FLOAT32,
        value) == PARAMETER_STATUS_OK);
    value.int32 = -4;
    assert(ParameterStore_Set(
        &source,
        PARAM_ID_OFFSET,
        PARAMETER_TYPE_INT32,
        value) == PARAMETER_STATUS_OK);

    assert(ParameterStore_EncodedSize(&source)
        == PARAMETER_IMAGE_HEADER_SIZE
            + (TEST_PARAMETER_COUNT * PARAMETER_IMAGE_ENTRY_SIZE));
    assert(ParameterStore_Encode(
        &source,
        image,
        TEST_IMAGE_CAPACITY,
        &written_size) == PARAMETER_STATUS_OK);
    assert(written_size == ParameterStore_EncodedSize(&source));

    assert(ParameterStore_Init(
        &target,
        descriptors,
        target_storage,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_STATUS_OK);
    assert(ParameterStore_Decode(
        &target,
        image,
        written_size) == PARAMETER_STATUS_OK);
    assert(ParameterStore_IsDirty(&target) == 0u);

    assert(ParameterStore_Get(
        &target,
        PARAM_ID_GAIN,
        &type,
        &value) == PARAMETER_STATUS_OK);
    assert(value.float32 == 3.5f);
    assert(ParameterStore_Get(
        &target,
        PARAM_ID_OFFSET,
        &type,
        &value) == PARAMETER_STATUS_OK);
    assert(value.int32 == -4);

    memcpy(corrupted, image, written_size);
    corrupted[PARAMETER_IMAGE_HEADER_SIZE + 4u] ^= 0x01u;
    assert(ParameterStore_Decode(
        &target,
        corrupted,
        written_size) == PARAMETER_STATUS_CRC_ERROR);

    memcpy(corrupted, image, written_size);
    corrupted[4] ^= 0x01u;
    assert(ParameterStore_Decode(
        &target,
        corrupted,
        written_size) == PARAMETER_STATUS_SCHEMA_MISMATCH);
}

static void TestDecodeIsTransactional(void)
{
    ParameterDescriptor source_descriptors[TEST_PARAMETER_COUNT];
    ParameterDescriptor target_descriptors[TEST_PARAMETER_COUNT];
    ParameterValue source_storage[TEST_PARAMETER_COUNT];
    ParameterValue target_storage[TEST_PARAMETER_COUNT];
    ParameterStore source;
    ParameterStore target;
    ParameterValue value;
    ParameterType type;
    uint8_t image[TEST_IMAGE_CAPACITY];
    uint16_t written_size = 0u;

    BuildDescriptors(source_descriptors, 10, 1.0f);
    assert(ParameterStore_Init(
        &source,
        source_descriptors,
        source_storage,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_STATUS_OK);

    value.float32 = 6.0f;
    assert(ParameterStore_Set(
        &source,
        PARAM_ID_GAIN,
        PARAMETER_TYPE_FLOAT32,
        value) == PARAMETER_STATUS_OK);
    value.int32 = 8;
    assert(ParameterStore_Set(
        &source,
        PARAM_ID_OFFSET,
        PARAMETER_TYPE_INT32,
        value) == PARAMETER_STATUS_OK);
    assert(ParameterStore_Encode(
        &source,
        image,
        TEST_IMAGE_CAPACITY,
        &written_size) == PARAMETER_STATUS_OK);

    BuildDescriptors(target_descriptors, 5, 9.0f);
    assert(ParameterStore_Init(
        &target,
        target_descriptors,
        target_storage,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_STATUS_OK);
    assert(ParameterStore_Decode(
        &target,
        image,
        written_size) == PARAMETER_STATUS_OUT_OF_RANGE);

    assert(ParameterStore_Get(
        &target,
        PARAM_ID_GAIN,
        &type,
        &value) == PARAMETER_STATUS_OK);
    assert(value.float32 == 9.0f);
    assert(ParameterStore_Get(
        &target,
        PARAM_ID_OFFSET,
        &type,
        &value) == PARAMETER_STATUS_OK);
    assert(value.int32 == 0);
}

int main(void)
{
    TestDefaultsValidationAndRevision();
    TestEncodeDecodeAndCrc();
    TestDecodeIsTransactional();

    puts("parameter store tests passed");
    return 0;
}
