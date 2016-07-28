#include "uvisor.h"
#include "halt.h"
#include "api/inc/uvisor_semaphore_exports.h"

/* uVisor runs in a single interrupt context most of the time (SVC). Semaphores
 * can be posted to from interrupt context, but not pended upon. */

int uvisor_semaphore_init(uvisor_semaphore_t * semaphore, int32_t count)
{
    HALT_ERROR(NOT_ALLOWED, "Semaphores must be initialized from outside of uVisor.");

    return -1;
}

int uvisor_semaphore_pend(uvisor_semaphore_t * semaphore, uint32_t timeout_ms)
{
    HALT_ERROR(NOT_ALLOWED, "Semaphores can't be pended upon from inside uVisor.");
    return -1;
}

int uvisor_semaphore_post(uvisor_semaphore_t * semaphore) {
    return g_priv_sys_hooks.priv_uvisor_semaphore_post(semaphore);
}
