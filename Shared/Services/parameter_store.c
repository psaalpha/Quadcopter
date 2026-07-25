#include "parameter_store.h"

#include <limits.h>
#include <string.h>

#define PARAMETER_IMAGE_MAGIC  0x52415051u

static uint16_t ParameterStore_ReadU16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0]
        | ((uint16_t)data[1] << 8));
}

static uint32_t ParameterStore_ReadU32(const uint8_t *data)
{
    return (uint32_t)data[0]
        | ((uint32_t)data[1] << 8)
        | ((uint32_t)data[2] << 16)
        | ((uint32_t)data[3] << 24);
}

static void ParameterStore_WriteU16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void ParameterStore_WriteU32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8) & 0xFFu);
    data[2] = (uint8_t)((value >> 16) & 0xFFu);
    data[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint32_t ParameterStore_Crc32(
    const uint8_t *data,
    uint16_t length)
{
    uint16_t index;
    uint8_t bit;
    uint32_t crc = 0xFFFFFFFFu;

    for (index = 0u; index < length; ++index)
    {
        crc ^= data[index];
        for (bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static uint8_t ParameterStore_TypeIsValid(ParameterType type)
{
    return ((type == PARAMETER_TYPE_FLOAT32)
        || (type == PARAMETER_TYPE_INT32)
        || (type == PARAMETER_TYPE_UINT32))
        ? 1u
        : 0u;
}

static uint8_t ParameterStore_ValueIsValid(
    ParameterType type,
    ParameterValue value,
    ParameterValue minimum,
    ParameterValue maximum)
{
    switch (type)
    {
        case PARAMETER_TYPE_FLOAT32:
            if ((value.float32 != value.float32)
                || (minimum.float32 != minimum.float32)
                || (maximum.float32 != maximum.float32))
            {
                return 0u;
            }
            return ((minimum.float32 <= maximum.float32)
                && (value.float32 >= minimum.float32)
                && (value.float32 <= maximum.float32))
                ? 1u
                : 0u;

        case PARAMETER_TYPE_INT32:
            return ((minimum.int32 <= maximum.int32)
                && (value.int32 >= minimum.int32)
                && (value.int32 <= maximum.int32))
                ? 1u
                : 0u;

        case PARAMETER_TYPE_UINT32:
            return ((minimum.uint32 <= maximum.uint32)
                && (value.uint32 >= minimum.uint32)
                && (value.uint32 <= maximum.uint32))
                ? 1u
                : 0u;

        default:
            return 0u;
    }
}

static uint8_t ParameterStore_ValuesEqual(
    ParameterType type,
    ParameterValue left,
    ParameterValue right)
{
    switch (type)
    {
        case PARAMETER_TYPE_FLOAT32:
            return (left.float32 == right.float32) ? 1u : 0u;
        case PARAMETER_TYPE_INT32:
            return (left.int32 == right.int32) ? 1u : 0u;
        case PARAMETER_TYPE_UINT32:
            return (left.uint32 == right.uint32) ? 1u : 0u;
        default:
            return 0u;
    }
}

static uint32_t ParameterStore_ValueToRaw(
    ParameterType type,
    ParameterValue value)
{
    uint32_t raw = 0u;

    if (type == PARAMETER_TYPE_FLOAT32)
    {
        memcpy(&raw, &value.float32, sizeof(raw));
    }
    else if (type == PARAMETER_TYPE_INT32)
    {
        memcpy(&raw, &value.int32, sizeof(raw));
    }
    else
    {
        raw = value.uint32;
    }

    return raw;
}

static ParameterValue ParameterStore_RawToValue(
    ParameterType type,
    uint32_t raw)
{
    ParameterValue value;

    value.uint32 = 0u;
    if (type == PARAMETER_TYPE_FLOAT32)
    {
        memcpy(&value.float32, &raw, sizeof(raw));
    }
    else if (type == PARAMETER_TYPE_INT32)
    {
        memcpy(&value.int32, &raw, sizeof(raw));
    }
    else
    {
        value.uint32 = raw;
    }

    return value;
}

static int32_t ParameterStore_FindIndex(
    const ParameterStore *store,
    uint16_t id)
{
    uint16_t index;

    for (index = 0u; index < store->count; ++index)
    {
        if (store->descriptors[index].id == id)
        {
            return (int32_t)index;
        }
    }

    return -1;
}

static void ParameterStore_IncrementRevision(ParameterStore *store)
{
    if (store->revision != UINT32_MAX)
    {
        store->revision++;
    }
}

ParameterStatus ParameterStore_Init(
    ParameterStore *store,
    const ParameterDescriptor *descriptors,
    ParameterValue *value_storage,
    uint16_t count,
    uint16_t schema_version)
{
    uint16_t index;
    uint16_t other_index;

    if ((store == 0) || (descriptors == 0) || (value_storage == 0)
        || (count == 0u) || (schema_version == 0u))
    {
        return PARAMETER_STATUS_INVALID_ARGUMENT;
    }
    if ((sizeof(float) != sizeof(uint32_t))
        || (count > (uint16_t)((UINT16_MAX - PARAMETER_IMAGE_HEADER_SIZE)
            / PARAMETER_IMAGE_ENTRY_SIZE)))
    {
        return PARAMETER_STATUS_UNSUPPORTED;
    }

    for (index = 0u; index < count; ++index)
    {
        const ParameterDescriptor *descriptor = &descriptors[index];

        if (!ParameterStore_TypeIsValid(descriptor->type)
            || !ParameterStore_ValueIsValid(
                descriptor->type,
                descriptor->default_value,
                descriptor->minimum,
                descriptor->maximum))
        {
            return PARAMETER_STATUS_INVALID_ARGUMENT;
        }

        for (other_index = (uint16_t)(index + 1u);
             other_index < count;
             ++other_index)
        {
            if (descriptor->id == descriptors[other_index].id)
            {
                return PARAMETER_STATUS_INVALID_ARGUMENT;
            }
        }
    }

    store->descriptors = descriptors;
    store->values = value_storage;
    store->count = count;
    store->schema_version = schema_version;
    store->revision = 1u;
    store->dirty = 1u;
    store->initialized = 1u;

    for (index = 0u; index < count; ++index)
    {
        store->values[index] = descriptors[index].default_value;
    }

    return PARAMETER_STATUS_OK;
}

ParameterStatus ParameterStore_Get(
    const ParameterStore *store,
    uint16_t id,
    ParameterType *type,
    ParameterValue *value)
{
    int32_t index;

    if ((store == 0) || (type == 0) || (value == 0))
    {
        return PARAMETER_STATUS_INVALID_ARGUMENT;
    }
    if (!store->initialized)
    {
        return PARAMETER_STATUS_NOT_INITIALIZED;
    }

    index = ParameterStore_FindIndex(store, id);
    if (index < 0)
    {
        return PARAMETER_STATUS_NOT_FOUND;
    }

    *type = store->descriptors[index].type;
    *value = store->values[index];
    return PARAMETER_STATUS_OK;
}

ParameterStatus ParameterStore_Set(
    ParameterStore *store,
    uint16_t id,
    ParameterType type,
    ParameterValue value)
{
    int32_t index;
    const ParameterDescriptor *descriptor;

    if (store == 0)
    {
        return PARAMETER_STATUS_INVALID_ARGUMENT;
    }
    if (!store->initialized)
    {
        return PARAMETER_STATUS_NOT_INITIALIZED;
    }

    index = ParameterStore_FindIndex(store, id);
    if (index < 0)
    {
        return PARAMETER_STATUS_NOT_FOUND;
    }

    descriptor = &store->descriptors[index];
    if (type != descriptor->type)
    {
        return PARAMETER_STATUS_TYPE_MISMATCH;
    }
    if ((descriptor->flags & PARAMETER_FLAG_RUNTIME_WRITABLE) == 0u)
    {
        return PARAMETER_STATUS_READ_ONLY;
    }
    if (!ParameterStore_ValueIsValid(
            descriptor->type,
            value,
            descriptor->minimum,
            descriptor->maximum))
    {
        return PARAMETER_STATUS_OUT_OF_RANGE;
    }

    if (!ParameterStore_ValuesEqual(type, store->values[index], value))
    {
        store->values[index] = value;
        store->dirty = 1u;
        ParameterStore_IncrementRevision(store);
    }

    return PARAMETER_STATUS_OK;
}

ParameterStatus ParameterStore_ResetDefaults(ParameterStore *store)
{
    uint16_t index;

    if (store == 0)
    {
        return PARAMETER_STATUS_INVALID_ARGUMENT;
    }
    if (!store->initialized)
    {
        return PARAMETER_STATUS_NOT_INITIALIZED;
    }

    for (index = 0u; index < store->count; ++index)
    {
        store->values[index] = store->descriptors[index].default_value;
    }
    store->dirty = 1u;
    ParameterStore_IncrementRevision(store);
    return PARAMETER_STATUS_OK;
}

uint8_t ParameterStore_IsDirty(const ParameterStore *store)
{
    return ((store != 0) && store->initialized && store->dirty) ? 1u : 0u;
}

uint32_t ParameterStore_GetRevision(const ParameterStore *store)
{
    return ((store != 0) && store->initialized) ? store->revision : 0u;
}

ParameterStatus ParameterStore_MarkPersisted(
    ParameterStore *store,
    uint32_t persisted_revision)
{
    if (store == 0)
    {
        return PARAMETER_STATUS_INVALID_ARGUMENT;
    }
    if (!store->initialized)
    {
        return PARAMETER_STATUS_NOT_INITIALIZED;
    }
    if (persisted_revision != store->revision)
    {
        return PARAMETER_STATUS_STALE_REVISION;
    }

    store->dirty = 0u;
    return PARAMETER_STATUS_OK;
}

uint16_t ParameterStore_EncodedSize(const ParameterStore *store)
{
    if ((store == 0) || !store->initialized)
    {
        return 0u;
    }

    return (uint16_t)(PARAMETER_IMAGE_HEADER_SIZE
        + (store->count * PARAMETER_IMAGE_ENTRY_SIZE));
}

ParameterStatus ParameterStore_Encode(
    const ParameterStore *store,
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t *written_size)
{
    uint16_t index;
    uint16_t required_size;
    uint16_t payload_size;
    uint8_t *entry;
    uint32_t crc;

    if ((store == 0) || (buffer == 0) || (written_size == 0))
    {
        return PARAMETER_STATUS_INVALID_ARGUMENT;
    }
    if (!store->initialized)
    {
        return PARAMETER_STATUS_NOT_INITIALIZED;
    }

    required_size = ParameterStore_EncodedSize(store);
    if (capacity < required_size)
    {
        return PARAMETER_STATUS_BUFFER_TOO_SMALL;
    }

    payload_size = (uint16_t)(store->count * PARAMETER_IMAGE_ENTRY_SIZE);
    ParameterStore_WriteU32(&buffer[0], PARAMETER_IMAGE_MAGIC);
    ParameterStore_WriteU16(&buffer[4], store->schema_version);
    ParameterStore_WriteU16(&buffer[6], store->count);
    ParameterStore_WriteU16(&buffer[8], payload_size);
    ParameterStore_WriteU16(&buffer[10], 0u);

    for (index = 0u; index < store->count; ++index)
    {
        entry = &buffer[PARAMETER_IMAGE_HEADER_SIZE
            + (index * PARAMETER_IMAGE_ENTRY_SIZE)];
        ParameterStore_WriteU16(entry, store->descriptors[index].id);
        entry[2] = (uint8_t)store->descriptors[index].type;
        entry[3] = 0u;
        ParameterStore_WriteU32(
            &entry[4],
            ParameterStore_ValueToRaw(
                store->descriptors[index].type,
                store->values[index]));
    }

    crc = ParameterStore_Crc32(
        &buffer[PARAMETER_IMAGE_HEADER_SIZE],
        payload_size);
    ParameterStore_WriteU32(&buffer[12], crc);
    *written_size = required_size;
    return PARAMETER_STATUS_OK;
}

ParameterStatus ParameterStore_Decode(
    ParameterStore *store,
    const uint8_t *buffer,
    uint16_t size)
{
    uint16_t descriptor_index;
    uint16_t entry_index;
    uint16_t image_count;
    uint16_t payload_size;
    uint16_t expected_size;
    uint16_t matches;
    const uint8_t *entry;
    ParameterValue value;
    uint32_t expected_crc;
    uint32_t actual_crc;

    if ((store == 0) || (buffer == 0))
    {
        return PARAMETER_STATUS_INVALID_ARGUMENT;
    }
    if (!store->initialized)
    {
        return PARAMETER_STATUS_NOT_INITIALIZED;
    }
    if (size < PARAMETER_IMAGE_HEADER_SIZE)
    {
        return PARAMETER_STATUS_FORMAT_ERROR;
    }
    if (ParameterStore_ReadU32(&buffer[0]) != PARAMETER_IMAGE_MAGIC)
    {
        return PARAMETER_STATUS_FORMAT_ERROR;
    }
    if (ParameterStore_ReadU16(&buffer[4]) != store->schema_version)
    {
        return PARAMETER_STATUS_SCHEMA_MISMATCH;
    }

    image_count = ParameterStore_ReadU16(&buffer[6]);
    payload_size = ParameterStore_ReadU16(&buffer[8]);
    expected_size = (uint16_t)(PARAMETER_IMAGE_HEADER_SIZE + payload_size);
    if ((image_count != store->count)
        || (payload_size
            != (uint16_t)(image_count * PARAMETER_IMAGE_ENTRY_SIZE))
        || (size != expected_size))
    {
        return PARAMETER_STATUS_FORMAT_ERROR;
    }

    expected_crc = ParameterStore_ReadU32(&buffer[12]);
    actual_crc = ParameterStore_Crc32(
        &buffer[PARAMETER_IMAGE_HEADER_SIZE],
        payload_size);
    if (actual_crc != expected_crc)
    {
        return PARAMETER_STATUS_CRC_ERROR;
    }

    /*
     * Validate the complete image first. No live value changes until every
     * descriptor is present exactly once, has the expected type, and is in
     * range.
     */
    for (descriptor_index = 0u;
         descriptor_index < store->count;
         ++descriptor_index)
    {
        matches = 0u;
        for (entry_index = 0u; entry_index < image_count; ++entry_index)
        {
            entry = &buffer[PARAMETER_IMAGE_HEADER_SIZE
                + (entry_index * PARAMETER_IMAGE_ENTRY_SIZE)];
            if (ParameterStore_ReadU16(entry)
                != store->descriptors[descriptor_index].id)
            {
                continue;
            }

            matches++;
            if ((ParameterType)entry[2]
                != store->descriptors[descriptor_index].type)
            {
                return PARAMETER_STATUS_TYPE_MISMATCH;
            }
            value = ParameterStore_RawToValue(
                store->descriptors[descriptor_index].type,
                ParameterStore_ReadU32(&entry[4]));
            if (!ParameterStore_ValueIsValid(
                    store->descriptors[descriptor_index].type,
                    value,
                    store->descriptors[descriptor_index].minimum,
                    store->descriptors[descriptor_index].maximum))
            {
                return PARAMETER_STATUS_OUT_OF_RANGE;
            }
        }

        if (matches != 1u)
        {
            return PARAMETER_STATUS_FORMAT_ERROR;
        }
    }

    for (descriptor_index = 0u;
         descriptor_index < store->count;
         ++descriptor_index)
    {
        for (entry_index = 0u; entry_index < image_count; ++entry_index)
        {
            entry = &buffer[PARAMETER_IMAGE_HEADER_SIZE
                + (entry_index * PARAMETER_IMAGE_ENTRY_SIZE)];
            if (ParameterStore_ReadU16(entry)
                == store->descriptors[descriptor_index].id)
            {
                store->values[descriptor_index] = ParameterStore_RawToValue(
                    store->descriptors[descriptor_index].type,
                    ParameterStore_ReadU32(&entry[4]));
                break;
            }
        }
    }

    store->dirty = 0u;
    ParameterStore_IncrementRevision(store);
    return PARAMETER_STATUS_OK;
}
