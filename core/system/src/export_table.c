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
#include <uvisor.h>
#include "api/inc/export_table_exports.h"
#include "api/inc/svc_exports.h"
#include "api/inc/vmpu_exports.h"
#include "context.h"
#include "halt.h"
#include "vmpu.h"

/* TODO Make this a bitmap to save some RAM. */
static int thread_local_storage_allocation_table[UVISOR_MAX_BOXES][UVISOR_MAX_THREADS_PER_BOX];

static UvisorThreadContext * thread_local_storage(void)
{
    const UvisorBoxIndex * const index =
            (UvisorBoxIndex * const) *(__uvisor_config.uvisor_box_context);
    return (UvisorThreadContext *) index->thread_local_storage;
}

/* Search through the list of boxes to find the context. Return -1 if not
 * found. */
static int box_id_for_context(UvisorThreadContext * context)
{
    int box_id;
    for (box_id = 0; box_id < g_vmpu_box_count; box_id++) {
        UvisorBoxIndex * box_index = (UvisorBoxIndex *) g_context_current_states[box_id].bss;
        uint32_t context_start = (uint32_t) context;
        uint32_t context_end = context_start + sizeof(*context);
        uint32_t thread_local_storage_start = (uint32_t) box_index->thread_local_storage;
        uint32_t thread_local_storage_end = thread_local_storage_start + sizeof(*box_index->thread_local_storage);
        if (context_start >= thread_local_storage_start && context_end <= thread_local_storage_end) {
            /* We found the box that this context belongs to. */
            return box_id;
        }
    }

    /* We didn't find the context in any boxes. */
    return -1;
}

static int thread_ctx_valid(int box_id, UvisorThreadContext * context)
{
    UvisorBoxIndex * box_index = (UvisorBoxIndex *) g_context_current_states[box_id].bss;
    UvisorThreadContext * storage = (UvisorThreadContext *) box_index->thread_local_storage;
    /* Check if context pointer points into the array. */
    int within_array = context >= storage && context < storage + UVISOR_MAX_THREADS_PER_BOX;

    /* Check if the context is aligned exactly to a context. */
    int aligned = (((uintptr_t) context - (uintptr_t) storage) % sizeof(*storage)) == 0;

    return within_array && aligned;
}

static UvisorThreadContext * allocate_thread_local_storage(void)
{
    size_t i;

    /* Search for a free slot in the box's thread local storage array. */
    for (i = 0; i < UVISOR_MAX_THREADS_PER_BOX; i++) {
        if (thread_local_storage_allocation_table[g_active_box][i] == 0) {
            /* We found a free slot. Allocate the thread local storage. */
            thread_local_storage_allocation_table[g_active_box][i] = 1;
            return &thread_local_storage()[i];
        }
    }

    /* We did not find a free slot. There is no space left in the thread local
     * storage array. */
    return NULL;
}

static void free_thread_local_storage(UvisorThreadContext * context)
{
    size_t i = context - thread_local_storage();
    thread_local_storage_allocation_table[g_active_box][i] = 1;
}

static void * thread_create(int id, void * allocator)
{
    const UvisorBoxIndex * const index =
            (UvisorBoxIndex * const) *(__uvisor_config.uvisor_box_context);

    /* Ignore the provided thread ID. */
    (void) id;

    UvisorThreadContext * context = allocate_thread_local_storage();
    if (context) {
        /* Assign the thread allocator. Fall back to the process heap if
         * user-provided allocator is NULL. */
        context->allocator = allocator ? allocator : index->box_heap;

        return context;
    }

    return allocator;
}

static void thread_destroy(void * c)
{
    UvisorThreadContext * context = c;
    if (context == NULL) {
        return;
    }

    /* Only if TID is valid and destruction status is zero. */
    if (thread_ctx_valid(g_active_box, context) && context->allocator && (box_id_for_context(context) == g_active_box)) {
        free_thread_local_storage(context);
    } else {
        HALT_ERROR(SANITY_CHECK_FAILED,
            "thread context (%08x) is invalid!\n",
            context);
    }
}

static void thread_switch(void * c)
{
    UvisorThreadContext * context = c;
    UvisorBoxIndex * index;
    if (context == NULL) {
        return;
    }

    int box_id = box_id_for_context(context);

    /* Only if TID is valid and the slot is used */
    if (!thread_ctx_valid(box_id, context) || box_id == -1) {
        HALT_ERROR(SANITY_CHECK_FAILED,
            "thread context (%08x) is invalid!\n",
            context);
        return;
    }
    /* If the thread is inside another process, switch into it. */
    if (box_id != g_active_box) {
        context_switch_in(CONTEXT_SWITCH_UNBOUND_THREAD, box_id, 0, 0);
    }
    /* Copy the thread allocator into the (new) box index. */
    /* Note: The value in index is updated by context_switch_in, or is already
     *       the correct one if no switch needs to occur. */
    index = (UvisorBoxIndex *) *(__uvisor_config.uvisor_box_context);
    if (context->allocator) {
        /* If the active_heap is NULL, then the process heap needs to be
         * initialized yet. The initializer sets the active heap itself. */
        if (index->active_heap) {
            index->active_heap = context->allocator;
        }
    }
}

static void boxes_init(void)
{
    /* Tell uVisor to call the uVisor lib box_init function for each box with
     * each box's uVisor lib config. */

    /* This must be called from unprivileged mode in order for the recursive
     * gateway chaining to work properly. */
    UVISOR_SVC(UVISOR_SVC_ID_BOX_INIT_FIRST, "");
}

/* This table must be located at the end of the uVisor binary so that this
 * table can be exported correctly. Placing this table into the .export_table
 * section locates this table at the end of the uVisor binary. */
__attribute__((section(".export_table")))
const TUvisorExportTable __uvisor_export_table = {
    .magic = UVISOR_EXPORT_MAGIC,
    .version = UVISOR_EXPORT_VERSION,
    .os_event_observer = {
        .version = 0,
        .pre_start = boxes_init,
        .thread_create = thread_create,
        .thread_destroy = thread_destroy,
        .thread_switch = thread_switch,
    },
    .pool_queue = {
        .pool_init = uvisor_pool_init,
        .pool_queue_init = uvisor_pool_queue_init,
        .pool_allocate = uvisor_pool_allocate,
        .pool_queue_enqueue = uvisor_pool_queue_enqueue,
        .pool_free = uvisor_pool_free,
        .pool_queue_dequeue = uvisor_pool_queue_dequeue,
        .pool_queue_dequeue_first = uvisor_pool_queue_dequeue_first,
        .pool_queue_find_first = uvisor_pool_queue_find_first,
    },
    .size = sizeof(TUvisorExportTable)
};
