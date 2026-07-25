#ifndef PARAMETER_PERSISTENCE_H
#define PARAMETER_PERSISTENCE_H

#include <stdint.h>

#include "parameter_store.h"

typedef enum
{
    PARAMETER_PERSISTENCE_STATUS_OK = 0,
    PARAMETER_PERSISTENCE_STATUS_INVALID_ARGUMENT,
    PARAMETER_PERSISTENCE_STATUS_NOT_INITIALIZED,
    PARAMETER_PERSISTENCE_STATUS_BACKEND_ERROR,
    PARAMETER_PERSISTENCE_STATUS_IMAGE_INVALID,
    PARAMETER_PERSISTENCE_STATUS_BUFFER_TOO_SMALL,
    PARAMETER_PERSISTENCE_STATUS_STALE_REVISION
} ParameterPersistenceStatus;

typedef ParameterPersistenceStatus (*ParameterPersistenceReadFn)(
    void *context,
    uint8_t *buffer,
    uint16_t capacity,
    uint16_t *read_size);
typedef ParameterPersistenceStatus (*ParameterPersistenceWriteAtomicFn)(
    void *context,
    const uint8_t *buffer,
    uint16_t size);

typedef struct
{
    ParameterPersistenceReadFn read;
    ParameterPersistenceWriteAtomicFn write_atomic;
    void *context;
} ParameterPersistenceBackend;

typedef struct
{
    ParameterStore *store;
    ParameterPersistenceBackend backend;
    uint8_t *scratch;
    uint16_t scratch_size;
    uint8_t initialized;
} ParameterPersistence;

ParameterPersistenceStatus ParameterPersistence_Init(
    ParameterPersistence *persistence,
    ParameterStore *store,
    const ParameterPersistenceBackend *backend,
    uint8_t *scratch,
    uint16_t scratch_size);
ParameterPersistenceStatus ParameterPersistence_Load(
    ParameterPersistence *persistence);
ParameterPersistenceStatus ParameterPersistence_Save(
    ParameterPersistence *persistence);

#endif
