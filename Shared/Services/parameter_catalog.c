#include "parameter_catalog.h"

#include <string.h>

static uint8_t ParameterCatalog_CategoryIsValid(
    ParameterCategory category)
{
    return (category <= PARAMETER_CATEGORY_DEBUG) ? 1u : 0u;
}

static uint8_t ParameterCatalog_UnitIsValid(ParameterUnit unit)
{
    return (unit <= PARAMETER_UNIT_TIMER_TICKS) ? 1u : 0u;
}

ParameterCatalogStatus ParameterCatalog_Init(
    ParameterCatalog *catalog,
    const ParameterMetadata *entries,
    uint16_t count,
    uint16_t schema_version)
{
    uint16_t index;
    uint16_t other;

    if ((catalog == 0) || (entries == 0)
        || (count == 0u) || (schema_version == 0u))
    {
        return PARAMETER_CATALOG_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0u; index < count; ++index)
    {
        if ((entries[index].name == 0) || (entries[index].name[0] == '\0')
            || !ParameterCatalog_CategoryIsValid(entries[index].category)
            || !ParameterCatalog_UnitIsValid(entries[index].unit)
            || (entries[index].introduced_schema == 0u)
            || (entries[index].introduced_schema > schema_version)
            || ((entries[index].deprecated_schema != 0u)
                && (entries[index].deprecated_schema
                    <= entries[index].introduced_schema)))
        {
            return PARAMETER_CATALOG_STATUS_INVALID_ARGUMENT;
        }

        for (other = (uint16_t)(index + 1u); other < count; ++other)
        {
            if ((entries[index].id == entries[other].id)
                || (strcmp(entries[index].name, entries[other].name) == 0))
            {
                return PARAMETER_CATALOG_STATUS_INVALID_ARGUMENT;
            }
        }
    }

    catalog->entries = entries;
    catalog->count = count;
    catalog->schema_version = schema_version;
    catalog->initialized = 1u;
    return PARAMETER_CATALOG_STATUS_OK;
}

const ParameterMetadata *ParameterCatalog_Find(
    const ParameterCatalog *catalog,
    uint16_t id)
{
    uint16_t index;

    if ((catalog == 0) || !catalog->initialized)
    {
        return 0;
    }

    for (index = 0u; index < catalog->count; ++index)
    {
        if (catalog->entries[index].id == id)
        {
            return &catalog->entries[index];
        }
    }
    return 0;
}

uint8_t ParameterCatalog_IsAvailable(
    const ParameterMetadata *metadata,
    uint16_t schema_version)
{
    if ((metadata == 0) || (schema_version < metadata->introduced_schema))
    {
        return 0u;
    }
    if ((metadata->deprecated_schema != 0u)
        && (schema_version >= metadata->deprecated_schema))
    {
        return 0u;
    }
    return 1u;
}

ParameterCatalogStatus ParameterCatalog_ValidateStore(
    const ParameterCatalog *catalog,
    const ParameterStore *store)
{
    uint16_t index;

    if ((catalog == 0) || (store == 0))
    {
        return PARAMETER_CATALOG_STATUS_INVALID_ARGUMENT;
    }
    if (!catalog->initialized || !store->initialized)
    {
        return PARAMETER_CATALOG_STATUS_NOT_INITIALIZED;
    }
    if (catalog->schema_version != store->schema_version)
    {
        return PARAMETER_CATALOG_STATUS_SCHEMA_MISMATCH;
    }
    if (catalog->count != store->count)
    {
        return PARAMETER_CATALOG_STATUS_INCOMPLETE;
    }

    for (index = 0u; index < store->count; ++index)
    {
        const ParameterMetadata *metadata = ParameterCatalog_Find(
            catalog,
            store->descriptors[index].id);
        if ((metadata == 0)
            || !ParameterCatalog_IsAvailable(
                metadata,
                catalog->schema_version))
        {
            return PARAMETER_CATALOG_STATUS_INCOMPLETE;
        }
    }

    return PARAMETER_CATALOG_STATUS_OK;
}
