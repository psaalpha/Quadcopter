#include "parameter_persistence.h"

static ParameterPersistenceStatus ParameterPersistence_MapStoreStatus(
    ParameterStatus status)
{
    if (status == PARAMETER_STATUS_OK)
    {
        return PARAMETER_PERSISTENCE_STATUS_OK;
    }
    if (status == PARAMETER_STATUS_BUFFER_TOO_SMALL)
    {
        return PARAMETER_PERSISTENCE_STATUS_BUFFER_TOO_SMALL;
    }
    if (status == PARAMETER_STATUS_STALE_REVISION)
    {
        return PARAMETER_PERSISTENCE_STATUS_STALE_REVISION;
    }
    return PARAMETER_PERSISTENCE_STATUS_IMAGE_INVALID;
}

ParameterPersistenceStatus ParameterPersistence_Init(
    ParameterPersistence *persistence,
    ParameterStore *store,
    const ParameterPersistenceBackend *backend,
    uint8_t *scratch,
    uint16_t scratch_size)
{
    if ((persistence == 0) || (store == 0) || !store->initialized
        || (backend == 0) || (backend->read == 0)
        || (backend->write_atomic == 0) || (scratch == 0)
        || (scratch_size < ParameterStore_EncodedSize(store)))
    {
        return PARAMETER_PERSISTENCE_STATUS_INVALID_ARGUMENT;
    }

    persistence->store = store;
    persistence->backend = *backend;
    persistence->scratch = scratch;
    persistence->scratch_size = scratch_size;
    persistence->initialized = 1u;
    return PARAMETER_PERSISTENCE_STATUS_OK;
}

ParameterPersistenceStatus ParameterPersistence_Load(
    ParameterPersistence *persistence)
{
    uint16_t read_size = 0u;
    ParameterPersistenceStatus status;

    if (persistence == 0)
    {
        return PARAMETER_PERSISTENCE_STATUS_INVALID_ARGUMENT;
    }
    if (!persistence->initialized)
    {
        return PARAMETER_PERSISTENCE_STATUS_NOT_INITIALIZED;
    }

    status = persistence->backend.read(
        persistence->backend.context,
        persistence->scratch,
        persistence->scratch_size,
        &read_size);
    if (status != PARAMETER_PERSISTENCE_STATUS_OK)
    {
        return PARAMETER_PERSISTENCE_STATUS_BACKEND_ERROR;
    }

    return ParameterPersistence_MapStoreStatus(
        ParameterStore_Decode(
            persistence->store,
            persistence->scratch,
            read_size));
}

ParameterPersistenceStatus ParameterPersistence_Save(
    ParameterPersistence *persistence)
{
    uint16_t written_size = 0u;
    uint32_t saved_revision;
    ParameterStatus store_status;
    ParameterPersistenceStatus backend_status;

    if (persistence == 0)
    {
        return PARAMETER_PERSISTENCE_STATUS_INVALID_ARGUMENT;
    }
    if (!persistence->initialized)
    {
        return PARAMETER_PERSISTENCE_STATUS_NOT_INITIALIZED;
    }

    saved_revision = ParameterStore_GetRevision(persistence->store);
    store_status = ParameterStore_Encode(
        persistence->store,
        persistence->scratch,
        persistence->scratch_size,
        &written_size);
    if (store_status != PARAMETER_STATUS_OK)
    {
        return ParameterPersistence_MapStoreStatus(store_status);
    }

    backend_status = persistence->backend.write_atomic(
        persistence->backend.context,
        persistence->scratch,
        written_size);
    if (backend_status != PARAMETER_PERSISTENCE_STATUS_OK)
    {
        return PARAMETER_PERSISTENCE_STATUS_BACKEND_ERROR;
    }

    return ParameterPersistence_MapStoreStatus(
        ParameterStore_MarkPersisted(
            persistence->store,
            saved_revision));
}
