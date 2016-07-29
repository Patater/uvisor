#ifndef UVISOR_MUTEX_H
#define UVISOR_MUTEX_H

#include "api/inc/uvisor_exports.h"

#define UVISOR_MUTEX_INTERNAL_SIZE (24)

/* An opaque structure, that one knows the size of so they can allocate memory
 * for them. */
typedef struct uvisor_mutex {
    uint8_t internal[UVISOR_MUTEX_INTERNAL_SIZE];
} uvisor_mutex_t;

UVISOR_EXTERN int uvisor_mutex_init(uvisor_mutex_t * mutex);

UVISOR_EXTERN void uvisor_mutex_list_append(uvisor_mutex_t * mutex_list, uvisor_mutex_t * next);

/* This function is not safe to call from interrupt context. */
UVISOR_EXTERN int uvisor_mutex_acquire(uvisor_mutex_t * mutex, uint32_t timeout_ms);

/* This function is not safe to call from interrupt context. */
UVISOR_EXTERN int uvisor_mutex_release(uvisor_mutex_t * mutex);

#endif

