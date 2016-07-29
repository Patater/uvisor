#include "api/inc/uvisor_mutex_exports.h"
#include "api/inc/uvisor_exports.h"
#include "cmsis_os.h"
#include <string.h>

typedef struct uvisor_mutex_internal {
    osMutexId id;
    osMutexDef_t def;
    int32_t data[4];
} uvisor_mutex_internal_t;

UVISOR_STATIC_ASSERT(UVISOR_MUTEX_INTERNAL_SIZE >= sizeof(uvisor_mutex_internal_t), mutex_size_too_small);

int uvisor_mutex_init(uvisor_mutex_t * m)
{
    uvisor_mutex_internal_t * mutex = (uvisor_mutex_internal_t *) m;

    memset(mutex->data, 0, sizeof(mutex->data));
    mutex->def.mutex = mutex->data;
    mutex->id = osMutexCreate(&mutex->def);

    /* Error when mutex->id is NULL */
    return -(mutex->id == NULL);
}

int uvisor_mutex_acquire(uvisor_mutex_t * m, uint32_t timeout_ms)
{
    uvisor_mutex_internal_t * mutex = (uvisor_mutex_internal_t *) m;

    osStatus status = osMutexWait(mutex->id, timeout_ms);

    /* If status was not OK, error. */
    return -(status != osOK);
}

int uvisor_mutex_release(uvisor_mutex_t * m) {
    uvisor_mutex_internal_t * mutex = (uvisor_mutex_internal_t *) m;
    osStatus status = osMutexRelease(mutex->id);

    /* If status was not OK, error. */
    return -(status != osOK);
}
