#include "api/inc/uvisor_semaphore_exports.h"
#include "api/inc/uvisor_exports.h"
#include "cmsis_os.h"
#include <string.h>

typedef struct uvisor_semaphore_internal {
    osSemaphoreId id;
    osSemaphoreDef_t def;
    uint32_t data[2];
} uvisor_semaphore_internal_t;

UVISOR_STATIC_ASSERT(UVISOR_SEMAPHORE_INTERNAL_SIZE >= sizeof(uvisor_semaphore_internal_t), semaphore_size_too_small);

int uvisor_semaphore_init(uvisor_semaphore_t * s, int32_t count)
{
    uvisor_semaphore_internal_t * semaphore = (uvisor_semaphore_internal_t *) s;

    memset(semaphore->data, 0, sizeof(semaphore->data));
    semaphore->def.semaphore = semaphore->data;
    semaphore->id = osSemaphoreCreate(&semaphore->def, count);

    s->next = NULL;

    /* Error when semaphore->id is NULL */
    return -(semaphore->id == NULL);
}

void uvisor_semaphore_list_append(uvisor_semaphore_t * semaphore_list, uvisor_semaphore_t * next)
{
    /* Find the last item in the list. */
    while (semaphore_list->next != NULL) {
        semaphore_list = semaphore_list->next;
    }

    /* Add the new semaphore to the end of the list. */
    semaphore_list->next = next;
}

int uvisor_semaphore_pend(uvisor_semaphore_t * s, uint32_t timeout_ms)
{
    uvisor_semaphore_internal_t * semaphore = (uvisor_semaphore_internal_t *) s;

    int32_t num_available_tokens = osSemaphoreWait(semaphore->id, timeout_ms);

    /* If no tokens were available, error. */
    return -(num_available_tokens == 0);
}

int uvisor_semaphore_post(uvisor_semaphore_t * s) {
    uvisor_semaphore_internal_t * semaphore = (uvisor_semaphore_internal_t *) s;
    return osSemaphoreRelease(semaphore->id);
}
