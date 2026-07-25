#ifndef PARAMETER_CATALOG_H
#define PARAMETER_CATALOG_H

#include <stdint.h>

#include "parameter_store.h"

typedef enum
{
    PARAMETER_CATEGORY_SYSTEM = 0,
    PARAMETER_CATEGORY_SENSOR,
    PARAMETER_CATEGORY_CONTROL,
    PARAMETER_CATEGORY_COMMUNICATION,
    PARAMETER_CATEGORY_SAFETY,
    PARAMETER_CATEGORY_DEBUG
} ParameterCategory;

typedef enum
{
    PARAMETER_UNIT_NONE = 0,
    PARAMETER_UNIT_MILLISECONDS,
    PARAMETER_UNIT_HERTZ,
    PARAMETER_UNIT_PERCENT,
    PARAMETER_UNIT_DEGREES,
    PARAMETER_UNIT_DEGREES_PER_SECOND,
    PARAMETER_UNIT_MILLIVOLTS,
    PARAMETER_UNIT_TIMER_TICKS
} ParameterUnit;

typedef struct
{
    uint16_t id;
    const char *name;
    ParameterCategory category;
    ParameterUnit unit;
    uint16_t introduced_schema;
    uint16_t deprecated_schema;
} ParameterMetadata;

typedef struct
{
    const ParameterMetadata *entries;
    uint16_t count;
    uint16_t schema_version;
    uint8_t initialized;
} ParameterCatalog;

typedef enum
{
    PARAMETER_CATALOG_STATUS_OK = 0,
    PARAMETER_CATALOG_STATUS_INVALID_ARGUMENT,
    PARAMETER_CATALOG_STATUS_NOT_INITIALIZED,
    PARAMETER_CATALOG_STATUS_NOT_FOUND,
    PARAMETER_CATALOG_STATUS_SCHEMA_MISMATCH,
    PARAMETER_CATALOG_STATUS_INCOMPLETE
} ParameterCatalogStatus;

ParameterCatalogStatus ParameterCatalog_Init(
    ParameterCatalog *catalog,
    const ParameterMetadata *entries,
    uint16_t count,
    uint16_t schema_version);
const ParameterMetadata *ParameterCatalog_Find(
    const ParameterCatalog *catalog,
    uint16_t id);
uint8_t ParameterCatalog_IsAvailable(
    const ParameterMetadata *metadata,
    uint16_t schema_version);
ParameterCatalogStatus ParameterCatalog_ValidateStore(
    const ParameterCatalog *catalog,
    const ParameterStore *store);

#endif
