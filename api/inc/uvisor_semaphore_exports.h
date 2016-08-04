#ifndef UVISOR_SEMAPHORE_H
#define UVISOR_SEMAPHORE_H

#include "api/inc/uvisor_exports.h"

#define UVISOR_SEMAPHORE_INTERNAL_SIZE (16)

/* An opaque structure, that one knows the size of so they can allocate memory
 * for them. */
typedef struct uvisor_semaphore {
    uint8_t internal[UVISOR_SEMAPHORE_INTERNAL_SIZE];
} uvisor_semaphore_t;

UVISOR_EXTERN int uvisor_semaphore_init(uvisor_semaphore_t * semaphore, int32_t count);

/* This function is not safe to call from interrupt context, even if the
 * timeout is zero. */
UVISOR_EXTERN int uvisor_semaphore_pend(uvisor_semaphore_t * semaphore, uint32_t timeout_ms);

/* This function is safe to call from interrupt context. */
UVISOR_EXTERN int uvisor_semaphore_post(uvisor_semaphore_t * semaphore);

#endif
