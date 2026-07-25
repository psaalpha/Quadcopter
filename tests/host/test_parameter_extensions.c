#include "parameter_catalog.h"
#include "parameter_persistence.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_PARAMETER_COUNT  2u
#define TEST_SCHEMA_VERSION   3u
#define TEST_IMAGE_SIZE       64u

typedef struct
{
    uint8_t image[TEST_IMAGE_SIZE];
    uint16_t size;
    ParameterStore *store_to_mutate;
    uint8_t mutate_on_write;
} FakeParameterBackend;

static ParameterPersistenceStatus FakeRead(
    void *context,
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t *read_size)
{
    FakeParameterBackend *fake = (FakeParameterBackend *)context;

    if (capacity < fake->size)
    {
        return PARAMETER_PERSISTENCE_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy(buffer, fake->image, fake->size);
    *read_size = fake->size;
    return PARAMETER_PERSISTENCE_STATUS_OK;
}

static ParameterPersistenceStatus FakeWriteAtomic(
    void *context,
    const uint8_t *buffer,
    uint16_t size)
{
    FakeParameterBackend *fake = (FakeParameterBackend *)context;
    ParameterValue value;

    if (size > TEST_IMAGE_SIZE)
    {
        return PARAMETER_PERSISTENCE_STATUS_BUFFER_TOO_SMALL;
    }
    memcpy(fake->image, buffer, size);
    fake->size = size;

    if (fake->mutate_on_write)
    {
        value.float32 = 4.0f;
        assert(ParameterStore_Set(
            fake->store_to_mutate,
            10u,
            PARAMETER_TYPE_FLOAT32,
            value) == PARAMETER_STATUS_OK);
    }
    return PARAMETER_PERSISTENCE_STATUS_OK;
}

static void BuildDescriptors(ParameterDescriptor *descriptors)
{
    memset(
        descriptors,
        0,
        sizeof(ParameterDescriptor) * TEST_PARAMETER_COUNT);

    descriptors[0].id = 10u;
    descriptors[0].type = PARAMETER_TYPE_FLOAT32;
    descriptors[0].default_value.float32 = 1.0f;
    descriptors[0].minimum.float32 = 0.0f;
    descriptors[0].maximum.float32 = 10.0f;
    descriptors[0].flags =
        PARAMETER_FLAG_RUNTIME_WRITABLE | PARAMETER_FLAG_PERSISTENT;

    descriptors[1].id = 20u;
    descriptors[1].type = PARAMETER_TYPE_UINT32;
    descriptors[1].default_value.uint32 = 100u;
    descriptors[1].minimum.uint32 = 10u;
    descriptors[1].maximum.uint32 = 1000u;
    descriptors[1].flags = PARAMETER_FLAG_PERSISTENT;
}

static void TestCatalogMatchesStore(void)
{
    static const ParameterMetadata metadata[TEST_PARAMETER_COUNT] =
    {
        {
            10u,
            "control.roll_kp",
            PARAMETER_CATEGORY_CONTROL,
            PARAMETER_UNIT_NONE,
            1u,
            0u
        },
        {
            20u,
            "communication.timeout_ms",
            PARAMETER_CATEGORY_COMMUNICATION,
            PARAMETER_UNIT_MILLISECONDS,
            2u,
            0u
        }
    };
    ParameterDescriptor descriptors[TEST_PARAMETER_COUNT];
    ParameterValue storage[TEST_PARAMETER_COUNT];
    ParameterStore store;
    ParameterCatalog catalog;
    const ParameterMetadata *entry;

    BuildDescriptors(descriptors);
    assert(ParameterStore_Init(
        &store,
        descriptors,
        storage,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_STATUS_OK);
    assert(ParameterCatalog_Init(
        &catalog,
        metadata,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_CATALOG_STATUS_OK);
    assert(ParameterCatalog_ValidateStore(
        &catalog,
        &store) == PARAMETER_CATALOG_STATUS_OK);

    entry = ParameterCatalog_Find(&catalog, 10u);
    assert(entry != 0);
    assert(entry->category == PARAMETER_CATEGORY_CONTROL);
    assert(ParameterCatalog_IsAvailable(entry, 1u) == 1u);
    assert(ParameterCatalog_IsAvailable(&metadata[1], 1u) == 0u);
}

static void TestPersistenceSaveLoadAndStaleRevision(void)
{
    ParameterDescriptor descriptors[TEST_PARAMETER_COUNT];
    ParameterValue storage[TEST_PARAMETER_COUNT];
    ParameterStore store;
    ParameterPersistence persistence;
    ParameterPersistenceBackend backend;
    FakeParameterBackend fake = {0};
    uint8_t scratch[TEST_IMAGE_SIZE];
    ParameterValue value;
    ParameterType type;

    BuildDescriptors(descriptors);
    assert(ParameterStore_Init(
        &store,
        descriptors,
        storage,
        TEST_PARAMETER_COUNT,
        TEST_SCHEMA_VERSION) == PARAMETER_STATUS_OK);
    fake.store_to_mutate = &store;
    backend.read = FakeRead;
    backend.write_atomic = FakeWriteAtomic;
    backend.context = &fake;

    assert(ParameterPersistence_Init(
        &persistence,
        &store,
        &backend,
        scratch,
        sizeof(scratch)) == PARAMETER_PERSISTENCE_STATUS_OK);

    value.float32 = 2.5f;
    assert(ParameterStore_Set(
        &store,
        10u,
        PARAMETER_TYPE_FLOAT32,
        value) == PARAMETER_STATUS_OK);
    assert(ParameterPersistence_Save(
        &persistence) == PARAMETER_PERSISTENCE_STATUS_OK);
    assert(ParameterStore_IsDirty(&store) == 0u);

    assert(ParameterStore_ResetDefaults(&store) == PARAMETER_STATUS_OK);
    assert(ParameterPersistence_Load(
        &persistence) == PARAMETER_PERSISTENCE_STATUS_OK);
    assert(ParameterStore_Get(
        &store,
        10u,
        &type,
        &value) == PARAMETER_STATUS_OK);
    assert(value.float32 == 2.5f);

    value.float32 = 3.0f;
    assert(ParameterStore_Set(
        &store,
        10u,
        PARAMETER_TYPE_FLOAT32,
        value) == PARAMETER_STATUS_OK);
    fake.mutate_on_write = 1u;
    assert(ParameterPersistence_Save(
        &persistence) == PARAMETER_PERSISTENCE_STATUS_STALE_REVISION);
    assert(ParameterStore_IsDirty(&store) == 1u);
}

int main(void)
{
    TestCatalogMatchesStore();
    TestPersistenceSaveLoadAndStaleRevision();

    puts("parameter extension tests passed");
    return 0;
}
