/*
 * Copyright (c) 2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <reent.h>

#include "uvisor-lib/uvisor-lib.h"
#include "cmsis_os.h"

#define DPRINTF(...) {};
/* Use printf with caution inside malloc: printf may allocate memory itself,
   so using printf in malloc may lead to recursive calls! */
/* #define DPRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__) */

extern UvisorBoxIndexOS *const __uvisor_ps;
extern void uvisor_malloc_init(void);

static int is_kernel_initialized()
{
    static uint8_t kernel_running = 0;
    return (kernel_running || (kernel_running = osKernelRunning()));
}

static int init_allocator()
{
    int ret = 0;
    if (__uvisor_ps == NULL) {
#if defined(UVISOR_PRESENT) && (UVISOR_PRESENT == 1)
        return -1;
#else
        uvisor_malloc_init();
#endif
    }

    if ((__uvisor_ps->mutex_id == NULL) && is_kernel_initialized()) {
        /* point the mutex pointer to the data */
        __uvisor_ps->mutex = &(__uvisor_ps->mutex_data);
        /* create mutex if not already done */
        __uvisor_ps->mutex_id = osMutexCreate((osMutexDef_t *) &(__uvisor_ps->mutex));
        /* mutex failed to be created */
        if (__uvisor_ps->mutex_id == NULL) {
            return -1;
        }
    }

    if (__uvisor_ps->index.active_heap == NULL) {
        /* we need to initialize the process heap */
        if (__uvisor_ps->index.process_heap != NULL) {
            /* lock the mutex during initialization */
            int kernel_initialized = is_kernel_initialized();
            if (kernel_initialized) {
                osMutexWait(__uvisor_ps->mutex_id, osWaitForever);
            }
            /* initialize the process heap */
            UvisorAllocator allocator = uvisor_allocator_create_with_pool(
                __uvisor_ps->index.process_heap,
                __uvisor_ps->index.process_heap_size);
            /* set the allocator */
            ret = allocator ? 0 : -1;
            __uvisor_ps->index.active_heap = allocator;
            /* release the mutex */
            if (kernel_initialized) {
                osMutexRelease(__uvisor_ps->mutex_id);
            }
        }
        else {
            DPRINTF("uvisor_allocator: No process heap available!\n");
            ret = -1;
        }
    }
    return ret;
}

/* public API */
UvisorAllocator uvisor_get_allocator(void)
{
    if (init_allocator()) {
        return NULL;
    }
    return __uvisor_ps->index.active_heap;
}
