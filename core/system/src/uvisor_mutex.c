#include "uvisor.h"
#include "halt.h"
#include "api/inc/uvisor_mutex_exports.h"

/* uVisor runs in a single interrupt context most of the time (SVC). Mutexes
 * aren't needed because the SVC serializes access. As such, most of these
 * functions are no-ops. */

int uvisor_mutex_init(uvisor_mutex_t * m)
{
    HALT_ERROR(NOT_ALLOWED, "Mutexes must be initialized from outside of uVisor.");

    return -1;
}

int uvisor_mutex_acquire(uvisor_mutex_t * mutex, uint32_t timeout_ms)
{
    return 0;
}

int uvisor_mutex_release(uvisor_mutex_t * mutex)
{
    return 0;
}
