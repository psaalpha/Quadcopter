#ifndef PARAMETER_STORE_H
#define PARAMETER_STORE_H

#include <stdint.h>

#define PARAMETER_FLAG_RUNTIME_WRITABLE  0x0001u
#define PARAMETER_FLAG_PERSISTENT        0x0002u
#define PARAMETER_FLAG_REBOOT_REQUIRED   0x0004u

#define PARAMETER_IMAGE_HEADER_SIZE      16u
#define PARAMETER_IMAGE_ENTRY_SIZE       8u

typedef enum
{
    PARAMETER_STATUS_OK = 0,
    PARAMETER_STATUS_INVALID_ARGUMENT,
    PARAMETER_STATUS_NOT_INITIALIZED,
    PARAMETER_STATUS_NOT_FOUND,
    PARAMETER_STATUS_TYPE_MISMATCH,
    PARAMETER_STATUS_OUT_OF_RANGE,
    PARAMETER_STATUS_READ_ONLY,
    PARAMETER_STATUS_BUFFER_TOO_SMALL,
    PARAMETER_STATUS_FORMAT_ERROR,
    PARAMETER_STATUS_SCHEMA_MISMATCH,
    PARAMETER_STATUS_CRC_ERROR,
    PARAMETER_STATUS_STALE_REVISION,
    PARAMETER_STATUS_UNSUPPORTED
} ParameterStatus;

typedef enum
{
    PARAMETER_TYPE_FLOAT32 = 0,
    PARAMETER_TYPE_INT32,
    PARAMETER_TYPE_UINT32
} ParameterType;

typedef union
{
    float float32;
    int32_t int32;
    uint32_t uint32;
} ParameterValue;

typedef struct
{
    uint16_t id;
    ParameterType type;
    ParameterValue default_value;
    ParameterValue minimum;
    ParameterValue maximum;
    uint16_t flags;
} ParameterDescriptor;

typedef struct
{
    const ParameterDescriptor *descriptors;
    ParameterValue *values;
    uint16_t count;
    uint16_t schema_version;
    uint32_t revision;
    uint8_t dirty;
    uint8_t initialized;
} ParameterStore;

ParameterStatus ParameterStore_Init(
    ParameterStore *store,
    const ParameterDescriptor *descriptors,
    ParameterValue *value_storage,
    uint16_t count,
    uint16_t schema_version);
ParameterStatus ParameterStore_Get(
    const ParameterStore *store,
    uint16_t id,
    ParameterType *type,
    ParameterValue *value);
ParameterStatus ParameterStore_Set(
    ParameterStore *store,
    uint16_t id,
    ParameterType type,
    ParameterValue value);
ParameterStatus ParameterStore_ResetDefaults(ParameterStore *store);
uint8_t ParameterStore_IsDirty(const ParameterStore *store);
uint32_t ParameterStore_GetRevision(const ParameterStore *store);
ParameterStatus ParameterStore_MarkPersisted(
    ParameterStore *store,
    uint32_t persisted_revision);
uint16_t ParameterStore_EncodedSize(const ParameterStore *store);
ParameterStatus ParameterStore_Encode(
    const ParameterStore *store,
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t *written_size);
ParameterStatus ParameterStore_Decode(
    ParameterStore *store,
    const uint8_t *buffer,
    uint16_t size);

#endif
