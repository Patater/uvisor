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
#include "api/inc/pool_queue_exports.h"
#include "api/inc/rpc_exports.h"
#include "api/inc/svc_exports.h"
#include "api/inc/uvisor_semaphore_exports.h"
#include "api/inc/vmpu_exports.h"
#include "context.h"
#include "halt.h"

/* By default a maximum of 16 threads are allowed. This can only be overridden
 * by the porting engineer for the current platform. */
#ifndef UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT
#define UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT ((uint32_t) 16)
#endif

/* Per thread we store the pointer to the allocator and the process id that
 * this thread belongs to. */
typedef struct {
    void * allocator;
    int process_id;
} UvisorThreadContext;

static UvisorThreadContext thread[UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT];


static int thread_ctx_valid(UvisorThreadContext * context)
{
    /* Check if context pointer points into the array. */
    if ((uint32_t) context < (uint32_t) &thread ||
        ((uint32_t) &thread + sizeof(thread)) <= (uint32_t) context) {
        return 0;
    }
    /* Check if the context is aligned exactly to a context. */
    return (((uint32_t) context - (uint32_t) thread) % sizeof(UvisorThreadContext)) == 0;
}

static void * thread_create(int id, void * c)
{
    (void) id;
    UvisorThreadContext * context = c;
    const UvisorBoxIndex * const index =
            (UvisorBoxIndex * const) *(__uvisor_config.uvisor_box_context);
    /* Search for a free slot in the tasks meta data. */
    uint32_t ii = 0;
    for (; ii < UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT; ii++) {
        if (thread[ii].allocator == NULL) {
            break;
        }
    }
    if (ii < UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT) {
        /* Remember the process id for this thread. */
        thread[ii].process_id = g_active_box;
        /* Fall back to the process heap if ctx is NULL. */
        thread[ii].allocator = context ? context : index->box_heap;
        return &thread[ii];
    }
    return context;
}

static void thread_destroy(void * c)
{
    UvisorThreadContext * context = c;
    if (context == NULL) {
        return;
    }

    /* Only if TID is valid and destruction status is zero. */
    if (thread_ctx_valid(context) && context->allocator && (context->process_id == g_active_box)) {
        /* Release this slot. */
        context->allocator = NULL;
    } else {
        HALT_ERROR(SANITY_CHECK_FAILED,
            "thread context (%08x) is invalid!\n",
            context);
    }
}

static uvisor_pool_t * fn_group_pool(void)
{
    UvisorBoxIndex * index = (UvisorBoxIndex *) *(__uvisor_config.uvisor_box_context);
    return index->rpc_fn_group_pool;
}

static uvisor_rpc_fn_group_t * fn_group_array(void)
{
    return (uvisor_rpc_fn_group_t *) fn_group_pool()->array;
}

/* Wake up all the potential handlers for this RPC target. */
static void wake_up_handlers_for_target(const TFN_Ptr function)
{
    /* TODO Use unpriv reads and writes */

    /* Wake up all known waiters for this function. Search for the function in
     * all known function groups. We have to search through all function groups
     * (not just those currently waiting for messages) because we want the RTOS
     * to be able to pick the highest priority waiter to schedule to run. Some
     * waiters will wake up and find they have nothing to do if a higher
     * priority waiter already took care of handling the incoming RPC. */
    uvisor_pool_slot_t i;
    for (i = 0; i < fn_group_pool()->num; i++) {
        /* If the entry in the pool is allocated: */
        if (fn_group_pool()->management_array[i].dequeued.state != UVISOR_POOL_SLOT_IS_FREE) {
            /* Look for the function in this function group. */
            uvisor_rpc_fn_group_t * fn_group = &fn_group_array()[i];
            TFN_Ptr const * fn_ptr_array = fn_group->fn_ptr_array;
            uvisor_pool_slot_t j;

            for (j = 0; j < fn_group->fn_count; j++) {
                /* If function is found: */
                if (fn_ptr_array[j] == function) {
                    /* Wake up the waiter. */
                    uvisor_semaphore_post(&fn_group->semaphore);
                }
            }
        }
    }
}

static void drain_message_queue(void)
{
    /* XXX This implementation is dumb and simple and slow and not secure. */

    UvisorBoxIndex * index = (UvisorBoxIndex *) *__uvisor_config.uvisor_box_context;
    uvisor_pool_queue_t * source_queue = index->rpc_outgoing_message_queue;
    uvisor_rpc_message_t * source_array = (uvisor_rpc_message_t *) source_queue->pool.array;
    int source_box = g_active_box;

    /* For each message in the queue: */
    do {
        uvisor_rpc_message_t uvisor_msg;
        uvisor_pool_slot_t source_slot;

        /* NOTE: We only dequeue the message from the queue. We don't free
         * the message from the pool. The caller will free the message from the
         * pool after finish waiting for the RPC to finish. */
        source_slot = uvisor_pool_queue_dequeue_first(source_queue);
        if (source_slot >= source_queue->pool.num) {
            /* The queue is empty. */
            break;
        }

        uvisor_rpc_message_t * msg = &source_array[source_slot];

        /* Copy the message. FIXME use unpriv copying */
        memcpy(&uvisor_msg, msg, sizeof(uvisor_msg));

        /* Set the ID of the calling box in the message. */
        uvisor_msg.source_box = source_box;

        /* Look up the destination box. */
        /* XXX Assume destination box is 1 for now. :p */
        static const int destination_box = 1;

        /* Switch to the destination box if the thread is in a different
         * process than we are currently in. */
        if (destination_box != source_box) {
            context_switch_in(CONTEXT_SWITCH_UNBOUND_THREAD, destination_box, 0, 0);
        }
        UvisorBoxIndex * dest_index = (UvisorBoxIndex *) *__uvisor_config.uvisor_box_context;
        uvisor_pool_queue_t * dest_queue = dest_index->rpc_incoming_message_queue;
        uvisor_rpc_message_t * dest_array = (uvisor_rpc_message_t *) dest_queue->pool.array;

        /* Place the message into the destination box queue. */
        static const uint32_t timeout_ms = 0; /* Don't wait for space in the destination queue. */
        uvisor_pool_slot_t dest_slot = uvisor_pool_queue_allocate(dest_queue, timeout_ms);

        /* If there is space in the destination queue: */
        if (dest_slot < dest_queue->pool.num)
        {
            uvisor_rpc_message_t * dest_msg = &dest_array[dest_slot];

            /* Copy the message to the destination. FIXME use unpriv copying */
            memcpy(dest_msg, &uvisor_msg, sizeof(*dest_msg));

            /* Enqueue the message */
            uvisor_pool_queue_enqueue(dest_queue, dest_slot);

            /* Poke anybody waiting on calls to this target function. */
            wake_up_handlers_for_target(uvisor_msg.function);
        }

        /* Switch back to the source box if the thread is in a different
         * process than we are currently in. We do this here for two reasons.
         *   1. We may need to put the message back into the source queue. We
         *      should put it back in source box context.
         *   2. We will read the next message in the source queue soon (on next
         *      loop iteration). We should read the next message from source box
         *      context. */
        if (destination_box != source_box) {
            context_switch_in(CONTEXT_SWITCH_UNBOUND_THREAD, source_box, 0, 0);
        }

        /* If there was no room in the destination queue: */
        if (dest_slot >= dest_queue->pool.num)
        {
            /* Put the message back into the source queue. This applies
             * backpressure on the caller when the callee is too busy. Note
             * that no data needs to be copied; only the source queue's
             * management array is modified. */
            uvisor_pool_queue_enqueue(source_queue, source_slot);
        }
    } while (1);
}

static void drain_result_queue(void)
{
    /* XXX This implementation is dumb and simple and slow and not secure. */

    UvisorBoxIndex * index = (UvisorBoxIndex *) *__uvisor_config.uvisor_box_context;
    uvisor_pool_queue_t * source_queue = index->rpc_outgoing_result_queue;
    uvisor_rpc_result_obj_t * source_array = (uvisor_rpc_result_obj_t *) source_queue->pool.array;

    int source_box = g_active_box;

    /* For each message in the queue: */
    do {
        uvisor_rpc_result_obj_t uvisor_result;
        uvisor_pool_slot_t source_slot;

        /* NOTE: We both dequeue and free the message from the queue. The
         * callee (the one sending result messages) doesn't care about the
         * message after they post it to their outgoing result queue. */
        source_slot = uvisor_pool_queue_dequeue_first(source_queue);
        source_slot = uvisor_pool_queue_free(source_queue, source_slot);
        if (source_slot >= source_queue->pool.num) {
            /* The queue is empty. */
            break;
        }

        uvisor_rpc_result_obj_t * result = &source_array[source_slot];

        /* Copy the message. FIXME use unpriv copying */
        memcpy(&uvisor_result, result, sizeof(uvisor_result));

        /* Look up the origin message. This should have been remembered
         * by uVisor when it did the initial delivery. */
        /* XXX For now, trust whatever the RPC callee says... This is not secure.
         * */
        uvisor_pool_slot_t dest_slot = uvisor_result.msg_slot; /* XXX NOT SECURE */

        /* Based on the origin message, look up the destination box. */
        /* XXX Assume destination box is 0 for now. :p */
        static const int destination_box = 0;

        /* Switch to the destination box if the thread is in a different
         * process than we are currently in. */
        if (destination_box != source_box) {
            context_switch_in(CONTEXT_SWITCH_UNBOUND_THREAD, destination_box, 0, 0);
        }
        UvisorBoxIndex * dest_index = (UvisorBoxIndex *) *__uvisor_config.uvisor_box_context;
        uvisor_pool_queue_t * dest_queue = dest_index->rpc_outgoing_message_queue;
        uvisor_rpc_message_t * dest_array = (uvisor_rpc_message_t *) dest_queue->pool.array;

        /* Place the message into the destination box queue. */
        uvisor_rpc_message_t * dest_msg = &dest_array[dest_slot];

        /* Write the result value to the destination. FIXME use unpriv writing */
        dest_msg->result = uvisor_result.value;

        /* Post to the result semaphore */
        uvisor_semaphore_post(&dest_msg->semaphore);

        /* Switch back to the source box if the thread is in a different
         * process than we are currently in. We do this here for one reason.
         *   1. We will read the next message in the source queue soon (on next
         *      loop iteration). We should read the next message from source box
         *      context. */
        if (destination_box != source_box) {
            context_switch_in(CONTEXT_SWITCH_UNBOUND_THREAD, source_box, 0, 0);
        }
    } while (1);
}

static void drain_outgoing_rpc_queues(void)
{
    drain_message_queue();
    drain_result_queue();
}

static void thread_switch(void * c)
{
    UvisorThreadContext * context = c;
    UvisorBoxIndex * index;

    /* Drain any outgoing RPC queues */
    drain_outgoing_rpc_queues();

    if (context == NULL) {
        return;
    }

    /* Only if TID is valid and the slot is used */
    if (!thread_ctx_valid(context) || context->allocator == NULL) {
        HALT_ERROR(SANITY_CHECK_FAILED,
            "thread context (%08x) is invalid!\n",
            context);
        return;
    }
    /* If the thread is inside another process, switch into it. */
    if (context->process_id != g_active_box) {
        context_switch_in(CONTEXT_SWITCH_UNBOUND_THREAD, context->process_id, 0, 0);
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
    .size = sizeof(TUvisorExportTable)
};
